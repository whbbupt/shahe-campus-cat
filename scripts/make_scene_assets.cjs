/* Mechanical conversion of original image_gen campus backgrounds to LVGL RGB565. */
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const arg=process.argv.indexOf('--sharp');
const sharp=require(arg<0?'sharp':process.argv[arg+1]);
const root=path.resolve(__dirname,'..'),art=path.join(root,'assets','pet-campus');
(async()=>{
 const code=['/* Generated from original campus backgrounds; see assets/pet-campus/. */','#include "pet_scene_assets.h"'];
 const header=['#ifndef PET_SCENE_ASSETS_H','#define PET_SCENE_ASSETS_H','#include "lvgl.h"'];
 const records=[]; fs.mkdirSync(path.join(art,'runtime'),{recursive:true});
 for(const name of ['dorm','classroom','canteen']){
  const file=path.join(art,'source',name+'.png'),source=fs.readFileSync(file);
  const width=354,height=216,pipeline=sharp(source).resize(width,height,{fit:'cover'}).removeAlpha();
  await pipeline.clone().png().toFile(path.join(art,'runtime',name+'.png'));
  const {data,info}=await pipeline.raw().toBuffer({resolveWithObject:true});
  if(info.channels!==3)throw Error('Expected RGB');
  const bytes=Buffer.alloc(width*height*2);
  for(let i=0;i<width*height;i++)bytes.writeUInt16LE(((data[i*3]>>3)<<11)|((data[i*3+1]>>2)<<5)|(data[i*3+2]>>3),i*2);
  const symbol='pet_scene_'+name;
  code.push(`static const LV_ATTRIBUTE_MEM_ALIGN uint8_t ${symbol}_data[] = {`);
  for(let i=0;i<bytes.length;i+=24)code.push('    '+Array.from(bytes.subarray(i,i+24),x=>'0x'+x.toString(16).padStart(2,'0')).join(',')+',');
  code.push('};',`const lv_image_dsc_t ${symbol} = {`,`    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = ${width}, .h = ${height}, .stride = ${width*2}},`,`    .data_size = sizeof(${symbol}_data), .data = ${symbol}_data`,'};');
  header.push(`extern const lv_image_dsc_t ${symbol};`);
  records.push({name,source_width:(await sharp(source).metadata()).width,source_height:(await sharp(source).metadata()).height,source_sha256:crypto.createHash('sha256').update(source).digest('hex'),width,height,image_data_bytes:bytes.length});
 }
 header.push('#endif');
 fs.writeFileSync(path.join(root,'src','pet_scene_assets.c'),code.join('\n')+'\n');
 fs.writeFileSync(path.join(root,'src','pet_scene_assets.h'),header.join('\n')+'\n');
 fs.writeFileSync(path.join(art,'conversion.json'),JSON.stringify({method:'cover resize, RGB565 little endian',total_image_data_bytes:458784,scenes:records},null,2)+'\n');
 console.log('Converted 3 campus backgrounds, 354x216 RGB565; 458784 bytes.');
})().catch(e=>{console.error(e);process.exit(1);});
