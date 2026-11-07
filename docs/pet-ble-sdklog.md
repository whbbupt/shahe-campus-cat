# v0.12.3 蓝牙底层日志采集

## 已确认的情况

用户在v0.12.2真实查询中得到wait-ready、callbacks/power/queued/handled全为0、loops4767、fault0。说明应用任务在循环、SDK启用调用已返回，但应用没有收到就绪回调，尚未走到ShaheCat广播流程。不能把result=0解读为蓝牙已就绪，或直接判定手机/控制器故障。

本版只启用SDK统一日志、中断日志、来源线程名并关闭颜色控制码。保留蓝牙角色、广播/GATT、诊断命令和原小猫玩法；没有改RTC、存档或数值，也没有宣称无线已修复。

## 这次怎么操作

1. 沿用原工程CodeKit Download更新固件。工程 `D:/project/sifli/course_starter/project`，板型sf32lb52-lchspi-ulp，构建输出 `build_sf32lb52-lchspi-ulp_hcpu`。
2. 更新后打开串口监视器，1000000 baud；先确认状态页版本 `v0.12.3-blelog`。
3. 串口发送框输入 `reboot`，文本模式、行尾CRLF，发送一次，让新启用的底层日志从启动开始被采集。
4. 等15秒以上，发送 `pet_ble_status`（同样文本+CRLF）。
5. 把从TX reboot到状态查询结束的完整内容发来，包含所有SDK的I/W/E/D信息，不只复制pet:SAVE或STATUS。可以像上次一样粘贴为文本文件。

之前的版本没有记录这些内部输出，旧的状态查询无法补回，因此本次需要新版本的一次完整启动。先不需要反复扫描；若已经出现ShaheCat，也可以连接并回传实际情况。

## 如何看待结果

- stage=wait-ready：继续等内部就绪，实际原因看SDK启动信息。
- callbacks/power为0：目前没有应用收到的SDK回调；不等于硬件损坏。
- stage=advertising且adv_events>0、adv_result=0：SDK已报告启动广播，再由LightBlue发现/连接验证。
- recv=-2：消息等待超时，不能单凭这一项判断故障。

扫描名仍为ShaheCat-XXXXXX；信息/测试特征不变，若连接成功可按原说明写PING读PONG。本版未实现手机校时。

## 构建与软件检查

原构建成功，ROM **7,435,428 / 8,388,608字节**，剩余 **953,180字节（约0.91MiB）**，静态RAM169,444字节；比v0.12.2增加18,381字节。main.bin为7,367,140字节，SHA-256为 `7E461D753E601A54980783A165060B050926E2E8310CFAC78C894C21F00FB138`。

原模型/世界/存档/音频/LVGL及BLE协议、生产回调与worker四种主机模拟通过。编译配置确认日志启用、颜色关闭、原BLE角色保留；SDK实际启动函数已调用ulog_output，SDK日志适配对象引用ulog_voutput。未使用的bt_log_output函数会被链接器正常裁剪，没有为凑检查强制保留。

全量编译保留9条告警：7条原有告警及2次此前已出现的SDK lv_obj_swap潜在空指针提示；未改SDK或抑制告警。分区、构建骨架和原存档未变；旧v0.12.2完整15项产物已核验备份。

TC-217软件检查通过；TC-218待本版实际日志。原TC-214连接失败仍待定位；TC-216只读查询已有真实输出、自动15秒输出未回传。软件检查不能代替无线实测。
