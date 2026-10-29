# =====================================================================
#  tools/idf.ps1 —— ESP-IDF 构建入口（Windows PowerShell 5.1+）
#
#  作用：自动定位本机的 ESP-IDF，激活环境后调用 idf.py。
#        不需要事先打开 "ESP-IDF PowerShell" 或手动 dot-source export.ps1。
#
#  用法（在任意目录都可以执行）：
#    .\tools\idf.ps1 build                  编译
#    .\tools\idf.ps1 flash                  烧录（自动选择串口）
#    .\tools\idf.ps1 -p COM3 flash monitor  指定串口烧录并查看日志
#    .\tools\idf.ps1 menuconfig             图形化配置
#    .\tools\idf.ps1                        显示帮助
#
#  ESP-IDF 查找顺序（取第一个可用的）：
#    1. 环境变量 IDF_PATH
#    2. %IDF_TOOLS_PATH%\frameworks\esp-idf-*       （官方安装器，默认 C:\Espressif）
#    3. %USERPROFILE%\.espressif\frameworks\esp-idf-*（VS Code 插件常见位置）
#    4. %USERPROFILE%\esp\esp-idf、C:\esp\esp-idf   （git 克隆常见位置）
#
#  可用环境变量覆盖：
#    IDF_PATH        ESP-IDF 根目录（优先级最高）
#    IDF_TOOLS_PATH  工具链根目录（默认自动推断）
# =====================================================================

# 不使用 param 块：让 idf.py 自己的参数（如 -p COM3）原样透传。
# 注意：这里不要设 $ErrorActionPreference='Stop'，否则 export.ps1 里
# python 往 stderr 打的进度信息（PS5 会当成 NativeCommandError）会中断脚本。
$IdfArgs = $args

# VS Code 任务用 ${config:idf.port} 传串口。把 settings.json 里的 idf.port
# 清空时可能收到 "-p" 后面没有值、或直接跟着动作名的情况，这里做规范化：
#   - 端口为空 -> 丢掉该 -p，让 idf.py 恢复自动扫描
#   - 空字符串参数 -> 丢掉，避免 idf.py 收到空的位置参数
$IdfActions = @('all', 'app', 'bootloader', 'build', 'clean', 'confserver',
                'erase-flash', 'erase-otadata', 'flash', 'fullclean', 'gdb',
                'gdbgui', 'ide', 'lint', 'menuconfig', 'merge-bin', 'monitor',
                'openocd', 'partition-table', 'reconfigure', 'set-target',
                'size', 'size-components', 'size-files', 'uf2')
$cleanArgs = New-Object System.Collections.ArrayList
for ($i = 0; $i -lt $IdfArgs.Count; $i++) {
    $arg = [string]$IdfArgs[$i]
    if ([string]::IsNullOrWhiteSpace($arg)) { continue }

    if ($arg -eq '-p' -or $arg -eq '--port') {
        $next = ''
        if ($i + 1 -lt $IdfArgs.Count) { $next = [string]$IdfArgs[$i + 1] }
        if ([string]::IsNullOrWhiteSpace($next) -or ($IdfActions -contains $next)) {
            continue   # 端口没填：丢掉 -p，后面的动作照常处理
        }
        [void]$cleanArgs.Add($arg)
        [void]$cleanArgs.Add($next)
        $i++
        continue
    }

    [void]$cleanArgs.Add($arg)
}
$IdfArgs = $cleanArgs.ToArray()

# 命令行没写 -p 时，复用 .vscode/settings.json 里的 idf.port（与 VS Code
# 任务共用同一个配置），省去 idf.py 每次扫描全部串口；显式 -p 优先。
$HasPortArg = $false
foreach ($a in $IdfArgs) {
    if ($a -eq '-p' -or $a -eq '--port') { $HasPortArg = $true; break }
}
$UsesSerial = $false
foreach ($a in $IdfArgs) {
    if ($a -in @('flash', 'monitor', 'erase-flash', 'erase-otadata', 'encrypted-flash', 'uf2')) {
        $UsesSerial = $true; break
    }
}
if (-not $HasPortArg -and $UsesSerial) {
    $settingsFile = Join-Path (Split-Path $PSScriptRoot -Parent) '.vscode\settings.json'
    if (Test-Path $settingsFile) {
        $portMatch = Select-String -Path $settingsFile -Pattern '"idf\.port"\s*:\s*"([^"]+)"' -ErrorAction SilentlyContinue |
                     Select-Object -First 1
        if ($portMatch) {
            $configuredPort = $portMatch.Matches[0].Groups[1].Value
            if (-not [string]::IsNullOrWhiteSpace($configuredPort)) {
                $IdfArgs = @('-p', $configuredPort) + $IdfArgs
                Write-Host "[idf.ps1] 串口: $configuredPort（来自 .vscode/settings.json 的 idf.port）" -ForegroundColor DarkGray
            }
        }
    }
}

