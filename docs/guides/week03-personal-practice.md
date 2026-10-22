# 第三周个人练习：屏幕与串口启动文字、烧录和取证

整理日期：2026-10-22。此前REQ-001/TC-001/TC-002已验证v0.1.1串口文字；课程手册第7页环境闭环还要求屏幕文字，当前补REQ-002/TC-003/TC-004，目标v0.1.2。详见[测试计划](../03-测试计划.md)。本练习不是完整T1任务。

## 0. 当前先做：v0.1.2屏幕补练

原v0.1.1只有串口输出和RGB循环，没有屏幕界面；它的两次启动证据仍有效，但不能作为屏幕验证。以下第1—6节保留串口练习过程及旧固件校验值，当前屏幕补练按本节操作，不能把旧SHA-256用于新版。

本轮修改src/main.c、src/course_ui.c/.h和project/proj.conf。main启动LVGL显示适配，界面从main接收同一版本/练习文字；RGB由定时器继续驱动，主线程持续刷新画面，颜色日志不再循环刷屏。project/proj.conf启用LVGL v9适配、图形内存堆、实际字体和8192字节主线程栈，依据见设计文档。主机预览是同一UI源码的软件渲染，不是板卡照片。

1. 本轮v0.1.2完整构建已成功，原始失败与重建日志均保存。在VS Code打开D:\project\sifli\course_starter，确认板型sf32lb52-lchspi-ulp、SDK v2.5.1和实际WCH串口。当前固件校验见evidence/week03/screen_build_metadata_20261022.json。
2. 关闭设备监视器，点击CodeKit Download，保存下载结束输出或截图；不另运行bat，不烧录其他工程。
3. 下载结束后打开监视器，1000000 baud；若没捕获启动文字，在发送框输入reboot，行尾CRLF，发送一次。
4. 观察至少5秒，拍板卡屏幕。应看清course_starter、v0.1.2、Week 03 practice: modified firmware is running.；文字可自动换行，但不能缺字。反馈RGB是否仍轮换、颜色日志是否已停止刷屏；发送启动版本和错误原文。
5. 首次结果核对后，再发送一次reboot，拍第二次启动后的屏幕并保存对应启动输出。这是TC-004，用于确认复位后也能显示。
6. 屏幕仍黑时先发本版启动输出及照片，不先判硬件坏，也不写显示PASS。拿到真实证据后保存evidence/week03，追加manifest.physical_verification并作hw提交。

源码/构建与实物结论分开记录；旧v0.1.1证据、主机预览或初始化日志都不能代替本轮屏幕照片。本人解释与身份、原始会话等交接字段仍需按实际补齐。

当前已收到用户实物照片、16:43复位日志及RGB仍轮换/停止刷屏反馈，TC-003通过；用户又明确确认这次复位后仍显示相同内容、文字完整，TC-004通过。未收到第二张照片，真实确认来源及采集缺口见报告。板卡操作阶段结束，无需重新下载或再次复位；下一步保存用户本人的修改解释并补交接材料，不把已确认部分改回未测。

## 1. 先读懂已经完成的修改

在VS Code打开 `D:\project\sifli\course_starter`，找到 `src/main.c`。当前代码提交是 `7b904e0`，原始基线是 `b3b2024`；工作区核对时干净。此前AI已协助完成以下修改和编译，不需要重复覆盖文件或重做初始化。

| 位置 | 原始版本 | 当前修改版 |
| --- | --- | --- |
| `APP_VERSION` | `"0.1.0"` | `"0.1.1"` |
| `main()`里的启动文字 | `Hello world!` | `Week 03 practice: modified firmware is running.` |

关键代码已存在：

```c
#define APP_VERSION     "0.1.1"

/* main() 中，RGB初始化和while循环之前 */
rt_kprintf("\n[%s] v%s boot ok\n", APP_NAME, APP_VERSION);
rt_kprintf("Week 03 practice: modified firmware is running.\n");
```

这两行位于循环之前，因此每次启动执行一次；循环内RGB颜色日志会重复。可以在工程根目录的终端查看真实修改：

```powershell
git diff b3b2024 7b904e0 -- src/main.c
```

先沿用这个已提交版本完成一次上板闭环。若之后再改自己的文字、版本或闪烁行为，需先同步REQ/TC再改代码、重新编译，不能沿用旧固件和旧验证结论。

## 2. 编译阶段与当前可用固件

原始版和v0.1.1已通过项目 `scripts/build_firmware.py` 构建。本次重新核对源码、main.bin、bootloader、ftab和两份下载配置的SHA-256，六项均与 `evidence/week03/build_metadata_20261016.json` 一致；本次没有修改应用或重新编译。

当前main.bin为297476字节，SHA-256：

```text
34247EFDBA3F37FBAB2354A0FFB93F6C2F6C5B800243A9F102B38D48BDD31E6C
```

以上为此前v0.1.1的构建记录。当前输出目录已更新为v0.1.2屏幕版，不能再用上述旧哈希描述当前main.bin；屏幕补练按第0节执行。以后改了源码，在Ctrl+Shift+P → `新建 SiFli 终端`（New SiFli terminal）打开的SDK已激活终端中，从工程根目录运行：

