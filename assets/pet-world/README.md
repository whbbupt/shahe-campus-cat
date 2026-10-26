# v0.5.0 场景资源

这些原图来自用户已确认的网页v1.4设计，PNG字节保留在source；房间和猫最初由内置image_gen生成，完整提示词分别保存在room-prompts.json、eight-directions-prompt.txt和activity-poses-prompt.txt。design-contract.json保存移植时的角色裁切、锚点、场景坐标和默认规则。

make_world_assets.cjs只做固件格式编译：三张390×450 RGB565背景、十九张72×76透明RGB565A8帧、三个按同一原背景轮廓剪出的椅背/立柱、六个原界面图标。睡姿18度与透明裁切在资源编译时处理，运行时不解码PNG。conversion.json保存实际源图和资源哈希，runtime中的PNG是编译结果检查图。

make_world_fonts.py使用SDK v2.5.1附带SourceHanSansSC-Normal.otf生成Pet UI CJK 16/20像素位图子集，保持独立派生名，许可见FONT-LICENSE.txt，源字体哈希/使用字符见font-conversion.json。不复制系统商业字体或修改SDK字体配置。

资源常量在ROM；具体固件容量、动态内存、实际触摸和帧率分别由构建记录与真实板卡验证给出。旧assets/pet-campus及pet-cat-ai保留为历史。
