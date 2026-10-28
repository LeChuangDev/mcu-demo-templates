# =====================================================================
#  tools/idf.ps1 —— ESP-IDF 构建入口包装
#
#  作用：先激活 ESP-IDF 环境，再调用 idf.py，因此在 IDE 任务或普通
#        终端里直接执行本脚本即可，不需要事先手动 dot-source export.ps1。
#
#  用法：
#    .\tools\idf.ps1 build                  构建
#    .\tools\idf.ps1 -p COM3 flash monitor  指定串口烧录并看日志
#    .\tools\idf.ps1 menuconfig             图形化配置
#
#  可用环境变量覆盖默认值：
#    IDF_TOOLS_PATH  IDF 安装根目录（默认 C:\Espressif）
#    IDF_VER_DIR     IDF 版本目录名（默认 v5.5.5）
# =====================================================================
# 不使用 param 块：让 idf.py 自己的参数（如 -p COM3）原样透传。
# 注意：这里不要设 $ErrorActionPreference='Stop'，否则 export.ps1 里
# python 往 stderr 打的进度信息（PS5 会当成 NativeCommandError）会中断脚本。
$IdfArgs = $args

# ---- 1. 定位 ESP-IDF 安装目录与激活脚本 ----
$idfTools   = if ($env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH } else { 'C:\Espressif' }
$idfVersion = if ($env:IDF_VER_DIR) { $env:IDF_VER_DIR } else { 'v5.5.5' }
$exportPs1  = Join-Path $idfTools "frameworks\esp-idf-$idfVersion\export.ps1"

if (-not (Test-Path $exportPs1)) {
    throw "找不到 ESP-IDF 激活脚本：$exportPs1（可用 IDF_TOOLS_PATH / IDF_VER_DIR 指定）"
}

# ---- 2. 指定 Python 虚拟环境 ----
# 系统 Python 版本较新时，export.ps1 会推断出不存在的 idfX.Y_py3.ZZ_env，
# 导致激活失败；这里直接复用 IDF 安装器已经装好的环境。
if (-not $env:IDF_PYTHON_ENV_PATH) {
    $envDir = Join-Path $idfTools 'python_env'
    if (Test-Path $envDir) {
        $candidate = Get-ChildItem $envDir -Directory -Filter 'idf5.*_py3.*_env' |
                     Sort-Object Name -Descending |
                     Select-Object -First 1
        if ($candidate) { $env:IDF_PYTHON_ENV_PATH = $candidate.FullName }
    }
}

# ---- 3. 激活环境（设置 IDF_PATH、PATH、Python 等）----
. $exportPs1 *> $null

# ---- 4. 在工程根目录执行 idf.py ----
if (-not $IdfArgs -or $IdfArgs.Count -eq 0) { $IdfArgs = @('--help') }

Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    & idf.py @IdfArgs
    $exitCode = $LASTEXITCODE
}
finally {
    Pop-Location
}

exit $exitCode
