# 电子宠物v0.3.0：架构与运行说明

日期2026-10-23。状态：首个离线会话版本已实现，主机逻辑/UI及目标构建通过，尚未下载或完成本版板端验收。正式产品的本地存档继续预研，联网作为拓展；此版本重启重新开始。

## 已实现的使用过程

启动看到MOMO和70/60两项数值 → Feed喂食或Pet摸摸 → 数值变化与角色动作 → 动作结束恢复真实状态。Rest进入休息，按钮变为Wake；休息时其他照料操作不可用，醒后恢复。首页明确提示Progress resets on restart。

Feed加15饱食、2秒冷却、1.2秒动作；Pet加10心情、3秒冷却、0.9秒动作。数值始终0～100；动作中请求拒绝、不排队，已满与冷却内不重复结算。饱食每5分钟减1；清醒累计10分钟心情减1，休息每分钟心情加1。当前没有死亡、成长、音频、网络或保存功能。

## 源码入口

| 文件 | 职责 |
|---|---|
| src/main.c | 初始化既有LCD/LVGL，启动产品后循环刷新；不启动T1轮询训练 |
| src/pet_app.c/.h | 主线程唯一模型所有者；事件前推进真实时钟，传送快照与操作结果 |
| src/pet_model.c/.h | 不依赖LVGL/RTOS的纯C规则；冷却、时间累计、饱和边界、动作/休息状态 |
| src/pet_ui.c/.h | 单点按钮、实际LVGL角色与动画；重建前删除旧动画定时器 |
| src/pet_selftest.c/.h | 同一固定输入用例供主机测试与实际板上启动调用 |
| tests/run_pet_tests.py | 本机C编译器构建逻辑测试；提供--sdk后编译真实LVGL和生产UI |

架构、数据路径和状态见02方案设计第8节，需求/测试见01第6节、03第6节。仅UI主线程操作LVGL，当前规则短小且有界，不需要新增业务线程。将来耗时存储/联网必须另做异步适配，不能直接塞进按钮回调。

模型按经过时间推进，不根据动画帧数计时。64位中间值计算经过周期，不逐次循环补算。32位tick通过无符号差支持一次回绕；主循环应在完整tick回绕周期内运行。毫秒转换保留余数，测试覆盖128Hz等非整毫秒tick。重建页面保留模型，重新启动程序则恢复默认状态。

## 主机验证与预览

在已有原生C编译器环境运行：

```text
python tests/run_pet_tests.py --cc cl --sdk D:/development/sifli/SiFli-SDK/v2.5.1
```

Windows普通终端可另传`--vcvars`与本机VS的vcvars64.bat完整路径；Linux/macOS传`--cc cc`。这使用主机编译器，不能用MCU交叉编译器代替。输出和缓存位于忽略的build/pet-host，主机测试日志与四态PNG保存在evidence/pet中。

七组固定输入检查初值、边界/连点/冷却、时间分段一致性与长间隔、休息拒绝与恢复、显示优先级、tick回绕与小数余量、无效事件与重置。真实UI测试覆盖Feed/Rest/Wake/Pet、页面重建10次后继续操作、动画定时器数量与顶层对象边界。

软件预览实际编译src/pet_ui.c和SDK的LVGL，模拟时钟、按钮事件和显示缓冲，不验证物理触摸、屏幕驱动、RTOS调度、实际堆/栈或长期稳定。角色为原创LVGL基础对象组合；字体沿用SDK内置Montserrat，SDK及字体原有许可保留，不导入第三方图片。

## 构建与本次产物

在已激活SDK v2.5.1、板型sf32lb52-lchspi-ulp的终端运行原脚本：

```text
python scripts/build_firmware.py
```

本机工具安装在D:/development/sifli/tools，导出SDK时需使SIFLI_SDK_TOOLS_PATH指向该实际目录；不需要为本版本重装工具。此次SDK实际HEAD为421126d9f，最终main.bin为699232字节；精确输入和下载产物哈希见evidence/pet/build_v0.3.0_metadata_20261023.json。

下载目录：project/build_sf32lb52-lchspi-ulp_hcpu，按CodeKit生成的完整配置包含bootloader、分区表ftab和main。CodeKit Download即可，uart_download.bat是可选入口，不要求额外重复下载。T1 v0.2.0旧固件及完整下载配置已在课程资料目录outputs/t1-firmware-before-pet-20261023中按原字节保留。

## 接下来的板端验证

这一步需要学生亲自操作，按工程AGENTS.md的物理验证规则取得真实证据后才登记通过：

1. 使用实际板卡串口和完整配置Download本版，监视1000000 baud，保存v0.3.0启动和`[pet:SELFTEST]`输出。预期是七组及all PASS；此处是预期，不是已取得日志。
2. 观察首页、Feed/Pet/Rest/Wake的实际显示和反馈，保存一张照片或视频及操作原始日志。继续按TC-112/113测试连点、冷却和休息恢复。
3. 修改数值后再次启动，核对恢复70/60及清醒状态；这是会话版重置测试，不是存档通过。

收到本版本真实证据前，TC-110、TC-112、TC-113、TC-115、TC-117维持未执行。若失败，保留版本、步骤和原始输出，修复后另记回归。
