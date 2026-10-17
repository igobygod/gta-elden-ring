import os
from pathlib import Path
import sys,json,struct,re,xml.etree.ElementTree as E
import numpy as np
from PIL import Image
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).resolve().parent
sys.path.insert(0,str(R/'PlayerRenderer'));from fix_rifle_materials import bake_weapon_palette
sys.path.insert(0,str(R/'PlayerRenderer/rifle-next'));from armed_motion import Exporter,Clip
ex=Exporter(O);p=ex.p
# Filesystem enumeration differs between Windows and macOS. Prefer the high-detail
# diffuse dictionary and the normal palette over lower-detail copies with the same names.
def texture_rank(path):
 dictionary=path.parent.name.lower()
 quality=2 if dictionary.endswith('+hi.ytd') else 0 if re.search(r'_l[1-9](?:_|\.)',dictionary) else 1
 return quality,path.relative_to(R/'Assets').as_posix().lower()
texmap={x.stem.lower():x for x in sorted((R/'Assets').rglob('*.dds'),key=texture_rank)}
atlas=Image.open(R/'PlayerRenderer/rifle-stage/atlas.png').convert('RGBA');tile=0;cache={};reports=[]
for projectile,model in [(False,'w_lr_rpg_hi'),(True,'w_lr_rpg_rocket')]:
 xml=next((R/'Assets').rglob(model+'.ydr.xml'));root=E.parse(xml).getroot();bones=root.findall('Skeleton/Bones/Item')
 if bones:
  bn,bt,bp,bl=p.rig(bones);bw=p.worlds(bl,bp);grip=bw[bn.index('Gun_GripR') if 'Gun_GripR'in bn else bn.index('Gun_Main_Bone')][:3,3];muzzle=bw[bn.index('Gun_Muzzle')][:3,3]if 'Gun_Muzzle'in bn else np.zeros(3)
 else:grip=muzzle=np.zeros(3)
 attachment=np.eye(4)
 if not projectile:
  data=json.loads((O/'rpg-expanded.json').read_text());clip=Clip(data,'aim_med_loop',p.tagindex);t,q=ex.rest_t.copy(),ex.rest_q.copy();clip.apply(0,t,q);world=ex.world(t,q);rot=np.array([[0,-1,0],[1,0,0],[0,0,1.]]);weapon=np.eye(4);weapon[:3,:3]=rot;weapon[:3,3]=world[ex.hand,:3,3]-rot@grip;attachment=np.linalg.inv(world[ex.hand])@weapon
 pos=[];uv=[];ind=[];off=0;meshes=[]
 for gi,g in enumerate(root.findall('DrawableModelsHigh/Item/Geometries/Item')):
  rows=np.array([[float(v)for v in row.split()]for row in g.findtext('VertexBuffer/Data').strip().splitlines()]);fields={};start=0
  for c in g.find('VertexBuffer/Layout'):fields[c.tag]=rows[:,start:start+p.W[c.tag]];start+=p.W[c.tag]
  sh=root.findall('ShaderGroup/Shaders/Item')[int(g.find('ShaderIndex').get('value'))];dn=sh.findtext("Parameters/Item[@name='DiffuseSampler']/Name");pn=sh.findtext("Parameters/Item[@name='TextureSamplerDiffPal']/Name");bucket=int(sh.find('RenderBucket').get('value'));solid=bucket==0
  key=(dn,pn,solid)
  if key not in cache:
   image=Image.open(texmap[dn.lower()]).convert('RGBA')
   if pn:image=bake_weapon_palette(image,Image.open(texmap[pn.lower()]))
   elif solid:image.putalpha(255)
   x=2048+256*tile;y=0;assert x+256<=4096;assert atlas.crop((x,y,x+256,y+256)).getchannel('A').getextrema()==(0,0),'occupied atlas tile';atlas.paste(image.resize((256,256)),(x,y));cache[key]=(x,y);tile+=1
  x,y=cache[key];v=fields['Position'];u=(np.mod(fields.get('TexCoord0',np.zeros((len(v),2))),1)*256+[x,y])/4096;idx=np.fromstring(g.findtext('IndexBuffer/Data'),sep=' ',dtype=np.uint32);pos.extend(v);uv.extend(u)
  if solid:ind.extend(idx+off)
  off+=len(v);meshes.append({'diffuse':dn,'palette':pn,'opaque':solid,'vertices':len(v),'indices':len(idx)if solid else 0,'atlasTile':[x,y,256,256]})
 pos=np.asarray(pos,dtype='<f4');uv=np.asarray(uv,dtype='<f4');ind=np.asarray(ind,dtype='<u4');assert np.isfinite(pos).all()and np.isfinite(uv).all()and np.isfinite(attachment).all()and ind.max()<len(pos)
 raw_bounds=[pos.min(0).tolist(),pos.max(0).tolist()]
 if projectile:
  # Original rocket barrel/nose axis is -Y; runtime projectilebasis expects+X.
  convert=np.array([[0,-1,0],[1,0,0],[0,0,1]],dtype='<f4');pos=np.asarray(pos@convert.T,dtype='<f4');muzzle=convert@muzzle
 file=O/('rpg-projectile-mesh.bin'if projectile else'rpg-mesh.bin')
 with file.open('wb')as f:f.write(struct.pack('<4I',0x414D5348,1,1,4096));f.write(struct.pack('<4I',6,len(pos),len(ind),1));f.write(np.asarray(attachment,dtype='<f4').tobytes());f.write(np.asarray(muzzle,dtype='<f4').tobytes());f.write(pos.tobytes());f.write(uv.tobytes());f.write(ind.tobytes())
 reports.append({'source':str(xml),'file':file.name,'vertices':len(pos),'indices':len(ind),'grip':grip.tolist(),'muzzle':muzzle.tolist(),'attachment':attachment.tolist(),'bounds':[pos.min(0).tolist(),pos.max(0).tolist()],'rawBounds':raw_bounds,'projectileBasis':'source(x,y,z)->(-y,x,z)'if projectile else'original','meshes':meshes,'poseLimitation':'Original RPG drawable and grip, attachment calibrated against original weapons@heavy@rpg aim_med_loop.'if not projectile else'Original projectile rigidly rotated source-Y to runtime+X, identityattachment, originalindices/UVunchanged.'})
atlas.save(O/'atlas.png');(O/'atlas.rgba').write_bytes(atlas.tobytes());(O/'rpg-report.json').write_text(json.dumps(reports,indent=2));print(json.dumps(reports))
