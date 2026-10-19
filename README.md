# MCU Firmware Demo Templates
基于 ARM-GCC 工具链 + CMake 构建系统的 MCU 工程模板集合。

## 支持平台
- `AT435_Demo`：雅特力 AT435 系列基础工程
- `GD407_Demo`：兆易创新 GD32F407 系列基础工程
- `ST407_Demo`：意法半导体 STM32F407 系列基础工程

## 环境要求
- ARM-GCC 工具链（arm-none-eabi-gcc）
- CMake 3.15+
- Make / Ninja 构建工具

## 编译方式
以 ST407_Demo 为例：
```bash
cd ST407_Demo
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
make -j8
