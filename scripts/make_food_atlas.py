"""Convert the generated 3x4 food atlas to one native 192x256 RGB565 texture."""
from pathlib import Path
import hashlib
import json
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / 'assets/pet-food/meal-atlas-source.png'
img = Image.open(source).convert('RGB').resize((192, 256), Image.Resampling.LANCZOS)
data = bytearray()
for r, g, b in img.getdata():
    pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    data.extend((pixel & 255, pixel >> 8))
code = ['/* Generated food atlas; source/provenance in assets/pet-food. */',
        '#include "pet_food_assets.h"',
        'static const LV_ATTRIBUTE_MEM_ALIGN uint8_t food_pixels[] = {']
code += [','.join('0x%02x' % v for v in data[i:i+24]) + ',' for i in range(0, len(data), 24)]
code += ['};', 'const lv_image_dsc_t pet_food_atlas = {',
         '.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=192,.h=256,.stride=384},',
         '.data_size=sizeof(food_pixels),.data=food_pixels};']
(root/'src/pet_food_assets.c').write_text('\n'.join(code)+'\n', encoding='utf-8')
(root/'src/pet_food_assets.h').write_text('#ifndef PET_FOOD_ASSETS_H\n#define PET_FOOD_ASSETS_H\n#include "lvgl.h"\nextern const lv_image_dsc_t pet_food_atlas;\n#endif\n', encoding='utf-8')
(root/'assets/pet-food/conversion.json').write_text(json.dumps({
    'source': source.name, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'generation': 'OpenAI built-in imagegen, 2026-11-09',
    'grid': [3,4], 'tile_size': [64,64], 'rgb565_bytes': len(data),
    'ordering': 'cafeteria six, then delivery six, row-major'
}, indent=2)+'\n', encoding='utf-8')
print('Food atlas: 12 tiles, 98304 bytes RGB565')
