# v0.12.2 BLE启动诊断：只需查询一次状态

本版针对v0.12.1小猫界面正常、LightBlue未发现设备而准备。现有反馈不能定位无线根因，本版增加阶段与事件诊断，不把发布或编译成功当作无线已修复。原校园、声音、饱食/心情、存档和PING/PONG协议保持原行为，尚未接入RTC校时。

## 操作

1. 沿用CodeKit Download更新当前工程 `D:/project/sifli/course_starter/project` 的构建产物，板型sf32lb52-lchspi-ulp。输出目录仍为 `project/build_sf32lb52-lchspi-ulp_hcpu`。
2. 更新后打开1000000 baud串口。状态页版本应为 `v0.12.2-blediag`；查询输出也会包含版本号。
3. 发送框输入 `pet_ble_status`，文本模式、行尾CRLF，发送一次。
4. 把所有 `[pet:BLE:STATUS]` 开头的内容原样发来。它会输出三行，可能被串口工具拆成多段RX，保留全部片段即可。查询只读，不重启、不改变宠物/存档/RTC。
5. 如果LightBlue已经能看到 `ShaheCat-…`，也可以连接。读取信息特征确认0.12.2-blediag，再按原BLE测试说明写PING读PONG；将实际结果回传。

不必为查询再重启。开机约15秒也会自动输出一组同类状态；错过可随时发命令查询。此自动输出由主线程发出，独立于BLE工作任务。

如果出现命令不存在，先核对版本是否仍为0.12.1；请回传原提示，不反复重刷。若查询完全没有返回，同样保留串口原文。

## 输出字段

- `stage`：queue/thread/stack-call/wait-ready/gatt/name/adv-init/adv-start/advertising/connected/error，表示代码到达的阶段。
- `result`：最近阶段的结果；SDK返回码保留。诊断用-100表示GATT未注册，-101表示事件队列故障；初始化内存/线程错误保留RT-Thread返回码。
- `callbacks/power/queued/handled/loops`：SDK回调数、就绪事件数、已入队数、处理数和工作循环数。
- `last_cb/last_handled`：最后收到/处理的事件ID。
- `recv`：最近队列接收返回值，-2可表示等待超时，不能单凭它认定蓝牙故障。
- `adv_events/adv_result/fault`：广播启动事件数、原始广播结果、队列故障锁定状态。

`advertising`阶段来自SDK成功启动事件；仍需手机发现/连接才能确认无线链路。`BLE-TEST protocol PASS`只验证RAM协议。

## 软件结果与容量

生产worker循环的四种主机模拟通过：成功就绪/广播、未就绪、广播失败、队列故障。模拟SDK调用尚未返回时调用独立报告入口，验证15秒仅输出一次及查询不改变快照、不调用SDK写操作。原生产回调、协议边界与模型/世界/存档/音频/真实LVGL回归通过，主机结果不替代实机。

原构建成功：ROM **7,417,047 / 8,388,608字节**，剩 **971,561字节（约0.93MiB）**，静态RAM169,088字节；比v0.12.1增加1499字节。main.bin为7,354,884字节，SHA-256为 `1B4892298C380327B784E934225B83B8DB59B3E7BBA910C1DD404FB13BF1FBC3`。最终7条告警与v0.12.1一致，没有新增。

未改工程配置、SDK、构建骨架、存档格式和分区；下载仍含bootloader/main/ftab三个镜像。原v0.12.1完整15项产物已按SHA独立备份，原交付ZIP保留。

TC-215软件通过；TC-216本版诊断输出尚未实测；原TC-214仍为已尝试、未通过待定位，不覆盖其失败历史。
