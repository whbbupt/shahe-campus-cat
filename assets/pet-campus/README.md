# 沙河校园插画场景（v0.4.0）

三张背景由内置image_gen原创生成，非实景照片/精确建筑复刻，未将官网照片直接嵌入固件。用户明确选择沙河校区和校园行为范围；具体插画、布局与参数由AI制作。

- 官方视觉参考：[沙河校区图书馆](https://zsb.bupt.edu.cn/info/1044/1694.htm)，取窗景中弧形/竖向格栅等建筑元素。
- 官方视觉参考：[沙河智慧教室](https://www.bupt.edu.cn/info/1079/83975.htm)，取蓝色课桌座椅、明亮教室等元素。
- 宿舍与食堂是校园生活题材的原创场景，不声明对应某个真实房间或食堂档口。

source/保留未经重编码的3幅生成PNG；完整提示词为prompts.json。runtime/为354×216机械cover缩放结果；RGB565常量共458784字节。原图尺寸和哈希见conversion.json。

在已有Node.js/sharp的环境运行`node scripts/make_scene_assets.cjs`，或加`--sharp <本机已有模块绝对路径>`，会生成src/pet_scene_assets.c/.h。无须重新调用生图，板端也不加载Node或PNG解码器。猫的源图/表情沿用../pet-cat-ai，缩小为112×96，主线程按模型选择背景/表情。

真实页面为C/LVGL绘制的文字、按钮和图片图层；用户尚未在板上确认本版三场景、行为或内存/稳定性。
