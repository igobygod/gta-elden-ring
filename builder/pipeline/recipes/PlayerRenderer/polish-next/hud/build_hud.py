import os
from PIL import Image,ImageFont,ImageDraw
from pathlib import Path
import json,hashlib
P=Path(__file__).resolve().parent
atlas=Image.new('RGBA',(1024,1024)); records=[];glyphs=[]
x=y=2;row=0

def pack(name,im,source):
 global x,y,row
 im=im.convert('RGBA');w,h=im.size
 if x+w+2>1024:x=2;y+=row+2;row=0
 assert y+h+2<=1024,(name,y,h)
 atlas.paste(im,(x,y));r={'name':name,'x':x,'y':y,'w':w,'h':h,'source':str(source)};records.append(r)
 x+=w+2;row=max(row,h);return r
fontpath=P.parent/'death-ui/exported-fonts/9_$Font2_Chalet-LondonNineteenSixty.ttf'
font=ImageFont.truetype(str(fontpath),32)
for c in range(32,127):
 s=chr(c);b=font.getbbox(s,anchor='ls');w=max(1,b[2]-b[0]);h=max(1,b[3]-b[1]);im=Image.new('RGBA',(w+4,h+4));ImageDraw.Draw(im).text((2-b[0],2-b[1]),s,font=font,anchor='ls',fill=(255,255,255,255))
 r=pack('glyph_'+str(c),im,fontpath)
 glyphs.append({**r,'code':c,'advance':float(font.getlength(s)),'bearingX':b[0]-2,'bearingY':b[1]-2})
# Rasterized directly by JPEXS from original Scaleform vector shapes, no redraw.
for name,cid in [('player_arrow',512),('player_arrow_stroke',508),('north',461),('enemy_blip',309),('health_trough',254),('health_fill',260),('weapon_sniper',358),('weapon_smg',360),('weapon_shotgun',362),('weapon_launcher',364),('weapon_pistol',366),('weapon_grenade',380),('weapon_rifle',384)]:
 src=P/'minimap-png'/f'{cid}.png';pack(name,Image.open(src),src)
for name in ['barglow02','barglowend','barglowmiddle','health_hit','bordertexture']:
 src=P/'donor/x64b.rpf/minimap.ytd'/f'{name}.dds';pack(name,Image.open(src),src)
# Bordertexture is an authored repeat texture, not a complete rectangular radar frame.
raw=atlas.tobytes();(P/'hud-atlas.rgba').write_bytes(raw);atlas.save(P/'hud-atlas.png')
manifest={'width':1024,'height':1024,'format':'RGBA8 straight alpha','fontSize':32,'fontAscent':font.getmetrics()[0],'fontDescent':font.getmetrics()[1],'glyphAnchor':'baseline; destination top-left = baseline + bearingX,bearingY; advance is actual source font advance','rects':records,'glyphs':glyphs,'limitations':['Enemy blip is original blip_white_16x16 vector primitive recolored by renderer for enemy status.','weapon_* are original GTA minimap weapon symbols, not wheel artwork.','bordertexture is original repeating radar border material; exact original radar frame layout is not recreated.','No GTA terrain used for Elden Ring radar.','HUD labels and positions supplied by renderer, not falsely claimed original gameplay layout.'],'atlasSHA256':hashlib.sha256(raw).hexdigest(),'fontSHA256':hashlib.sha256(fontpath.read_bytes()).hexdigest()}
(P/'hud-manifest.json').write_text(json.dumps(manifest,indent=2))
h=['#pragma once','#include <cstring>','namespace gta_hud {','inline constexpr int AtlasWidth=1024,AtlasHeight=1024;','inline constexpr float FontSize=32;','struct Rect { const char* name; int x,y,w,h; };','struct Glyph { Rect rect; float advance,bearingX,bearingY; };','inline constexpr Rect Rects[]={']
for r in records:h.append('{"%s",%d,%d,%d,%d},'%(r['name'],r['x'],r['y'],r['w'],r['h']))
h+=['};','inline constexpr Glyph Glyphs[95]={']
for g in glyphs:h.append('{{"glyph_%d",%d,%d,%d,%d},%.6ff,%.6ff,%.6ff},'%(g['code'],g['x'],g['y'],g['w'],g['h'],g['advance'],g['bearingX'],g['bearingY']))
h+=['};','inline const Rect* find(const char* name) { for(const auto& r:Rects)if(std::strcmp(name,r.name)==0)return &r;return nullptr;}','inline const Glyph* glyph(unsigned char c) {return c>=32&&c<=126?&Glyphs[c-32]:nullptr;}','}']
(P/'hud_atlas.h').write_text('\n'.join(h)+'\n')
print(json.dumps({'atlas':str(P/'hud-atlas.rgba'),'bytes':len(raw),'rects':len(records),'sha256':manifest['atlasSHA256']}))
