# MOMO AI生图角色（当前v0.4.0校园版）

source/保留用户于2026-10-24明确认可的idle原图和以它为参考的closed/eat/sad三次内置image_gen编辑，原字节不变。原图提示词为prompt-original.txt，完整编辑提示词为prompts-expressions.json。派生表情并非学生逐项指定；不声称编辑结果逐像素相同或已经实测。

当前runtime/由统一透明边裁切（left160/top62/width1018/height1118）后等比contain缩放为112×96，保留alpha，以满足用户缩小猫、让校园场景成为主体的要求。4张RGB565A8与原30×28爱心共131544字节；精确源图/输出参数见conversion.json。旧v0.3.2的196×160资源和378840字节数据保留在Git历史232d4d9。

已有Node.js/sharp时运行`node scripts/make_cat_assets.cjs`，或传`--sharp <本机已有sharp模块绝对路径>`。脚本机械裁切/缩放并生成src/pet_cat_assets.c/.h，颜色平面为RGB565，随后A8透明平面；不会重画猫或调用生图。闭眼图复用为眨眼、开心/摸摸和休息，爱心及z z仍由LVGL图层显示。

旧SVG保留于../pet-cat，当前背景与生成来源见../pet-campus。运行时完全离线，重启重置；板上显示/触摸/动态堆栈尚待真实证据。
