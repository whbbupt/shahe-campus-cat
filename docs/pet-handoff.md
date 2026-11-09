# 沙河校园小猫：接手说明

> 最新版 v0.17.4 统一顶部状态、场景短签和底部反馈，详见[操作说明](pet-v0174-hud.md)。下方 v0.17.0 为生活规则基线。

> 当前入口（2026-11-09）：v0.17.0，宵禁与坐席图文菜单、独立外卖、宿舍布置及数值平衡；[本版操作与验收](pet-v017-life.md)。软件/构建通过，TC-260待实机；下文旧版本描述保留为历史，当前不再有集中商店。上传、平台提交及同学交接继续暂缓。

当前软件v0.17.0，分支feat/pet-core。用户已确认宵禁、撤商店并保留金币、场景图文点餐与宿舍布置，实施中要求先坐下弹滑动菜单及数值平衡。当前软件与构建通过，TC-260完整实机仍待；上传/课程提交及同学交接继续暂缓。GitHub默认账号whbbupt，未创建或推送remote，同学身份/实际职责未提供。

## 先从哪里看

1. 根目录AGENTS.md和README.md；再看[本版生活互动与验收](pet-v017-life.md)、[下一阶段](pet-next-steps.md)。
2. docs/01需求、02设计、03用例、04测试结果；docs/adr保存选择理由，manifest.yaml保存证据索引。
3. 主入口src/main.c→pet_app.c。pet_world是业务模型，pet_ui只呈现快照；不要在蓝牙或音频线程直接画UI或改世界。

| 模块 | 文件 | 责任 |
| --- | --- | --- |
| 养成/导航/作息 | src/pet_world.c/.h、pet_world_selftest.c、pet_life_selftest.c | 业务单调时钟、路径、奖励与资格；RTC跳变不能重复结算 |
| 小游戏 | src/pet_memory.c/.h、pet_challenge.c/.h、pet_challenge_selftest.c | 原课堂记忆与新配餐/节奏/找物；输入只改变纯C状态 |
| 画面与素材 | src/pet_ui.c、pet_shahe_assets.c、pet_world_assets.c、pet_world_font.c | 390×450，压缩缓存与触摸热点；新中文需重建子集字体 |
| 金币/场景消费 | src/pet_economy.c/.h、pet_economy_selftest.c | 价格/有界钱包/家具拥有与免费切换；世界负责唯一扣费/奖励提交点 |
| 存档 | src/pet_save.c/.h、pet_save_flash.c、pet_save_selftest.c | 格式10/64字节/双区日志，读旧1–9、旧9拒新10；外卖品类/签到日期与金币同事务，路径/动作/半局延后保存；分区不要随意移动 |
| 时间/手机 | src/pet_time*、pet_ble*、pet_cts* | 固定UTC+8、SDK RTC、CTS自动校时；保留外设安全请求，不能恢复中心专用配对调用 |
| 声音 | src/pet_audio* | 事件观察与PCM混音，SDK送音；新事件复用现有短音 |
| 验证 | tests/run_pet_tests.py、tests/*、evidence/pet | 主机、真实LVGL、NOR/SDK模拟；物理证据必须单独登记 |

## 在另一台电脑复现

工具基线：Windows、SiFli SDK v2.5.1（421126d9）、板型sf32lb52-lchspi-ulp、Arm GNU14.2.1，SDK按官方安装配置，不携带整套SDK。先读README和SDK说明安装依赖，再使用SDK export激活环境；本机export.ps1缺uv时，已用现有SDK Python执行tools/sdk_env.py export --shell powershell并加载返回脚本，没有改SDK或重新安装。

- 在已激活SDK的终端执行 `python scripts/build_firmware.py -j4`，构建日志位于build/last_build.log；CodeKit按本板型使用project目录。
- 主机完整验证：`python tests/run_pet_tests.py --cc cl --vcvars <本机vcvars64.bat> --sdk <SDK根目录>`；需本机MSVC C编译器，输出build/pet-host。
- 字体再生成：`python scripts/make_world_fonts.py --sdk <SDK根目录>`，需Pillow/fontTools，来源与OFL许可保留。
- 课程检查：`python scripts/check_all.py`；最终里程碑另按规范执行 `--final`。检查器、钩子、AGENTS/CLAUDE与SDK骨架不得为通过检查而改动。

## 验收后上传GitHub

先确认本版完整实机结果与最终基线，再在whbbupt下选择现有或新仓库，确认公开/私有与同学账号。上传前检查源码/素材来源、历史与证据中的个人信息和密钥；保留完整Git历史和原失败记录，固件放发行附件或单独交付，SDK/缓存/大视频不入源码仓库。不强推或覆盖已有远程历史。

选择仓库后设置实际remote并推送已验收分支和需要的标签，再按Git流程合并main；不把GitHub推送当教学平台已提交。若共享私有仓库，需知道同学GitHub用户名并由用户明确邀请后才操作。同学应在新目录clone、按说明构建并确认版本，再开始修改。

## 接手后的待认领事项

- 同学姓名/学号、组号、实际分工、教师范围核定和最终提交时间；本人研制总结和反思由本人表达，不代写经历。
- 根据集中验收结果处理真实缺陷，补充真实日志/视频/照片。当前不再扩展需求；如要加新功能，先定义需求/用例并由负责人确定。
- 整理M0—M5里程碑与主文档首页，使最终版本/测试/需求一致；历史进度保留，不按当前日期推算教学周。
- 准备演示和完整Git项目ZIP，按“产品名称-成员学号列表.zip”提交；截止日期以老师通知为准。
- 原2026-10-28至11-05间隔8天使日志检查失败，不能伪造提交或降低阈值；其他身份/完整时间戳/原始会话及未测项保持真实缺口。

已有v0.14.2配对和自动校时时间正确的用户确认有效；重连、完全断电保时、FPS/动态内存与音质等以实际结果补充。历史版本的“尚未存档/未连接”等描述不代表当前能力，当前状态优先看本说明和最新报告。
