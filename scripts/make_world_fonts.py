"""Rasterize only UI glyphs from the SDK's OFL SourceHanSansSC; native LVGL 4bpp."""
from pathlib import Path
import argparse, hashlib, json, re, shutil
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];art=root/'assets/pet-world';art.mkdir(parents=True,exist_ok=True)
source=a.sdk/'external/lvgl_v9/scripts/built_in_font/SourceHanSansSC-Normal.otf'
license_file=a.sdk/'external/lvgl_v9/scripts/built_in_font/font_license/SourceHanSansSC/LICENSE.txt'
chars=set(chr(i) for i in range(32,127))
for file in [root/'src/pet_ui.c',root/'src/pet_world.c']:
    for c in file.read_text(encoding='utf-8'):
        if ord(c)>127:chars.add(c)
cmap=TTFont(source).getBestCmap();missing=[c for c in chars if ord(c) not in cmap]
if missing:raise RuntimeError('Source font lacks glyphs: '+repr(missing))
chars=sorted(chars,key=ord);code=['/* Pet UI CJK derived bitmap subset, SIL OFL 1.1; see assets/pet-world/FONT-LICENSE.txt. */','#include "pet_world_font.h"'];header=['#ifndef PET_WORLD_FONT_H','#define PET_WORLD_FONT_H','#include "lvgl.h"'];metrics=[]
for size in [16,20]:
    font=ImageFont.truetype(str(source),size);data=bytearray();desc=['{0}']
    for c in chars:
        x0,y0,x1,y1=font.getbbox(c,anchor='ls');w,h=x1-x0,y1-y0
        offset=len(data)
        if w and h:
            image=Image.new('L',(w,h));ImageDraw.Draw(image).text((-x0,-y0),c,font=font,fill=255,anchor='ls')
            pixels=list(image.tobytes());pixels=[(v+8)//17 if v<248 else 15 for v in pixels]
            for i in range(0,len(pixels),2):data.append((pixels[i]<<4)|(pixels[i+1] if i+1<len(pixels) else 0))
        desc.append('{.bitmap_index=%d,.adv_w=%d,.box_w=%d,.box_h=%d,.ofs_x=%d,.ofs_y=%d}'%(offset,round(font.getlength(c)*16),w,h,x0,-y1))
    prefix='pet_font_'+str(size)
    code.append('static const uint8_t '+prefix+'_bitmap[]={')
    for i in range(0,len(data),24):code.append(','.join(str(x) for x in data[i:i+24])+',')
    code.extend(['};','static const lv_font_fmt_txt_glyph_dsc_t '+prefix+'_glyphs[]={'+','.join(desc)+'};','static const uint16_t '+prefix+'_unicode[]={'+','.join(str(ord(c)-32) for c in chars)+'};','static const lv_font_fmt_txt_cmap_t '+prefix+'_cmap[]={ {.range_start=32,.range_length='+str(ord(chars[-1])-31)+',.glyph_id_start=1,.unicode_list='+prefix+'_unicode,.list_length='+str(len(chars))+',.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY} };','static const lv_font_fmt_txt_dsc_t '+prefix+'_data={.glyph_bitmap='+prefix+'_bitmap,.glyph_dsc='+prefix+'_glyphs,.cmaps='+prefix+'_cmap,.cmap_num=1,.bpp=4,.bitmap_format=0,.stride=0};','const lv_font_t '+prefix+'={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height='+str(size+6)+',.base_line=5,.dsc=&'+prefix+'_data};'])
    header.append('extern const lv_font_t '+prefix+';');metrics.append({'size':size,'glyphs':len(chars),'bitmap_bytes':len(data)})
header.append('#endif');(root/'src/pet_world_font.c').write_text('\n'.join(code)+'\n',encoding='utf-8');(root/'src/pet_world_font.h').write_text('\n'.join(header)+'\n',encoding='utf-8')
shutil.copy2(license_file,art/'FONT-LICENSE.txt')
(art/'font-conversion.json').write_text(json.dumps({'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'license':'SIL OFL 1.1; derived bitmap name Pet UI CJK','characters':''.join(chars),'fonts':metrics},ensure_ascii=False,indent=2),encoding='utf-8')
print('Compiled UI glyphs:',metrics)
