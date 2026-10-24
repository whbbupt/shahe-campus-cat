# MOMO AI生图角色（v0.3.2）

用户已明确认可source/idle.png对应的奶白橘猫原图，并于2026-10-24要求接入。原图由内置image_gen生成；closed/eat/sad是以该图为参考的3次内置生图编辑。没有把AI图记成学生手绘，也不声称编辑后的身体逐像素不变。三张派生图与原图毛色/姿势经AI查看一致；学生尚未单独确认派生表情或板端显示。

原图提示词：prompt-original.txt。3次编辑完整提示词：prompts-expressions.json。source/保留未经裁切、未经重编码的1254×1254 RGBA输出，哈希见conversion.json；runtime/是机械裁切透明边缘并等比缩放的目标图。

## 再生成固件资源

已有Node.js与sharp时，在工程根目录执行：

```text
node scripts/make_cat_assets.cjs
```

也可传`--sharp <本机已有sharp模块绝对路径>`。不需要再调用生图接口，不在板上安装Node或PNG解码库。脚本对所有猫图统一裁切left=160/top=62/width=1018/height=1118，再contain等比缩放为196×160，保留透明alpha；不会用几何图形重画猫。输出src/pet_cat_assets.c/.h中的RGB565颜色平面+A8平面。

闭眼图复用为眨眼、开心/摸摸与休息；摸摸加原爱心、休息加z z。4张整帧加原30×28爱心，共378840字节常量。保留旧assets/pet-cat中的SVG历史与爱心图。真实屏幕/触摸和运行时堆栈尚待学生实测。
