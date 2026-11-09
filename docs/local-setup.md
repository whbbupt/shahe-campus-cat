# 从零开始配置沙河校园小猫

以下步骤以 **Windows PowerShell** 为例。没有开发板也可以做到第 4 步并生成固件。要在实体手表上运行，准备一块黄山派 `sf32lb52-lchspi-ulp` 和一根支持数据传输的 USB 线。安卓工具独立打包，见第 6 步。

## 1. 安装软件

| 软件 | 用途 |
| --- | --- |
| [Git for Windows](https://git-scm.com/download/win) | 下载项目与 SDK 源码 |
| [Python](https://www.python.org/downloads/windows/) 3.13 | 运行项目的环境检查和构建脚本。安装时勾选将 Python 加入 PATH。 |
| [uv](https://docs.astral.sh/uv/getting-started/installation/) | 供 SDK 安装脚本准备编译工具与自己的 Python 环境 |
| [VS Code](https://code.visualstudio.com/Download) 与 [SiFli-SDK-CodeKit](https://docs.sifli.com/projects/codekit/get-started/install.html) | 查看代码并用图形界面烧录。只编译时可以跳过这两个。 |

安装后重新打开 PowerShell，逐行运行以下命令。三行应分别显示版本号。如果提示无法识别某个命令，先完成对应软件的安装或重新打开终端。

```powershell
git --version
python --version
uv --version
```

## 2. 下载项目和指定版本的 SDK

下面以 `C:\dev` 为例。也可以换成自己有写入权限的英文目录，后面命令中的路径一并替换。

```powershell
New-Item -ItemType Directory -Force C:\dev | Out-Null
Set-Location C:\dev
git clone https://github.com/whbbupt/shahe-campus-cat.git
git clone --branch v2.5.1 --recurse-submodules https://gitee.com/SiFli/sifli-sdk.git sifli-sdk-2.5.1
```

`shahe-campus-cat` 是项目目录，`sifli-sdk-2.5.1` 是独立的 SDK 目录。克隆 SDK 时保留 `--recurse-submodules`，用于下载构建所需的子模块。如果子模块下载中断，进入 SDK 目录运行 `git submodule update --init --recursive` 继续。

用第 1 步装好的 Python，在项目目录创建独立的 Python 环境并安装项目脚本的依赖。以下命令只在第一次配置时运行。

```powershell
Set-Location C:\dev\shahe-campus-cat
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
```

## 3. 安装和启用 SDK 工具链

首次在 SDK 目录运行安装脚本，再运行导出脚本。安装脚本会下载 SCons、ARM 编译工具及相关依赖。已经用 CodeKit 安装过 v2.5.1 时，可进入实际 SDK 目录直接导出。官方说明见 [SiFli SDK v2.5.1 Windows 安装流程](https://docs.sifli.com/projects/sdk/v2.5.1/sf32lb52x/quickstart/install/script/windows.html)。

```powershell
Set-Location C:\dev\sifli-sdk-2.5.1
.\install.ps1
.\export.ps1
```

以后每打开一个新 PowerShell，都要先进入 SDK 目录运行一次 `.\export.ps1`，再在**同一个窗口**进入项目目录。若之前自定义过 SDK 工具目录，在导出前把环境变量 `SIFLI_SDK_TOOLS_PATH` 设为那个目录。

## 4. 检查环境并编译固件

继续用刚才导出过 SDK 的窗口。

```powershell
Set-Location C:\dev\shahe-campus-cat
.\.venv\Scripts\python.exe scripts/check_env.py
.\.venv\Scripts\python.exe scripts/build_firmware.py
```

这里直接调用项目自己的 Python。SiFli SDK 导出后也会提供一个 Python，项目依赖装在 `.venv` 即可。

环境检查应能找到 SiFli SDK、SCons 和 `arm-none-eabi-gcc`。编译成功后会显示输出目录 `project/build_sf32lb52-lchspi-ulp_hcpu/`。目录内应有 `main.bin`、`bootloader/bootloader.bin` 和 `ftab/ftab.bin`。完整编译输出在 `build/last_build.log`。

## 5. 烧录到手表

1. 用数据线连接开发板的 **USB-to-UART** 口。打开 Windows「设备管理器 → 端口（COM 和 LPT）」查看新出现的 COM 号。
2. 在 VS Code 打开克隆下来的仓库，用 CodeKit 选中 `project/`，核对板型为 `sf32lb52-lchspi-ulp`，点击 **Download**，选择刚才的串口。
3. 也可以在已导出 SDK 的 PowerShell 中运行 `.\project\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat`，按提示输入端口数字。例如 `COM7` 输入 `7`。保留存档时使用常规下载，避开全片擦除。
4. 烧录完成后复位。屏幕应出现小猫场景，点右上角「状态」核对固件版本。需要排查启动时，用串口助手打开同一个端口，波特率设为 `1000000`，再复位查看启动信息。手机连接方式见[上手与校时](pet-quickstart.md)。

## 6. 可选：从源码打包安卓校时工具

准备 [JDK 17](https://adoptium.net/temurin/releases/?version=17) 和 [Android Studio](https://developer.android.com/studio)。在 Android Studio 的 **SDK Manager** 中安装 **Android 14（API 34）** 和 **Android SDK Build-Tools 36.0.0**，记下窗口顶部的 **Android SDK Location**。项目自带独立打包脚本，可以从仓库根目录运行。

```powershell
.\.venv\Scripts\python.exe android-clock/build.py --sdk "<你的 Android SDK 目录>" --jdk "<你的 JDK 17 目录>"
```

把两个带尖括号的占位值连同尖括号换成实际路径，保留双引号。成功后将 `build/android-clock/shahe-clock.apk` 传到安卓手机安装。最低支持 Android 8.0。连接、权限和读回校验见[安卓工具说明](../android-clock/README.md)。

## 常见卡点

| 看到什么 | 怎么处理 |
| --- | --- |
| PowerShell 阻止运行 `install.ps1` 或 `export.ps1` | 在当前 PowerShell 执行 `Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned` 后重试官方脚本。关闭窗口后这项设置即失效。 |
| 找不到 SDK、SCons 或 ARM 编译器 | 回到 SDK 目录运行 `.\export.ps1`，再在同一窗口返回项目目录。若导出提示工具缺失，回到第 3 步安装。 |
| 系统没有显示新的 COM 端口 | 检查数据线和 USB-to-UART 口。若设备管理器显示未知设备，按板卡说明安装对应的串口驱动。 |
| 下载提示串口被占用 | 关闭串口监视器，重新下载。下载结束再打开串口监视器。 |
