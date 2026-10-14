import os
from pathlib import Path
import hashlib,json,math,xml.etree.ElementTree as E
import numpy as np
from PIL import Image,ImageDraw,ImageChops
P=Path(__file__).resolve().parent;root=P.parents[1]
source=root/'polish-next/hud/donor/x64b.rpf/hud.ytd/weapons_all.dds'
raw=(root/'polish-v4/radar/hud-atlas.rgba').read_bytes();assert len(raw)==1024*1024*4
atlas=Image.frombytes('RGBA',(1024,1024),raw);art=Image.open(source).convert('RGBA');assert art.size==(512,512)
doc=E.parse(P/'hud.xml').getroot();lookup={}
for e in doc.iter('item'):
 for key in ['spriteId','shapeId']:
  if key in e.attrib:lookup[e.get(key)]=e
weapons=[('wheel_fist',-1569615261),('wheel_pistol',453432689),('wheel_smg',736523883),('wheel_shotgun',487013001),('wheel_rifle',-1074790547),('wheel_sniper',100416529),('wheel_gl',-1568386805),('wheel_grenade',-1813897027),('wheel_rpg',-1312131151)]
rects=[];x=2;y=256;row=0;review=Image.new('RGBA',(450,(len(weapons)+1)*85),(40,40,40,255));draw=ImageDraw.Draw(review)
for idx,(name,hash) in enumerate(weapons):
 placed=None
 for tags in doc.findall('.//subTags'):
  kids=list(tags)
  for i,e in enumerate(kids):
   if e.get('type')=='FrameLabelTag' and e.get('name')=='INT'+str(hash):
    placed=next((c for c in kids[i+1:] if c.get('type')=='PlaceObject2Tag'),None);break
  if placed is not None:break
 assert placed is not None,name
 sprite=lookup[placed.get('characterId')];child=sprite.find('./subTags/item[@type="PlaceObject2Tag"]');shape=lookup[child.get('characterId')]
 fill=shape.find('.//item[@type="FILLSTYLE"]');assert fill.get('bitmapId')=='2',(name,fill.attrib)
 matrix=fill.find('bitmapMatrix');sx=float(matrix.get('scaleX','1'));sy=float(matrix.get('scaleY','1'));r0=float(matrix.get('rotateSkew0','0'));r1=float(matrix.get('rotateSkew1','0'));tx=float(matrix.get('translateX','0'));ty=float(matrix.get('translateY','0'))
 inv=np.linalg.inv(np.array([[sx,r1],[r0,sy]]));bounds=shape.find('shapeBounds');lo=np.array([float(bounds.get('Xmin')),float(bounds.get('Ymin'))]);hi=np.array([float(bounds.get('Xmax')),float(bounds.get('Ymax'))]);pixels=10 # 2x Scaleform's authored pixel size (20 twips/pixel)
 w,hh=(int(math.ceil(v/pixels)) for v in hi-lo);matrixA=inv*pixels;offset=inv@(lo-np.array([tx,ty]));im=art.transform((w,hh),Image.Transform.AFFINE,tuple(matrixA[0])+ (offset[0],)+tuple(matrixA[1])+(offset[1],),resample=Image.Resampling.BILINEAR)
 points=[];px=py=0
 for e in shape.findall('.//shapeRecords/item'):
  typ=e.get('type')
  if typ=='StyleChangeRecord':
   if e.get('stateMoveTo')=='true':px=float(e.get('moveDeltaX','0'));py=float(e.get('moveDeltaY','0'));points.append(((px-lo[0])/pixels,(py-lo[1])/pixels))
  elif typ=='StraightEdgeRecord':px+=float(e.get('deltaX','0'));py+=float(e.get('deltaY','0'));points.append(((px-lo[0])/pixels,(py-lo[1])/pixels))
  else:assert typ=='EndShapeRecord',(name,typ)
 mask=Image.new('L',(w,hh));ImageDraw.Draw(mask).polygon(points,fill=255);im.putalpha(ImageChops.multiply(im.getchannel('A'),mask))
 if x+w+2>1024:x=2;y+=row+2;row=0
 assert y+hh+2<1024;atlas.paste(im,(x,y));slotmat=placed.find('matrix');scaleX=float(slotmat.get('scaleX','1'));scaleY=float(slotmat.get('scaleY','1'))
 rects.append(dict(name=name,x=x,y=y,w=w,h=hh,width=(hi[0]-lo[0])/20*scaleX,height=(hi[1]-lo[1])/20*scaleY,shape_id=shape.get('shapeId'),hash=hash));x+=w+2;row=max(row,hh)
 show=im.copy();show.thumbnail((275,75));review.alpha_composite(show,(165,idx*85+5));draw.text((5,idx*85+20),name,fill='white')
