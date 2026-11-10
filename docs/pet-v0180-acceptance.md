# 沙河校园小猫 v0.18.0 真机验收单

> 2026-11-10 已用 sftool 在 COM6 常规烧录并启动 v0.18.0：启动日志 70 项自检 PASS、真实 Flash 已写入格式 11；用户已回传图鉴 0/12、外卖后 1/12、宿舍主界面三张实机照片（见 `evidence/pet/previews/album-v0.18.0/REAL_PHOTOS.md`）。**仍缺**食堂菜点亮、复位保持、布置/配送页面、手机与安卓证据。这份单子列剩余动作与证据；没有证据的项不要勾选。

## 0. 文件与版本

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `main.bin` | 4,537,068 B | `14E1C14393C5AD505D16EADC54214347441D076D3218F92178086D5F3834DF54` |
| `bootloader.bin` | 58,496 B | `E59D70DB44367D524FBB46A8A014CE0092462E86959D101A8E2E00AD27BB5E21` |
| `ftab.bin` | 11,288 B | `40C75495CCDA74FA8B6990658A9DC5D8C14557CC544CC49D4FF10EF8A0BD594E` |
| `lcpu.bin` | 5,336 B | `AC060A33ED7E8B4C48A880B842245B9C063924106A6C8A546A9EECC76B6776F9` |

固件包：`build/shahe-campus-cat-v0.18.0-firmware.zip`（ZIP SHA-256 `BABD87D6A51A190054D67C53F376485356B7AE7F0D693B904C5170CF602389B1`）。

> 本机 `sftool.exe` 被应用控制策略拦截，CodeKit 的 Download 若也调用它，可能同样失败。请换一台不受该策略限制的电脑，或请管理员放行 sftool；不要关闭整机安全策略来绕过。

## 1. 烧录前

1. 备份旧固件和旧档；**不要全片擦除**。
2. 记录旧版本（状态页或启动日志），存 `evidence/pet/v0180_before_version.txt`。
3. 确认板型 `sf32lb52-lchspi-ulp`、USB-to-UART 线、实际 COM 口。

## 2. 烧录与启动

1. CodeKit 打开 `project/`，选择板型，常规 Download（main/bootloader/ftab/lcpu 一起）。
2. 复位后状态页应显示 **v0.18.0**。
3. 用只读串口记录器抓启动日志（端口按实际改，本次为 COM6）：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/capture_serial.ps1 -Port COM6 -Seconds 20
```

运行后按一次复位键，结束后把生成的 `evidence/pet/serial_v0180_*.txt` 留存。脚本不发送任何字节。

> **复位时屏幕不一定会熄灭或黑屏**：背光可能保持亮，面板短暂无数据后直接显示新画面。可靠判据是串口出现 `[pet] v0.18.0 boot ok` 和 `all PASS`，或状态页时间回到 `--:-- 待校时`、图鉴进度从存档重建。若画面完全冻结且串口没有新的 boot 日志，再拔 USB 重新上电。

**判据**：日志包含 `[pet] v0.18.0 boot ok`；没有新的 ERROR；`[pet:WORLD] all PASS`、`[pet:SAVE-TEST] all PASS`。

## 3. 图鉴（TC-270）

- [ ] 首页点“图鉴”，应看到 **3×4 网格、12 道菜一屏**，上两行食堂、下两行外卖；未解锁显示灰暗图和“未解锁”。
- [ ] 吃一道食堂菜、一道外卖，再打开图鉴：对应两格点亮、显示菜名，顶部 `已尝 2/12`（数字随实际次数）。
- [ ] 等状态页显示“已保存，重启可恢复”后复位，再打开图鉴：已点亮的菜仍在。
- [ ] 关闭图鉴后，饱食、心情、金币没有变化（图鉴只读）。

照片命名：`v0180_album_grid.jpg`、`v0180_album_after_eat.jpg`、`v0180_album_after_reboot.jpg`。

## 4. 布置与配送（TC-271 / TC-273）

- [ ] 宿舍点地面让猫走向房门，在行走途中点“布置”：不应弹出“布置宿舍”。
- [ ] 正常停在宿舍点“布置”：可以打开；睡觉时打开应显示“先叫醒”。
- [ ] 点外卖下单，再打开外卖页：不出现“向上滑动看更多”；应显示“订单进行中 · 菜品列表已收起”“配送中，可先在屋里活动”。
- [ ] 送达后再打开：显示“已送达，点门旁纸袋取餐”。

照片/录屏：`v0180_shop_walk.jpg`、`v0180_takeout_delivering.jpg`、`v0180_takeout_ready.jpg`。

## 5. 手机与存档（TC-272 / TC-268 / TC-269）

- [ ] 食堂坐下后，用手机读 0003 状态，`phase` 应为 `seated`（不是 idle）；离开座位恢复。
- [ ] 安卓手机安装 `shahe-clock.apk`，扫描并点选 `ShaheCat`，确认校时成功；记录 App 提示和手表时间。
- [ ] 旧格式 10 档升级到 11：先备份，再刷本版，确认钱包/家具/签到保留、图鉴从 0 开始；再做一次正常复位，确认图鉴保留。
- [ ] 做一次掉电/拔电测试（可选，但请记录条件）：恢复后应回到最近一次“已保存”快照，不重放动作、不重复发金币。

## 6. 回传后我会更新什么

- `docs/04-测试报告.md`：TC-260/268/269/270/271/272/273 的真实结果与证据路径；
- `manifest.yaml`：`physical_verification` 与对应 `test_results`；
- `docs/06`：关闭或保留风险；
- `docs/devlog/2026-11-10.md`：如当天补测，追加真实记录。

**不要**在没有照片/日志时勾选或口述“通过”；主机 124 项 PASS 不能替代板端结论。


