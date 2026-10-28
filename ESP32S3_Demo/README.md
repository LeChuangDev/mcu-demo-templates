# ESP32-S3 Demo

一个开箱即用的 ESP32-S3 最小工程模板：上电后板载 LED 闪烁，并通过串口每秒打印一次心跳和剩余内存。
按 **App / BSP / Config** 分层，App 层只依赖 `bsp.h`，方便换板子和继续加功能。

## 目录结构

```
ESP32S3_Demo/
├─ main/                    App 层：app_main() 入口
├─ BSP/
│  ├─ Inc/bsp.h             板级接口（App 只能 include 这个文件）
│  └─ Src/                  LED、延时等板级实现
├─ Config/
│  └─ sdkconfig.defaults    工程默认配置（改这里后需 fullclean 才生效）
├─ tools/
│  ├─ idf.ps1               Windows 构建入口（自动查找并激活 ESP-IDF）
│  ├─ idf.sh                Linux / macOS 构建入口
│  └─ update-clangd.ps1     按本机工具链刷新 .clangd 的头文件路径
├─ CMakeLists.txt           顶层构建脚本
└─ README.md
```

## 环境准备（只需做一次）

### 1. 硬件与 USB 驱动

- ESP32-S3 开发板一块
- **能传数据的 USB 线**（只能充电的线会导致电脑识别不到串口）
- USB 转串口驱动（按板子上的芯片二选一）：
  - **CP210x**（Silicon Labs）：<https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers>
  - **CH340 / CH343**（WCH）：<https://www.wch.cn/downloads/>

装好后打开「设备管理器 → 端口 (COM 和 LPT)」，能看到 `COMx` 即可。
Linux 下一般会出现 `/dev/ttyUSB0` 或 `/dev/ttyACM0`。

### 2. 安装 ESP-IDF

任选一种方式，**推荐前两种**：

1. **官方离线安装器（Windows 推荐）**
   下载地址：<https://dl.espressif.com/dl/esp-idf/>
   安装时勾选 ESP32-S3 相关组件，默认会装到 `C:\Espressif`。
2. **VS Code 的 ESP-IDF 扩展**
   在扩展市场搜索 `ESP-IDF`，按向导安装即可。
3. **git 克隆 + 官方脚本**
   参考：<https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html>

本工程只依赖标准 ESP-IDF，不需要额外下载组件（没有 `idf_component.yml`）。
推荐使用 **ESP-IDF v5.5.x**；v5.0 及以上也可以（5.3 以下会自动改用 `driver` 组件）。

> `tools/idf.ps1` 会自动查找 ESP-IDF，**不需要**事先打开「ESP-IDF PowerShell」。
> 查找顺序：`IDF_PATH` → `%IDF_TOOLS_PATH%\frameworks\esp-idf-*` →
> `%USERPROFILE%\.espressif\frameworks\esp-idf-*` → `%USERPROFILE%\esp\esp-idf` → `C:\esp\esp-idf`。
> 装在别处时，设置一次环境变量即可：
>
> ```powershell
> $env:IDF_PATH = "D:\Espressif\frameworks\esp-idf-v5.5.5"
> ```

### 3. PowerShell 执行策略（仅 Windows 首次）

如果提示「禁止运行脚本」，在 PowerShell 里执行一次：

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

不想改策略也可以每次这样调用：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\idf.ps1 build
```

## 下载 / 克隆

```powershell
git clone https://github.com/LeChuangDev/mcu-demo-templates.git
cd mcu-demo-templates/ESP32S3_Demo
```

没有 git 的话，直接在 GitHub 页面下载 ZIP 解压，进入 `ESP32S3_Demo` 目录即可。

> 用 VS Code 时请直接**打开 `ESP32S3_Demo` 这个文件夹**（`文件 → 打开文件夹`），
> 而不是打开仓库根目录，否则 `.vscode/tasks.json` 和 `.clangd` 不会生效。

## 编译与烧录

### Windows

```powershell
# 编译（首次会自动生成 sdkconfig 和 build/）
.\tools\idf.ps1 build

# 自动选择串口：编译 + 烧录 + 打开串口日志
.\tools\idf.ps1 flash monitor