# 从 Git Bash / MSYS 环境启动时，MSYSTEM 会让 IDF 的 activate.py 拒绝运行；
# 官方也不支持在 MSYS 里编译，这里清掉后再激活（普通 PowerShell 下无影响）。
if ($env:MSYSTEM) { Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue }

function Show-Usage {
    Write-Host ''
    Write-Host 'ESP32-S3 Demo - ESP-IDF 构建入口' -ForegroundColor Cyan
    Write-Host ''
    Write-Host '用法:'
    Write-Host '  .\tools\idf.ps1 build                    编译'
    Write-Host '  .\tools\idf.ps1 flash                    烧录（自动选择串口）'
    Write-Host '  .\tools\idf.ps1 -p COM3 flash monitor    指定串口烧录并查看日志'
    Write-Host '  .\tools\idf.ps1 menuconfig               图形化配置'
    Write-Host '  .\tools\idf.ps1 fullclean                清理构建目录'
    Write-Host ''
    Write-Host '参数会原样传给 idf.py，例如: .\tools\idf.ps1 -p COM3 build flash monitor'
    Write-Host '查看全部 idf.py 命令: .\tools\idf.ps1 --help'
    Write-Host ''
}

# 从 esp-idf-v5.5.5 这类目录名里取出可用于排序的版本号
function Get-VersionFromDirName([string]$Name) {
    if ($Name -match 'v?(\d+)\.(\d+)(?:\.(\d+))?') {
        $major = [int]$Matches[1]
        $minor = [int]$Matches[2]
        if ($Matches[3]) { $patch = [int]$Matches[3] } else { $patch = 0 }
        return [version]"$major.$minor.$patch"
    }
    return [version]"0.0.0"
}

function Find-IdfPath {
    $candidates = @()

    # 1. 用户显式指定的 IDF_PATH
    if ($env:IDF_PATH) { $candidates += $env:IDF_PATH }

    # 2. 官方安装器 / VS Code 插件的 frameworks 目录（同名多版本时取最高版本）
    $sdkRoots = @()
    if ($env:IDF_TOOLS_PATH) { $sdkRoots += $env:IDF_TOOLS_PATH }
    $sdkRoots += (Join-Path $env:USERPROFILE '.espressif')
    $sdkRoots += 'C:\Espressif'

    foreach ($root in $sdkRoots) {
        if (-not (Test-Path $root)) { continue }
        $fwDir = Join-Path $root 'frameworks'
        if (-not (Test-Path $fwDir)) { continue }

        $dirs = @(Get-ChildItem -Path $fwDir -Directory -Filter 'esp-idf-*' -ErrorAction SilentlyContinue)
        $dirs = @($dirs | Sort-Object -Property @{ Expression = { Get-VersionFromDirName $_.Name } } -Descending)
        foreach ($dir in $dirs) { $candidates += $dir.FullName }
    }

    # 3. git 克隆的常见位置
    $candidates += (Join-Path $env:USERPROFILE 'esp\esp-idf')
    $candidates += 'C:\esp\esp-idf'

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path (Join-Path $candidate 'export.ps1'))) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

# IDF 安装器通常把 tools/ 和 python_env/ 放在 IDF 的上一级（C:\Espressif）；
# git 克隆方式则放在 %USERPROFILE%\.espressif。
function Find-IdfToolsPath([string]$IdfPath) {
    if ($env:IDF_TOOLS_PATH -and (Test-Path $env:IDF_TOOLS_PATH)) { return $env:IDF_TOOLS_PATH }

    $frameworkDir = Split-Path $IdfPath -Parent
    if ((Split-Path $frameworkDir -Leaf) -eq 'frameworks') {
        $root = Split-Path $frameworkDir -Parent
        if (Test-Path (Join-Path $root 'tools')) { return $root }
    }

    foreach ($root in @((Join-Path $env:USERPROFILE '.espressif'), 'C:\Espressif')) {
        if (Test-Path (Join-Path $root 'tools')) { return $root }
    }
    return $null
}

