#!/usr/bin/env sh
# =====================================================================
#  tools/idf.sh —— ESP-IDF 构建入口（Linux / macOS）
#  注意：Windows 请在 PowerShell 里使用 tools/idf.ps1，
#        ESP-IDF 不支持在 Git Bash / MSYS 环境中编译。
#
#  用法：
#    ./tools/idf.sh build
#    ./tools/idf.sh -p /dev/ttyUSB0 flash monitor
#    ./tools/idf.sh menuconfig
#    ./tools/idf.sh                  # 显示帮助
#
#  ESP-IDF 查找顺序：
#    IDF_PATH -> $IDF_TOOLS_PATH/frameworks/esp-idf-*
#    -> ~/.espressif/frameworks/esp-idf-* -> ~/esp/esp-idf
#    -> /opt/esp/idf -> ~/esp-idf
#  多个版本并存时，可用 IDF_PATH 直接指定要用的版本。
# =====================================================================
set -eu

# Windows 的 Git Bash / MSYS 不在 ESP-IDF 支持范围内，直接给出明确提示
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*)
        echo "[错误] 检测到 Git Bash / MSYS 环境。" >&2
        echo "  Windows 请在 PowerShell 中运行: .\\tools\\idf.ps1 build" >&2
        exit 1
        ;;
esac

show_help() {
    cat <<'EOF'

ESP32-S3 Demo - ESP-IDF 构建入口

用法:
  ./tools/idf.sh build                    编译
  ./tools/idf.sh flash                    烧录（自动选择串口）
  ./tools/idf.sh -p /dev/ttyUSB0 flash monitor
                                          指定串口烧录并查看日志
  ./tools/idf.sh menuconfig               图形化配置
  ./tools/idf.sh fullclean                清理构建目录
  ./tools/idf.sh                          显示本帮助

参数会原样传给 idf.py；查看全部命令: ./tools/idf.sh --help
EOF
}

if [ "$#" -eq 0 ]; then
    show_help
    exit 0
fi

find_idf() {
    if [ -n "${IDF_PATH:-}" ] && [ -f "$IDF_PATH/export.sh" ]; then
        printf '%s\n' "$IDF_PATH"
        return 0
    fi

    for root in "${IDF_TOOLS_PATH:-}" "$HOME/.espressif" /opt/esp /c/Espressif; do
        [ -n "$root" ] || continue
        for dir in "$root"/frameworks/esp-idf-*; do
            if [ -f "$dir/export.sh" ]; then
                printf '%s\n' "$dir"
            fi
        done
    done

    for dir in "$HOME/esp/esp-idf" /opt/esp/idf "$HOME/esp-idf"; do
        if [ -f "$dir/export.sh" ]; then
            printf '%s\n' "$dir"
        fi
    done
    return 0
}

IDF_DIR=$(find_idf | tail -n 1)

if [ -z "$IDF_DIR" ]; then
    echo "" >&2
    echo "[错误] 没有找到 ESP-IDF。" >&2
    echo "  请先安装 ESP-IDF，或指定：export IDF_PATH=/path/to/esp-idf" >&2
    echo "  详细步骤见 README.md 的“环境准备”一节。" >&2
    echo "" >&2
    exit 1
fi

# 安装器的 tools/ 通常与 IDF 同级（.../frameworks/esp-idf-x -> 上一级）；
# git 克隆方式则放在 ~/.espressif。
if [ -z "${IDF_TOOLS_PATH:-}" ]; then
    case "$IDF_DIR" in
        */frameworks/esp-idf-*)
            IDF_TOOLS_PATH=$(dirname "$(dirname "$IDF_DIR")")
            ;;
        *)
            if [ -d "$(dirname "$IDF_DIR")/tools" ]; then
                IDF_TOOLS_PATH=$(dirname "$IDF_DIR")
            elif [ -d "$HOME/.espressif/tools" ]; then
                IDF_TOOLS_PATH="$HOME/.espressif"
            fi
            ;;
    esac
    if [ -n "${IDF_TOOLS_PATH:-}" ]; then
        export IDF_TOOLS_PATH
    fi
fi

echo "[idf.sh] ESP-IDF: $IDF_DIR" >&2

# shellcheck disable=SC1090
. "$IDF_DIR/export.sh" >/dev/null 2>&1

if ! command -v idf.py >/dev/null 2>&1; then
    echo "[错误] 激活环境后仍找不到 idf.py（$IDF_DIR），ESP-IDF 安装可能不完整。" >&2
    exit 1
fi

cd "$(dirname "$0")/.."
exec idf.py "$@"
