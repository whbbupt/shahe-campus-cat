/* Convert the approved AI PNGs to LVGL v9 ROM images; no drawing/redesign here.
 * Run: node scripts/make_cat_assets.cjs [--sharp PATH_TO_SHARP]
 * Sources and prompts: assets/pet-cat-ai/. Legacy SVGs remain in assets/pet-cat/.
 */
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const arg = process.argv.indexOf('--sharp');
const sharp = require(arg < 0 ? 'sharp' : process.argv[arg + 1]);
const root = path.resolve(__dirname, '..');
const art = path.join(root, 'assets', 'pet-cat-ai');
const crop = {left:160, top:62, width:1018, height:1118};
const sprites = ['idle','closed','eat','sad'].map(name => ({
  name, source:path.join(art,'source',name+'.png'), width:196, height:160, crop
}));
sprites.push({name:'heart', source:path.join(root,'assets','pet-cat','cat-heart.png'), width:30,height:28});

(async () => {
  const code=['/* Generated from approved AI PNGs by scripts/make_cat_assets.cjs. */','#include "pet_cat_assets.h"'];
  const header=['#ifndef PET_CAT_ASSETS_H','#define PET_CAT_ASSETS_H','#include "lvgl.h"'];
  const records=[];
  let total=0;
  fs.mkdirSync(path.join(art,'runtime'),{recursive:true});
  for(const sprite of sprites){
    const {name,source,width,height}=sprite;
    const input=fs.readFileSync(source);
    const meta=await sharp(input).metadata();
    if(!meta.hasAlpha)throw Error(name+': alpha channel required');
    if(sprite.crop && (meta.width!==1254 || meta.height!==1254))throw Error(name+': expected unaltered 1254 square source');
    let pipeline=sharp(input);
    if(sprite.crop)pipeline=pipeline.extract(sprite.crop);
    pipeline=pipeline.resize(width,height,{fit:'contain',background:{r:0,g:0,b:0,alpha:0}}).ensureAlpha();
    await pipeline.clone().png().toFile(path.join(art,'runtime',name+'.png'));
    const {data,info}=await pipeline.raw().toBuffer({resolveWithObject:true});
    if(info.channels!==4)throw Error('Expected RGBA');
    const pixels=width*height;
    const bytes=Buffer.alloc(pixels*3);
    for(let i=0;i<pixels;i++){
      const color=((data[i*4]>>3)<<11)|((data[i*4+1]>>2)<<5)|(data[i*4+2]>>3);
      bytes.writeUInt16LE(color,i*2);
      bytes[pixels*2+i]=data[i*4+3];
    }
    const symbol='pet_cat_'+name;
    code.push(`static const LV_ATTRIBUTE_MEM_ALIGN uint8_t ${symbol}_data[] = {`);
    for(let i=0;i<bytes.length;i+=24)code.push('    '+Array.from(bytes.subarray(i,i+24),v=>'0x'+v.toString(16).padStart(2,'0')).join(',')+',');
    code.push('};',`const lv_image_dsc_t ${symbol} = {`,
      `    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8, .w = ${width}, .h = ${height}, .stride = ${width*2}},`,
      `    .data_size = sizeof(${symbol}_data), .data = ${symbol}_data`, '};');
    header.push(`extern const lv_image_dsc_t ${symbol};`);
    records.push({name,source:path.relative(root,source).replaceAll('\\','/'),source_width:meta.width,source_height:meta.height,
      source_sha256:crypto.createHash('sha256').update(input).digest('hex'),crop:sprite.crop||null,width,height,image_data_bytes:bytes.length,
      image_data_sha256:crypto.createHash('sha256').update(bytes).digest('hex')});
    total+=bytes.length;
  }
  header.push('#endif');
  fs.writeFileSync(path.join(root,'src','pet_cat_assets.c'),code.join('\n')+'\n');
  fs.writeFileSync(path.join(root,'src','pet_cat_assets.h'),header.join('\n')+'\n');
  fs.writeFileSync(path.join(art,'conversion.json'),JSON.stringify({method:'Mechanical shared transparent-margin crop, contain resize, RGB565 color plane + A8; no creative redraw',total_image_data_bytes:total,sprites:records},null,2)+'\n');
  console.log('Converted four AI cat sprites and heart; image data = '+total+' bytes.');
})().catch(e=>{console.error(e);process.exit(1);});