# 系统 Python 版本较新时，export.ps1 可能推断出不存在的虚拟环境；
# 这里优先复用 IDF 安装器已经装好的 python_env。
function Find-PythonEnv([string]$IdfToolsPath, [string]$IdfPath) {
    if ($env:IDF_PYTHON_ENV_PATH -and (Test-Path $env:IDF_PYTHON_ENV_PATH)) {
        return $env:IDF_PYTHON_ENV_PATH
    }
    if (-not $IdfToolsPath) { return $null }

    $envDir = Join-Path $IdfToolsPath 'python_env'
    if (-not (Test-Path $envDir)) { return $null }

    $filter = 'idf*_py3.*_env'
    if ((Split-Path $IdfPath -Leaf) -match 'v?(\d+\.\d+)') {
        $filter = "idf$($Matches[1])_py3.*_env"
    }

    $candidate = Get-ChildItem -Path $envDir -Directory -Filter $filter -ErrorAction SilentlyContinue |
                 Sort-Object Name -Descending |
                 Select-Object -First 1

    if (-not $candidate) {
        $candidate = Get-ChildItem -Path $envDir -Directory -Filter 'idf*_py3.*_env' -ErrorAction SilentlyContinue |
                     Sort-Object Name -Descending |
                     Select-Object -First 1
    }

    if ($candidate) { return $candidate.FullName }
    return $null
}

if (-not $IdfArgs -or $IdfArgs.Count -eq 0) {
    Show-Usage
    exit 0
}

# ---- 1. 定位 ESP-IDF ----
$idfPath = Find-IdfPath
if (-not $idfPath) {
    Write-Host ''
    Write-Host '[错误] 没有找到 ESP-IDF。' -ForegroundColor Red
    Write-Host '  已查找：IDF_PATH、%IDF_TOOLS_PATH%\frameworks、'
    Write-Host '          %USERPROFILE%\.espressif\frameworks、%USERPROFILE%\esp\esp-idf、C:\esp\esp-idf'
    Write-Host ''
    Write-Host '  请先安装 ESP-IDF（推荐官方离线安装器），或手动指定：'
    Write-Host '    $env:IDF_PATH = ''C:\Espressif\frameworks\esp-idf-v5.5.5'''
    Write-Host '  详细步骤见 README.md 的“环境准备”一节。'
    Write-Host ''
    exit 1
}

$idfToolsPath = Find-IdfToolsPath $idfPath
if ($idfToolsPath -and -not $env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH = $idfToolsPath }

$pythonEnv = Find-PythonEnv $idfToolsPath $idfPath
if ($pythonEnv -and -not $env:IDF_PYTHON_ENV_PATH) { $env:IDF_PYTHON_ENV_PATH = $pythonEnv }

# export.ps1 会用 PATH 里的 python 执行 activate.py。
# 把 IDF 自带的虚拟环境放到 PATH 最前面，即使系统没装 Python 也能激活。
if ($pythonEnv) {
    $pythonScripts = Join-Path $pythonEnv 'Scripts'
    if (Test-Path (Join-Path $pythonScripts 'python.exe')) {
        $env:PATH = "$pythonScripts;$env:PATH"
    }
}

Write-Host "[idf.ps1] ESP-IDF: $idfPath" -ForegroundColor DarkGray

# ---- 2. 激活环境（设置 IDF_PATH、PATH、Python 等）----
$exportPs1 = Join-Path $idfPath 'export.ps1'
try {
    . $exportPs1 *> $null
}
catch {
    Write-Host ''
    Write-Host "[错误] 激活 ESP-IDF 环境失败：$($_.Exception.Message)" -ForegroundColor Red
    Write-Host "  ESP-IDF: $idfPath"
    if ($idfToolsPath) { Write-Host "  IDF_TOOLS_PATH: $idfToolsPath" }
    if ($pythonEnv)    { Write-Host "  IDF_PYTHON_ENV_PATH: $pythonEnv" }
    Write-Host '  可尝试重新运行 ESP-IDF 安装器修复环境。'
    Write-Host ''
    exit 1
}

if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    Write-Host ''
    Write-Host '[错误] 激活环境后仍找不到 idf.py，ESP-IDF 安装可能不完整。' -ForegroundColor Red
    Write-Host "  请检查：$idfPath\tools"
    Write-Host ''
    exit 1
}

# ---- 3. 在工程根目录执行 idf.py ----
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    & idf.py @IdfArgs
    $exitCode = $LASTEXITCODE
}
finally {
    Pop-Location
}

exit $exitCode
