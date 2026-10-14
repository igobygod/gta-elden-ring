import os
from pathlib import Path
import sys,json,contextlib,struct
import numpy as np
from PIL import Image
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']); O=Path(sys.argv[1]) if len(sys.argv)>1 else R/'PlayerRenderer/rifle-stage';O.mkdir(exist_ok=True);sys.path.insert(0,str(R/'Build/original-rig-proof'));import probe as p
with (O/'asset.log').open('w') as log,contextlib.redirect_stdout(log):p.build()
body=[m for m in p.meshes if not m['gun']];guns=[m for m in p.meshes if m['gun']]; nv=sum(len(m['positions']) for m in body);ng=sum(len(m['positions']) for m in guns)
from fix_rifle_materials import prepare_rifle_materials
material_images, emit_indices, material_report=prepare_rifle_materials(guns)
(O/"materials.json").write_text(json.dumps(material_report,indent=2))
# Preserve the approved locomotion atlas, adding a separate lower half for guns/FX.
atlas=Image.new('RGBA',(4096,4096));atlas.paste(Image.frombytes('RGBA',(2048,2048),(R/'PlayerRenderer/atlas.rgba').read_bytes()),(0,0))
uv=[];indices=[];start=0
for i,m in enumerate(guns):
 col=i%8;row=4+i//8;atlas.paste(material_images[i].resize((512,512)),(col*512,row*512));v=m['uv'].copy();v[:,0]=(col+np.mod(v[:,0],1))/8;v[:,1]=(row+np.mod(v[:,1],1))/8;uv.extend(v)
 if emit_indices[i]:indices.extend((m['faces']+start).ravel())
 start+=len(v)
for name,col in [('muz_assault_flare',0),('bullet_tracer',1)]:atlas.paste(Image.open(R/'NativePort/combat-source/fx-assets'/f'{name}.png').convert('RGBA').resize((512,512)),(col*512,3072))
atlas.save(O/'atlas.png');(O/'atlas.rgba').write_bytes(atlas.tobytes())
# Mask by actual skeleton influence, preserving legs/pelvis and existing action clips.
upper=set()
for i,n in enumerate(p.names):
 j=i
 while j>=0:
  if p.names[j]=='SKEL_Spine_Root':upper.add(i);break
  j=p.parents[j]
masks=[];prepared=[]
for m in body:
 lookup=np.array([p.names.index(n) for n in m['bones']]);j=lookup[m['joints']];mask=sum(m['weights'][:,k]*np.isin(j[:,k],list(upper)) for k in range(4));masks.extend(mask);prepared.append((m,np.column_stack([m['positions'],np.ones(len(m['positions']))]),j))
# Calibrate weapon origin from its explicit grip marker to the right hand, forward along donor +Y.
gm=guns[0];grip=gm['bind'][gm['bones'].index('Gun_GripR')][:3,3];muzzle=gm['bind'][gm['bones'].index('Gun_Muzzle')][:3,3]
source=json.loads((R/'Build/rifle-animation-source/weapons_rifle_hi_.json').read_text());p.clip=next(c for c in source['weapon_clips'] if c['name']=='pack:/aim_med_loop.clip');p.amap={a['hash']:a for a in p.clip['animations']};aim=p.pose(0);hand=p.names.index('PH_R_Hand')
# Donor gun X forward, Z up. Aim clip faces donor +Y; map X->Y,Y->-X,Z->Z.
rot=np.array([[0,-1,0],[1,0,0],[0,0,1.]])
weapon=np.eye(4);weapon[:3,:3]=rot;weapon[:3,3]=aim[hand,:3,3]-rot@grip;attachment=np.linalg.inv(aim[hand])@weapon
clips=[];report=[]
for name,file,clipname in [('aim','weapons_rifle_hi_.json','aim_med_loop'),('fire','weapons_rifle_hi_.json','fire_med'),('reload','weapons_rifle_hi_assault_rifle_str.json','reload_aim')]:
 source=json.loads((R/'Build/rifle-animation-source'/file).read_text());p.clip=next(c for c in source['weapon_clips'] if c['name']==f'pack:/{clipname}.clip');p.amap={a['hash']:a for a in p.clip['animations']};duration=p.clip['duration'];nf=max(2,round(duration*30)+1);frames=[];tips=[]
 for t in np.linspace(0,duration,nf):
  w=p.pose(float(t));skin=w@np.linalg.inv(p.restworld);row=[]
  for m,coords,j in prepared:
   v=sum(np.einsum('nij,nj->ni',skin[j[:,k]],coords)[:,:3]*m['weights'][:,k,None] for k in range(4));row.append(v)
  gunworld=w[hand]@attachment
  for m in guns:row.append((gunworld@np.column_stack([m['positions'],np.ones(len(m['positions']))]).T).T[:,:3])
  v=np.concatenate(row);frames.append(v);tip=(gunworld@np.r_[muzzle,1])[:3];tips.append(tip)
 frames=np.array(frames,dtype='<f4');assert np.isfinite(frames).all();clips.append((nf,duration,frames,np.array(tips,dtype='<f4')));report.append(dict(name=name,frames=nf,duration=duration))
with (O/'rifle.bin').open('wb') as f:
 f.write(struct.pack('<6I',0x5249464c,1,nv,ng,len(indices),3));f.write(np.array(indices,dtype='<u4').tobytes());f.write(np.array(uv,dtype='<f4').tobytes());f.write(np.array(masks,dtype='<f4').tobytes())
 for nf,duration,frames,tips in clips:f.write(struct.pack('<If',nf,duration));f.write(frames.tobytes());f.write(tips.tobytes())
(O/'report.json').write_text(json.dumps(dict(body_vertices=nv,gun_vertices=ng,gun_indices=len(indices),clips=report,attachment=attachment.tolist(),limits=['Fixed medium aiming pose; pitch adjustment pending','Magazine uses static original mesh; magazine prop animation pending','Grip requires in-game visual confirmation']),indent=2));print(json.dumps(report))
