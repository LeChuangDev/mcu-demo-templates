# =====================================================================
#  tools/update-clangd.ps1 —— 按本机工具链刷新 .clangd 的系统头文件路径
#
#  clangd 不是交叉编译器，不会自己去找 xtensa 工具链的标准库头文件，
#  所以 .clangd 里需要显式写出 3 个 -isystem 目录。换电脑或升级
#  ESP-IDF 后，运行本脚本即可自动更新，不用手动查路径。
#
#  原理：从 build/compile_commands.json 取出实际使用的编译器，
#        执行 `gcc -E -v -xc NUL`，解析 "#include <...> search starts here"。
#
#  用法（需要先编译一次生成编译数据库）：
#    .\tools\update-clangd.ps1
#    .\tools\update-clangd.ps1 -Compiler "C:\...\xtensa-esp32s3-elf-gcc.exe"
# =====================================================================
param(
    [string]$Compiler = ''
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path $PSScriptRoot -Parent
$clangdPath  = Join-Path $projectRoot '.clangd'
$dbPath      = Join-Path $projectRoot 'build\compile_commands.json'

if (-not (Test-Path $clangdPath)) {
    throw "找不到 .clangd：$clangdPath"
}

# ---- 1. 确定编译器 ----
if (-not $Compiler) {
    if (-not (Test-Path $dbPath)) {
        throw "找不到 build\compile_commands.json。请先编译一次：.\tools\idf.ps1 build"
    }

    $db = Get-Content -Path $dbPath -Raw | ConvertFrom-Json
    $entry = $db | Where-Object { $_.file -like '*main.c' } | Select-Object -First 1
    if (-not $entry) { $entry = $db | Select-Object -First 1 }
    if (-not $entry) { throw "编译数据库为空：$dbPath" }

    if ($entry.arguments -and $entry.arguments.Count -gt 0) {
        $Compiler = $entry.arguments[0]
    }
    else {
        $cmd = [string]$entry.command
        if ($cmd.StartsWith('"')) {
            $end = $cmd.IndexOf('"', 1)
            if ($end -lt 0) { throw "无法解析编译器路径：$cmd" }
            $Compiler = $cmd.Substring(1, $end - 1)
        }
        else {
            $Compiler = ($cmd -split '\s+')[0]
        }
    }
}

if (-not (Test-Path $Compiler)) {
    throw "找不到编译器：$Compiler"
}

# ---- 2. 读取工具链的系统头文件搜索路径 ----
$nullDevice = if ($env:OS -eq 'Windows_NT') { 'NUL' } else { '/dev/null' }
Write-Host "[update-clangd] 编译器: $Compiler" -ForegroundColor DarkGray

# GCC 的 -v 信息写到 stderr；PS5.1 在 ErrorActionPreference=Stop 时会把它
# 当成终止错误，因此调用期间临时改成 Continue。
$oldEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$outputLines = @(& $Compiler -E -v -xc $nullDevice 2>&1 | ForEach-Object { "$_" })
$ErrorActionPreference = $oldEap

$dirs = @()
$inList = $false
foreach ($line in $outputLines) {
    if ($line -match '^#include <\.\.\.> search starts here:') {
        $inList = $true
        continue
    }
    if ($inList) {
        if ($line -match '^End of search list\.') { break }
        $trimmed = $line.Trim()
        if ($trimmed) {
            # gcc 会输出 bin/../lib/... 这类路径，规范化后写进 .clangd 更清晰
            $dirs += [System.IO.Path]::GetFullPath($trimmed).Replace('\', '/')
        }
    }
}

if ($dirs.Count -eq 0) {
    throw "没有从编译器输出中解析到头文件路径。可手动运行：`n  $Compiler -E -v -xc $nullDevice"
}

# ---- 3. 改写 .clangd（保留注释、缩进、换行风格和 BOM）----
$bytes  = [System.IO.File]::ReadAllBytes($clangdPath)
$hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
$content = [System.IO.File]::ReadAllText($clangdPath, [System.Text.Encoding]::UTF8)

$newline = if ($content.Contains("`r`n")) { "`r`n" } else { "`n" }
$lines   = $content -split "`r?`n"

$indices = @()
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\s*- -isystem') { $indices += $i }
}

$indent = '    '
if ($indices.Count -gt 0) {
    $indent = ($lines[$indices[0]] -replace '^(\s*).*$', '$1')
}
$newLines = @($dirs | ForEach-Object { "$indent- -isystem$_" })

$result = @()
if ($indices.Count -gt 0) {
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($indices -contains $i) {
            if ($i -eq $indices[0]) { $result += $newLines }
        }
        else {
            $result += $lines[$i]
        }
    }
}
else {
    # 兼容没有 -isystem 行的旧配置：插到 --target 行之后
    $inserted = $false
    foreach ($line in $lines) {
        $result += $line
        if (-not $inserted -and $line -match '^\s*- --target=') {
            $result += $newLines
            $inserted = $true
        }
    }
    if (-not $inserted) {
        throw ".clangd 里既没有 -isystem 行，也没有 --target 行，请手动检查：$clangdPath"
    }
}

$utf8 = New-Object System.Text.UTF8Encoding($hasBom)
[System.IO.File]::WriteAllText($clangdPath, ($result -join $newline), $utf8)

Write-Host "[update-clangd] 已写入 $clangdPath" -ForegroundColor Green
foreach ($d in $dirs) { Write-Host "  -isystem$d" }