```powershell
python scripts/build_firmware.py
```

板型用 `sf32lb52-lchspi-ulp`，SDK用v2.5.1。需要看到完整结束状态和退出结果；编译通过不等于板上通过。本实验不要求重新安装环境。

## 3. 连接并下载当前工程

1. 使用此前下载peripheral_lab的USB数据线及板卡USB转串口接口，连接电脑和板卡。
2. Ctrl+Shift+P → `选择下载串口`，查看实际WCH串口。历史为COM7；如果这次端口变化，使用本次实际端口。2026-10-22早先检查曾只看到COM3—COM6蓝牙串口，后续用户已完成Download并提供启动输出；不能把早先枚举情况当成现在未连接，也不能选择蓝牙端口代替开发板。
3. 先关闭设备监视器及其他占用该板串口的工具。
4. 确认VS Code当前打开的是 `D:\project\sifli\course_starter`，CodeKit板型为 `sf32lb52-lchspi-ulp`、SDK为v2.5.1。插件可根据该目录下的src和project识别工程；当前产物位于 `project/build_sf32lb52-lchspi-ulp_hcpu`。
5. 点击CodeKit的 **Download**。鼠标悬停时确认提示为 `Run SiFli download`，以实际命令名称为准。现有修改版已经编译，使用该工程生成的完整下载配置即可。
6. 等待下载任务结束，保存完整输出或下载结果截图，建议文件名 `evidence/week03/flash_20261022.txt` 或同日期截图。出错时保留错误原文，下载尚不能记为成功。

CodeKit按钮下载和生成的uart_download.bat是两种可选入口，不需要依次执行。日常直接使用Download按钮即可；bat只是在需要命令行下载时的备用方式。本工程完整下载配置包含bootloader、main、ftab三份固件及对应地址。这里是给用户的操作步骤，AI没有执行烧录。若改天操作，证据用实际执行日期命名。

## 4. 第一次启动证据：TC-001

1. Ctrl+Shift+P → `监视设备`（Monitor device），选择实际WCH串口。
2. 设置1000000 baud、8数据位、无校验、1停止位、无流控；发送选择文本模式。
3. 保持监视器打开，在发送框输入 `reboot`，行尾选择CRLF，然后发送。只输入单词，不要把字符 `\r\n` 打进文本。
4. 当前配置启用了FinSH，SDK源码及现有main.elf均包含reboot命令；它用于在监视器已打开时触发重启，便于捕获早期启动文字。2026-10-22收到的用户记录已包含TX reboot、SFBL及新的启动文字，确认本次实际命令有效；以后若无响应或出现命令错误，保存原文再排查。
5. 保留启动起始至red、green、blue三行日志，并观察板上RGB实际表现。

下面是预期文本示意，尚非用户实测结果，不能直接保存成成功证据：

```text
[course_starter] v0.1.1 boot ok
Week 03 practice: modified firmware is running.
[course_starter] rgb -> red
[course_starter] rgb -> green
[course_starter] rgb -> blue
```

通过判据：同一次启动包含版本行和练习文字，练习文字在本次启动只出现一次。实际有异常则如实记录。颜色日志仅说明执行到打印位置，灯的实际颜色由用户观察，不能用日志代替。

原始输出/截图建议保存为 `evidence/week03/boot_20261022.txt` 或同日期截图，保留监视器时间、RX分片和异常。

## 5. 再次启动证据：TC-002

串口保持打开，再发送一次 `reboot` + CRLF。保存第二次启动至三行RGB日志，确认版本仍为v0.1.1，练习文字在这次启动同样只出现一次。建议文件名 `reboot_20261022.txt`；也可用同一个原始日志，清楚区分两次启动。

2026-10-22已收到两次真实启动输出：首段RX标签10:46:27；第二段包含TX 10:54:07的reboot及RX 10:54:09启动输出。两次版本和练习文字各一次，TC-001/TC-002的启动文字判据通过，详见[测试报告](../04-测试报告.md)。用户另确认LED轮换。采集未覆盖步骤中的完整三行RGB，作为辅助记录缺口保留；AI此前额外要求追着复制RGB，现按原始通过判据纠正，不改写原测试计划。当前无需重烧、再复位或继续复制刷屏日志；若以后出现失败，保留失败证据后追加复测。

## 6. 解释、记录与Git收尾

用户用自己的话说明：改了哪个文件和哪两处；版本行如何组成；启动文字为什么每次只打印一次；如何排除仍运行旧程序。这是本人理解记录，不能由AI代写成“已经掌握”。

已按用户下载反馈与两次启动证据更新测试报告、`manifest.yaml` 的physical_verification/test_results和当日开发日志，并以 `hw(...)` 保存证据。本次应用代码仍为7b904e0对应内容，不生成假的代码改动。

当前manifest另缺身份信息与原始会话导出等字段，需按规范如实补齐；不能用整理摘要替代原始导出。板端两次启动文字已验证，本人解释尚待记录，本指南不代表最终交接材料已齐全。
