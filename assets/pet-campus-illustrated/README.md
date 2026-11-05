# 插画校园局部样板 v0.7.1

来源：本次内置imagegen工具生成，2026-11-05；三幅源PNG原字节保留在source/，完整提示词分别为ground-prompt.txt、building-prompt.txt、tree-prompt.txt。参考style-comparison-reference.png的左侧插画风；右侧像素风未选。不是实测沙河校区建筑复刻。

地面按固定坐标要求生成，教学楼与树为真实透明PNG。make_campus_illustrated_assets.py只做裁切、缩放、切块与RGB565/RGB565A8转换；conversion.json记录来源/像素哈希和拼接检查。既有室内/猫不重绘。地面样板bounds=(280,-180,560,880)，原路网不挪；源图道路中心存在小幅画笔偏差，生产图必须核验猫脚底在铺装内。

原始building门中心偏离画布几何中心，转换后通过136px脚底锚点将台阶中心对齐world=(560,120)，不是强行用整图中心。树world=(583,410,180,250)，地面锚点y=660；阴影在地面层、树与猫影按脚底排序；遮住猫时用image_opa=50%淡化树，避免整控件opacity引入额外合成层。宿舍/食堂原接近线由同一铺装裁出的64×100纹理片复用，路线中心检测覆盖382个点。室外其余区域仍是旧矢量素材，样板不声称整个地图美术已完成。

重新转换：Python环境含Pillow，运行python scripts/make_campus_illustrated_assets.py。运行端无新解码器、文件系统或整图RAM缓冲。AI图仅是素材；实际生产LVGL渲染和板端验收单独记录。
