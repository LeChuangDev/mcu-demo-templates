# ESP32-S3 Demo

一个开箱即用的 ESP32-S3 最小工程模板：上电后板载 LED 闪烁，并通过串口每秒打印一次心跳和剩余内存；
同时输出一路 1 kHz、50% 占空比的 PWM（默认 GPIO4），方便用示波器/逻辑分析仪确认固件已运行。
按 **App / BSP / Config** 分层，App 层只依赖 `bsp.h`；引脚、频率等板级参数集中在
`BSP/Src/bsp_config.h`，换板子只改这一个文件。

## 目录结构

```
ESP32S3_Demo/
├─ main/                    App 层：app_main() 入口
├─ BSP/
│  ├─ Inc/bsp.h             板级接口（App 只能 include 这个文件）
│  └─ Src/
│     ├─ bsp_config.h       ★ 板级配置：LED / PWM 引脚、频率、占空比
│     ├─ bsp_led.c          LED 实现
│     ├─ bsp_pwm.c          PWM 测试输出（LEDC）
│     └─ bsp_delay.c        延时实现
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
选择 `烧录 Flash`、`IDF: monitor`、`IDF: build + flash + monitor` 等任务。
任务名 `编译 Build` / `烧录 Flash` 与 Templates 下其它工程保持一致，
同一套键盘快捷键在所有模板工程里通用。

**串口**固定在 `.vscode/settings.json` 的 `idf.port`（默认 `COM11`）：

```jsonc
"idf.port": "COM11"
```

烧录和监视任务只连这一个口，不再扫描全部串口，速度明显更快。
换板子/换 USB 口后改这一行；设为 `""` 则退回自动扫描。
命令行 `.\tools\idf.ps1 flash` 也读这个配置，显式写的 `-p COM3` 优先。
`idf.port` 同时是 ESP-IDF 扩展的串口设置，在扩展里选择端口也会写到这里。

> `.vscode/settings.json` 里的 `idf.currentSetup` 是本机 IDF 路径，只影响 ESP-IDF 扩展的菜单；
> 换电脑后按需修改或删除，不影响命令行编译和烧录。

### 快捷键（可选）

VS Code 的快捷键配置在用户目录，工程本身带不了。推荐用 `Alt+B` 编译、
`Alt+U` 编译并烧录（B = Build，U = Upload）：单手可操作，焦点在编辑器、终端或资源管理器时
都生效，不区分大小写，也不占用 VS Code 的默认快捷键。按 **Ctrl+Shift+P** →
`Preferences: Open Keyboard Shortcuts (JSON)`，把下面两行加到数组里：

```jsonc
{ "key": "alt+b", "command": "workbench.action.tasks.runTask", "args": "编译 Build" },
{ "key": "alt+u", "command": "workbench.action.tasks.runTask", "args": "烧录 Flash" }
```

也可以用 `Shift+Z` / `Shift+X`，但它们是输入大写字母的按键，必须加
`"when": "!editorTextFocus"` 限制在编辑器外触发，否则编辑器里打不出大写 Z/X：

```jsonc
{ "key": "shift+z", "command": "workbench.action.tasks.runTask", "args": "编译 Build", "when": "!editorTextFocus" },
{ "key": "shift+x", "command": "workbench.action.tasks.runTask", "args": "烧录 Flash", "when": "!editorTextFocus" }
```

其它任务（monitor、menuconfig 等）同理，把 `args` 换成对应任务名即可。

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
| LED 引脚、有效电平 | `BSP/Src/bsp_config.h` 的 `BSP_LED_GPIO` / `BSP_LED_ACTIVE_LEVEL` |
| PWM 引脚、频率、占空比 | `BSP/Src/bsp_config.h` 的 `BSP_PWM_GPIO` / `BSP_PWM_FREQ_HZ` / `BSP_PWM_DUTY_PERCENT` |
| Flash 容量、PSRAM | `Config/sdkconfig.defaults`（见下节） |
| 延时 / 计时实现 | `BSP/Src/bsp_delay.c` |
| 板级接口 | `BSP/Inc/bsp.h` |
| 应用逻辑 | `main/main.c` |

`bsp_config.h` 是 BSP 内部头文件，只被 BSP 源文件包含；App 层仍然只 `#include "bsp.h"`。

### Flash 容量与 PSRAM

`Config/sdkconfig.defaults` 默认按 **16MB Flash、不开 PSRAM** 配置：

```ini
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
# CONFIG_SPIRAM=y
# CONFIG_SPIRAM_MODE_OCT=y
```

- **Flash 容量**：按模组实际容量改成 `_2MB` / `_4MB` / `_8MB` / `_16MB` 对应选项。
  容量设置偏小不会烧坏，但启动日志会出现
  `Detected size(16384k) larger than the size in the binary image header(2048k)`，
  且超过设置大小的空间无法使用。
- **PSRAM**：N16R8 等带 8MB PSRAM 的模组取消后两行注释；`CONFIG_SPIRAM_MODE_OCT`
  对应八线 PSRAM（R8），四线（R2）模组改用 `CONFIG_SPIRAM_MODE_QUAD=y`。
  板子没有 PSRAM 时保持注释，否则启动会失败。

改完执行 `.\tools\idf.ps1 fullclean` 再编译，让默认配置重新生成 `sdkconfig`。

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

**启动日志出现 `Detected size larger than the size in the binary image header`**
固件头里记录的 Flash 容量比芯片实际容量小（例如 16MB 芯片按 2MB 编译）。
把 `Config/sdkconfig.defaults` 里的 `CONFIG_ESPTOOLPY_FLASHSIZE_*` 改成实际容量，
然后 `fullclean` 重新编译烧录。不影响功能，但超过设定值的空间用不到。

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
