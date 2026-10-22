# 第三周个人修改练习收尾记录

完成记录日期：2026-10-22（Asia/Shanghai）。范围：course_starter一次可观察修改、编译、烧录、屏幕与串口验证、证据、本人解释和本地Git。本记录不代表T0全部训练、T1按键任务或正式产品封版。

## 修改与版本

| 阶段 | 内容 | 关键提交 |
|---|---|---|
| 原始基线 | v0.1.0，串口Hello world与RGB循环，没有屏幕页 | b3b2024 |
| 串口练习 | v0.1.1，修改版本与练习文字；原始/修改版均编译 | 9520e08、7b904e0 |
| 串口实物 | 两次启动文字，用户确认下载/LED轮换 | 4450970、a8435e3 |
| 屏幕要求补齐 | 用户指出文字应在屏幕显示；先补REQ-002与用例 | 0a475d9 |
| 屏幕实现 | v0.1.2，LVGL页、RGB非阻塞定时器、停止颜色日志刷屏 | 0d20b1e |
| 屏幕与复位实物 | 原图、16:43启动日志、RGB反馈及复位后画面明确确认 | 86b6104、8b1d813、7a75891 |

当前修改位置：src/main.c的APP_VERSION/PRACTICE_MESSAGE及显示主循环；src/course_ui.c/.h创建屏幕页；project/proj.conf启用SDK图形适配、字体、内存堆与主线程栈。原始构建脚本、规范和检查脚本未改动。

## 结果与证据

| 验证 | 结论 | 原始证据（相对工程根目录） |
|---|---|---|
| v0.1.2完整构建 | 通过；首次漏配MEMHEAP失败已保留，补配置后成功；仍有SDK/链接告警 | evidence/week03/screen_build_first_attempt_20261022.txt、screen_build_20261022.txt |
| TC-001 / TC-002 | v0.1.1串口文字范围通过 | evidence/week03/boot_20261022_user_excerpt.txt、reboot_20261022_user_transcript.txt |
| TC-003 | v0.1.2屏幕/串口及RGB、停止刷屏通过 | evidence/week03/screen_20261022_user_photo.jpg、reboot_164319_20261022_user_transcript.txt |
| TC-004 | 本次复位后版本与画面通过，画面来自用户明确实物确认 | evidence/week03/reboot_164319_screen_confirmation_20261022.txt |
| 本人解释 | 用户指出main中的version/文字，通过屏幕确认；与实际一致 | evidence/week03/personal_explanation_20261022_user.txt |

用户原话：

> 在main文件改了一下version 然后的话还有文字 通过屏幕显示确认的

源代码和主机检查由Codex协助，用户负责实际下载、观察、复位、证据和说明。AI曾误把串口练习当作足够覆盖屏幕要求，用户纠正后补齐屏幕；构建/原生预览的失败日志与纠正均保留，不美化过程。

## 可复现信息

- 板型sf32lb52-lchspi-ulp；SDK v2.5.1，Git 421126d9f476ed8e2a6f0b0ca28a9f241c182e65；ARM GCC 14.2.1；监视1000000 baud。
- SDK已激活终端中，从工程根目录运行python scripts/build_firmware.py；CodeKit打开工程根目录后用Download下载完整配置。原始生成的下载配置含bootloader/main/ftab，不能只下载猜测地址的单个文件。
- 本次保留实测固件，不在收尾阶段重编译。main.bin为695184字节，SHA-256为BB28EBC19FE4A75F7C38CF839771C43BAAC292D60A964078CA5275CE57A2F829；源码/配置/固件明细见evidence/week03/screen_build_metadata_20261022.json。
- 本地收尾标签计划为week03-practice-v0.1.2；标签与归档中的Git提交信息共同定位最终文档及证据。该标签仅表示个人练习，不是正式产品Release。
- 课程资料目录outputs将独立保存源码快照、Git bundle、现有可烧录固件与校验清单；源码仓库继续忽略构建产物。归档的实际验证报告随包保存，不以尚未运行的检查填成功。

## 保留的限制

原始完整CodeKit下载输出未取得，不补造退出码或写入校验；实物日期未包含在用户日志中，保存证据收到日期，未知executed_at留空。TC-004没有第二张照片，保留采集缺口，依据用户明确实物确认和本次日志通过原画面/版本判据，不把旧图冒充新图。未测长期稳定性、精确RGB节奏、运行时堆/栈峰值或其他外设。

姓名、学号、正式组别、平台提交时间和原始会话导出未提供，manifest仍如实标缺；不影响已完成的本次个人练习结论，也不冒充最终产品材料齐全。正式M0—M5、完整T0/T1按各自范围另行推进。UART待核实、双指未通过的peripheral_lab结论保持，小智与产品选题本次不处理。