# 有多块板子 / 指定串口
.\tools\idf.ps1 -p COM3 flash monitor
```

串口日志界面按 **Ctrl + ]** 退出。

### Linux / macOS

```bash
chmod +x tools/idf.sh          # 首次需要；如果 git 已记录可执行位可省略
./tools/idf.sh build
./tools/idf.sh -p /dev/ttyUSB0 flash monitor
```

### 在 VS Code 里

按 **Ctrl + Shift + B** 直接编译。其它操作按 **Ctrl + Shift + P** → `Tasks: Run Task`，
选择 `IDF: flash`、`IDF: monitor`、`IDF: build + flash + monitor` 等任务。
串口默认自动选择，要在任务里固定端口，在 `tasks.json` 对应任务的 `args` 里加 `"-p", "COM3"`。

> `.vscode/settings.json` 里的 `idf.currentSetup` 是本机 IDF 路径，只影响 ESP-IDF 扩展的菜单；
> 换电脑后按需修改或删除，不影响命令行编译和烧录。

### 常用命令

| 命令 | 作用 |
| --- | --- |
| `.\tools\idf.ps1 build` | 编译 |
| `.\tools\idf.ps1 flash` | 烧录（会自动先编译） |
| `.\tools\idf.ps1 monitor` | 打开串口日志 |
| `.\tools\idf.ps1 -p COM3 flash monitor` | 指定串口烧录并看日志 |
| `.\tools\idf.ps1 menuconfig` | 图形化配置（生成根目录 `sdkconfig`） |
| `.\tools\idf.ps1 fullclean` | 清理构建目录 |
| `.\tools\idf.ps1 merge-bin` | 生成 `build/merged-binary.bin`（见下） |
| `.\tools\idf.ps1 --help` | 查看 idf.py 全部命令 |

Linux / macOS 把 `.\tools\idf.ps1` 换成 `./tools/idf.sh` 即可。

### 生成“一个文件”的固件（可选）

如果只想把固件发给别人（不需要对方安装 ESP-IDF）：

```powershell
.\tools\idf.ps1 build merge-bin
```

生成的 `build/merged-binary.bin` 已包含 bootloader、分区表和应用程序。
对方只需要 Python 和 esptool（`pip install esptool`），从 `0x0` 烧录即可：

```powershell
python -m esptool --chip esp32s3 -p COM3 -b 460800 write_flash 0x0 build/merged-binary.bin
```

## 修改板级参数

换板子时只需要改 BSP 层，App 层不用动：

| 要改什么 | 改哪里 |
| --- | --- |
| LED 引脚、有效电平 | `BSP/Src/bsp_led.c` 顶部的 `BSP_LED_GPIO` / `BSP_LED_ACTIVE_LEVEL` |
| 延时 / 计时实现 | `BSP/Src/bsp_delay.c` |
| 板级接口 | `BSP/Inc/bsp.h` |
| 应用逻辑 | `main/main.c` |
| 工程默认配置 | `Config/sdkconfig.defaults` |

> 部分 ESP32-S3 开发板（如 DevKitC-1 v1.1）板载的是 WS2812 RGB 灯（接 GPIO48），
> 不能直接用 GPIO 电平控制，需要改用 RMT/LED 驱动，或者外接普通 LED。

## 常见问题

**找不到 ESP-IDF / 提示找不到 export.ps1**
按上面的查找顺序确认安装位置；装在不常见的位置时设置 `IDF_PATH`。

**串口列表里没有设备**
换一根能传数据的 USB 线；安装 CP210x / CH 系列驱动；换一个 USB 口。
Windows 下可在设备管理器查看是否出现 `COMx`。

**烧录时一直停在 `Connecting...`**
按住板子上的 **BOOT** 键，点一下 **RST**（或重新插拔 USB），再松开 BOOT 重试。
也请确认没有其它软件（串口助手、另一个 monitor）占用同一串口。

**首次编译很慢**
正常现象。工程已启用 ccache，后续增量编译会快很多。

**改了 `Config/sdkconfig.defaults` 不生效**
`build/` 建好后不会自动重新读默认配置，执行 `.\tools\idf.ps1 fullclean` 后重新编译；
或者删除根目录的 `sdkconfig` 再编译。

**clangd 报找不到 `stdlib.h` 等系统头文件**
`.clangd` 里显式列出了本机 xtensa 工具链的系统头文件目录。先编译一次生成编译数据库，再执行：

```powershell
.\tools\update-clangd.ps1
```

脚本会从 `build/compile_commands.json` 找到实际使用的编译器，自动改写 `.clangd` 里的
`-isystem` 路径。换电脑或升级 ESP-IDF 后重跑一次即可，最后在 VS Code 里执行
`clangd: Restart language server`。

> 不要给 clangd 加 `--query-driver`：它会让 clangd 执行 `gcc --target=...`，
> 而 xtensa-gcc 不认识 `--target`，反而会丢掉全部系统头文件。

**`sdkconfig` 要不要提交？**
不要。它由 `Config/sdkconfig.defaults` 生成，且包含本机串口等个人配置，已在 `.gitignore` 中排除。
