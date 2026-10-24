# MOMO奶白橘猫

原创可编辑SVG，用户于2026-10-24选择圆脸、短爪、奶白橘猫方向。cat-base含身体、静态鼻子与腮红；idle/blink/happy/eat/sad为透明五官叠层，heart为摸摸反馈。PNG用于美术预览；固件只使用src/pet_cat_assets.c的常量。

## 重新生成

已有Node.js和sharp时，在工程根目录执行：

```text
node scripts/make_cat_assets.cjs
```

sharp未安装于默认搜索位置时，用`--sharp <sharp模块绝对路径>`指向现有模块。本机使用Codex捆绑的sharp；固件不引入Node或sharp依赖。生成器内保留原始路径与调色，可直接修改后重新导出。

身体196×160，五种表情88×44，爱心30×28。共154680字节RGB565A8数据，颜色平面后接A8；按照所用SDK的LVGL v9头文件定义。内存/稳定性与触摸需实际板卡验证。
