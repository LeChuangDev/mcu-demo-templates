# =====================================================================
#  ESP32-S3 工程总装脚本（目录结构对齐 ST407_Demo）
#
#    App 层   : main/                    —— IDF 强制入口组件目录
#    BSP 层   : BSP/Inc  BSP/Src         —— 板级封装，App 只依赖 bsp.h
#    Config   : Config/sdkconfig.defaults
#    tools    : tools/idf.ps1            —— 命令行构建入口
#    芯片层   : 由 ESP-IDF 提供（对应 ST407 的 Core/Drivers），无需自建
# =====================================================================
cmake_minimum_required(VERSION 3.16)

# ---- 1. 启用 ccache 加速重复编译 ----
# ccache 由 ESP-IDF 安装器提供，激活 IDF 环境后即在 PATH 中；
# 未安装时 IDF 只会告警，不影响构建。此设置会覆盖 idf.py --no-ccache 的默认值。
set(CCACHE_ENABLE ON)

# ---- 2. BSP 放在工程根（而不是 components/ 下），需显式告知 IDF ----
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/BSP")

# ---- 3. 默认配置表改放 Config/，等价于 ST407_Demo 的 Config 层 ----
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/Config/sdkconfig.defaults")

# ---- 4. 引入 ESP-IDF 构建系统并定义工程 ----
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32S3_Demo)
