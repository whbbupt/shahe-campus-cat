# 第三周个人练习：启动文字修改、烧录与两次启动取证

整理日期：2026-10-22。需求REQ-001，用例TC-001/TC-002，详见[测试计划](../03-测试计划.md)。目标是能解释修改，并完成修改版上板与证据闭环。本练习使用串口文字，不是屏幕UI或完整T1任务。

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

已有构建可以用于下一步。以后改了源码，在Ctrl+Shift+P → `新建 SiFli 终端`（New SiFli terminal）打开的SDK已激活终端中，从工程根目录运行：

```powershell
python scripts/build_firmware.py
```

板型用 `sf32lb52-lchspi-ulp`，SDK用v2.5.1。需要看到完整结束状态和退出结果；编译通过不等于板上通过。本实验不要求重新安装环境。

## 3. 连接并下载当前工程

1. 使用此前下载peripheral_lab的USB数据线及板卡USB转串口接口，连接电脑和板卡。
2. Ctrl+Shift+P → `选择下载串口`，查看实际WCH串口。历史为COM7；如果这次端口变化，使用本次实际端口。2026-10-22核对时只看到COM3—COM6蓝牙串口，尚未枚举到WCH板卡，不能选择这些蓝牙端口代替开发板。
3. 先关闭设备监视器及其他占用该板串口的工具。
4. 确认VS Code当前打开的是 `D:\project\sifli\course_starter`，CodeKit板型为 `sf32lb52-lchspi-ulp`、SDK为v2.5.1。插件可根据该目录下的src和project识别工程；当前产物位于 `project/build_sf32lb52-lchspi-ulp_hcpu`。
5. 点击CodeKit的 **Download**。鼠标悬停时确认提示为 `Run SiFli download`，以实际命令名称为准。现有修改版已经编译，使用该工程生成的完整下载配置即可。
6. 等待下载任务结束，保存完整输出或下载结果截图，建议文件名 `evidence/week03/flash_20261022.txt` 或同日期截图。出错时保留错误原文，下载尚不能记为成功。

CodeKit按钮下载和生成的uart_download.bat是两种可选入口，不需要依次执行。日常直接使用Download按钮即可；bat只是在需要命令行下载时的备用方式。本工程完整下载配置包含bootloader、main、ftab三份固件及对应地址。这里是给用户的操作步骤，AI没有执行烧录。若改天操作，证据用实际执行日期命名。

## 4. 第一次启动证据：TC-001

1. Ctrl+Shift+P → `监视设备`（Monitor device），选择实际WCH串口。
2. 设置1000000 baud、8数据位、无校验、1停止位、无流控；发送选择文本模式。
3. 保持监视器打开，在发送框输入 `reboot`，行尾选择CRLF，然后发送。只输入单词，不要把字符 `\r\n` 打进文本。
4. 当前配置启用了FinSH，SDK源码及现有main.elf均包含reboot命令；它用于在监视器已打开时触发重启，便于捕获早期启动文字。此命令尚待本板运行反馈，若无响应或出现命令错误，保存原文再排查。
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

TC-001和TC-002当前仍未执行/取证；必须收到真实结果后才能更新判定。若一项失败，保留失败证据，修复后追加复测。

## 6. 解释、记录与Git收尾

用户用自己的话说明：改了哪个文件和哪两处；版本行如何组成；启动文字为什么每次只打印一次；如何排除仍运行旧程序。这是本人理解记录，不能由AI代写成“已经掌握”。

取得实际下载结果与两次启动证据后，再据实更新测试报告、`manifest.yaml` 的physical_verification/test_results、当日开发日志，并用 `hw(...)` 提交。本次此前代码提交仍为7b904e0，不生成假的代码改动。

当前manifest另缺身份信息与原始会话导出等字段，需按规范如实补齐；不能用整理摘要替代原始导出。新实验结果、原始会话和身份信息都尚未收到，因此本指南不代表个人验收已完成。
