# ADR-0016 iPhone系统CTS自动校时

2026-11-08。状态：用户已同意；v0.14.1软件与构建已完成，实际iPhone验证待TC-240。

备选：A设备连接后读取iPhone原生CTS，免开发独立App但依赖系统服务/配对兼容性；B独立手机App连接后自动写0004，行为可控但增加iOS开发与安装；C继续手动编码。用户询问自动方案，在明确A的流程、可能配对和需实测后回复“可以”，采用A，保留手动备用，不重复请求已给出的授权。

Apple配件指南列Current Time Service，本机SDK v2.5.1 bf0_ble_tipc.c和bf0_sibles.h提供服务发现/远端注册/读取与配对接口；这不是本机无线成功证据。原GAP外设角色保持，仅启用GATT客户端。数据回调不操作RTC/UI，使用原主线程校时/读回与格式7资格。自动连接与持续监听不纳入本增量。

参考：https://developer.apple.com/accessories/Accessory-Design-Guidelines.pdf 。具体实现/错误和板测见REQ-207、TC-237至240。
