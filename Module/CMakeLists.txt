cmake_minimum_required(VERSION 3.20)

project(mcu_modules LANGUAGES C)

# 供 clangd / IDE 使用；不影响实际编译。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

# 主机侧单元测试。集成到 MCU 工程时关闭即可。
option(MODULE_BUILD_TESTS "Build host-side unit tests" ON)

add_subdirectory(Jy61_P)
add_subdirectory(Oled_Spi)
add_subdirectory(Zigbee)