# Native ER addition: orthographic silhouette of the actual extracted Sword of
# Night and Flame, not a GTA weapon replacement or a hand-drawn approximation.
sword_source=root/'polish-v4.3/sword-prototype/sword-original.npz'
sword=np.load(sword_source);positions=np.concatenate([sword[f'mesh{i}_position'] for i in range(2)])
lo=positions.min(0);hi=positions.max(0);w=320;hh=int(math.ceil(w*(hi[0]-lo[0])/(hi[1]-lo[1])))+4
mask=Image.new('L',(w*4,hh*4));md=ImageDraw.Draw(mask);ratio=(w-4)/(hi[1]-lo[1])
for i in range(2):
 v=sword[f'mesh{i}_position'];xy=np.column_stack(((hi[1]-v[:,1])*ratio+2,(v[:,0]-lo[0])*ratio+2))*4
 for tri in sword[f'mesh{i}_triangles']:md.polygon([tuple(xy[j]) for j in tri],fill=255)
im=Image.new('RGBA',(w,hh),(255,255,255,0));im.putalpha(mask.resize((w,hh),Image.Resampling.LANCZOS))
x=2;y+=row+2;assert y+hh+2<1024;atlas.paste(im,(x,y));rects.append(dict(name='wheel_sword',x=x,y=y,w=w,h=hh,width=w/2,height=hh/2,source=str(sword_source),source_sha256=hashlib.sha256(sword_source.read_bytes()).hexdigest(),projection='native -Y right / X down; white silhouette'))
im.save(P/'sword-wheel-icon.png');show=im.copy();show.thumbnail((275,75));review.alpha_composite(show,(165,len(weapons)*85+5));draw.text((5,len(weapons)*85+20),'wheel_sword_ER',fill='white')
h=['#pragma once','// Original weapons_all pixels clipped/transformed by local hud.gfx shapes.','namespace gta_wheel_art {','inline constexpr gta_hud::Rect icons[]={']
for r in rects:h.append('{"%s",%d,%d,%d,%d},'%(r['name'],r['x'],r['y'],r['w'],r['h']))
h+=['};','inline constexpr float sizes[][2]={']
for r in rects:h.append('{%.6ff,%.6ff},'%(r['width'],r['height']))
h+=['};','inline unsigned index(unsigned weapon,bool rpg,bool sword=false){return weapon==0&&sword?9:weapon==6&&rpg?8:weapon<8?weapon:0;}','inline const gta_hud::Rect& icon(unsigned weapon,bool rpg,bool sword=false){return icons[index(weapon,rpg,sword)];}','}']
(P/'wheel_atlas.h').write_text('\n'.join(h)+'\n');atlas.save(P/'hud-atlas.png');(P/'hud-atlas.rgba').write_bytes(atlas.tobytes());review.save(P/'wheel-icons-review.png')
(P/'wheel-art.json').write_text(json.dumps({'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'shape_source':str(P/'hud.xml'),'atlas_sha256':hashlib.sha256(atlas.tobytes()).hexdigest(),'original_hud_pixels_preserved':atlas.tobytes()[:1024*256*4]==raw[:1024*256*4],'rects':rects},indent=2))
print('Original GTA wheel: 9 HUD shape masks/transforms plus original ER sword silhouette; existing HUD pixels preserved.')
