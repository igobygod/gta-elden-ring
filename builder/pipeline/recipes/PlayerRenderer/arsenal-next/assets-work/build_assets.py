import os
from pathlib import Path
import sys,struct,json,importlib.util,xml.etree.ElementTree as ET
import numpy as np
from PIL import Image
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).parent
sys.path.insert(0,str(R/'PlayerRenderer'));from fix_rifle_materials import bake_weapon_palette
sys.path.insert(0,str(R/'PlayerRenderer/rifle-next'));from armed_motion import Exporter,Clip
ex=Exporter(O);p=ex.p
names=['unarmed','pistol','smg','shotgun','rifle','sniper','launcher','grenade']
models=['','w_pi_pistol_hi','w_sb_smg_hi','w_sg_pumpshotgun_hi','w_ar_assaultrifle_hi','w_sr_sniperrifle_hi','w_lr_grenadelauncher_hi','w_ex_grenadefrag_hi']
atlas=Image.open(R/'PlayerRenderer/rifle-stage/atlas.png').convert('RGBA');assert atlas.size==(4096,4096)
textures=list((R/'Assets').rglob('*.dds'))+list((O/'donor').rglob('*.dds'));texmap={x.stem.lower():x for x in textures}
# The shared decal tile is first allocated by the pistol. Use its own dictionary,
# rather than whichever unrelated weapon's identically named decal is enumerated last.
for texture in (R/'Assets/GTA-Franklin/x64e.rpf/w_pi_pistol.ytd').glob('w_at_weapondecals*.dds'):
 texmap[texture.stem.lower()]=texture
# Free rows 7 and 4-5 after existing rifle seven tiles; 256 tiles in lower half except FX row6 and rifle row4 first half.
slots=[(x,y) for y in [3584,3840,2560,2816,2304,2048] for x in range(0,4096,256) if not(y==2048 and x<3584) and not(y==2304 and x<3584)]
tilecache={};reports=[];records=[]
rot=np.array([[0,-1,0],[1,0,0],[0,0,1.]])
for slot,model in enumerate(models):
 if not model:records.append((slot,np.zeros((0,3)),np.zeros((0,2)),np.zeros(0,dtype=np.uint32),np.eye(4),np.zeros(3)));continue
 paths=list((O/'donor').rglob(model+'.ydr.xml'))+list((R/'Assets').rglob(model+'.ydr.xml'));xml=paths[0];root=ET.parse(xml).getroot();bn,bt,bp,bl=p.rig(root.findall('Skeleton/Bones/Item'));bw=p.worlds(bl,bp);grip=bw[bn.index('Gun_GripR') if 'Gun_GripR' in bn else bn.index('Gun_Main_Bone')][:3,3];muzzle=bw[bn.index('Gun_Muzzle')][:3,3] if 'Gun_Muzzle' in bn else grip
 # Anchor donor grip to current right palm; original aim category reference.
 source=R/'Build/movement-animation-source'/('weapons@pistol@.json' if slot==1 else 'weapons@heavy@grenade_launcher.json' if slot==6 else 'weapons@projectile@.json' if slot==7 else '../rifle-animation-source/weapons_rifle_hi_.json')
 data=json.loads(source.read_text());cl=Clip(data,'aim_m' if slot==7 else 'aim_med_loop',p.tagindex);t,q=ex.rest_t.copy(),ex.rest_q.copy();cl.apply(0,t,q);world=ex.world(t,q);weapon=np.eye(4);weapon[:3,:3]=rot;weapon[:3,3]=world[ex.hand,:3,3]-rot@grip;attachment=np.linalg.inv(world[ex.hand])@weapon
 pos=[];uv=[];ind=[];meshes=[];off=0
 for gi,g in enumerate(root.findall('DrawableModelsHigh/Item/Geometries/Item')):
  rows=np.array([[float(v) for v in row.split()] for row in g.findtext('VertexBuffer/Data').strip().splitlines()]);fields={};start=0
  for col in g.find('VertexBuffer/Layout'):fields[col.tag]=rows[:,start:start+p.W[col.tag]];start+=p.W[col.tag]
  v=fields['Position'];u=fields.get('TexCoord0',np.zeros((len(v),2)));faces=np.fromstring(g.findtext('IndexBuffer/Data'),sep=' ',dtype=np.uint32)
  shader=root.findall('ShaderGroup/Shaders/Item')[int(g.find('ShaderIndex').get('value'))];diff=shader.findtext("Parameters/Item[@name='DiffuseSampler']/Name");pal=shader.findtext("Parameters/Item[@name='TextureSamplerDiffPal']/Name");bucket=int(shader.find('RenderBucket').get('value'));hard=shader.find("Parameters/Item[@name='HardAlphaBlend']");solid=bucket==0 and not(hard is not None and float(hard.get('x','0'))>0)
  key=(diff,pal,solid)
  if key not in tilecache:
   image=Image.open(texmap[diff.lower()]).convert('RGBA')
   if pal:image=bake_weapon_palette(image,Image.open(texmap[pal.lower()]))
   elif solid:image.putalpha(255)
   x,y=slots[len(tilecache)];atlas.paste(image.resize((256,256)),(x,y));tilecache[key]=(x,y)
  x,y=tilecache[key];u=(np.mod(u,1)*256+np.array([x,y]))/4096
  pos.extend(v);uv.extend(u)
  if solid:ind.extend(faces+off)
  meshes.append(dict(geometry=gi,vertices=len(v),indices=int(len(faces)) if solid else 0,diffuse=diff,palette=pal,opaque=solid));off+=len(v)
 pos=np.array(pos,dtype='<f4');uv=np.array(uv,dtype='<f4');ind=np.array(ind,dtype='<u4');assert np.isfinite(pos).all() and np.isfinite(attachment).all() and ind.max()<len(pos)
 records.append((slot,pos,uv,ind,attachment,muzzle));reports.append(dict(slot=slot,name=names[slot],source=str(xml),vertices=len(pos),indices=len(ind),attachment=attachment.tolist(),muzzle=muzzle.tolist(),meshes=meshes,bounds=[pos.min(0).tolist(),pos.max(0).tolist()]))
with (O/'arsenal-mesh.bin').open('wb') as f:
 f.write(struct.pack('<4I',0x414d5348,1,8,4096))
 for slot,pos,uv,ind,att,muzzle in records:
  f.write(struct.pack('<4I',slot,len(pos),len(ind),int(len(pos)>0)));f.write(np.asarray(att,dtype='<f4').tobytes());f.write(np.asarray(muzzle,dtype='<f4').tobytes());f.write(pos.tobytes());f.write(uv.tobytes());f.write(ind.tobytes())

for fx,col in [('exp_grd_gren_core',2),('exp_grd_gren_core_pillar',3),('muz_smg_core',4)]:atlas.paste(Image.open(R/'NativePort/combat-source/fx-assets'/f'{fx}.png').convert('RGBA').resize((512,512)),(col*512,3072))
atlas.save(O/'atlas.png');(O/'atlas.rgba').write_bytes(atlas.tobytes());(O/'mesh-report.json').write_text(json.dumps(reports,indent=2))
# Body vertex order exactly probe/build_rifle; joints resolve original bone names.
positions=np.concatenate([m['positions'] for m in ex.body]).astype('<f4');joints=np.concatenate([np.array([p.names.index(n) for n in m['bones']])[m['joints']] for m in ex.body]).astype('<u4');weights=np.concatenate([m['weights'] for m in ex.body]).astype('<f4')
cliprecords=[];clipreports=[]
for slot in range(1,8):
 folder=R/'Build/movement-animation-source';generic=R/'Build/rifle-animation-source/weapons_rifle_hi_.json';reload=R/'Build/rifle-animation-source/weapons_rifle_hi_assault_rifle_str.json'
 if slot==1:generic=folder/'weapons@pistol@.json';reload=folder/'weapons@pistol@pistol_str.json'
 if slot==2:generic=O/'weapons@rifle@lo@.json';reload=O/'weapons@rifle@lo@smg_str.json'
 if slot==3:generic=O/'weapons@rifle@lo@pump.json';reload=O/'weapons@rifle@lo@pump_str.json'
 if slot==5:generic=O/'weapons@rifle@hi@sniper_rifle.json';reload=O/'weapons@rifle@hi@sniper_rifle_str.json'
 if slot==6:generic=folder/'weapons@heavy@grenade_launcher.json';reload=folder/'weapons@heavy@grenade_launcher_str.json'
 if slot==7:generic=folder/'weapons@projectile@.json'
 data=json.loads(generic.read_text());rd=json.loads(reload.read_text());available={c['name'] for c in data['weapon_clips']}
 wanted=[(1,'aim_m' if slot==7 else 'aim_med_loop',data),(2,'fire_med',data),(3,'reload_aim',rd)]
 base=json.loads((R/'Build/rifle-animation-source/weapons_rifle_hi_.json').read_text())
 wanted=[(aid,n,d if 'pack:/'+n+'.clip' in {c['name'] for c in d['weapon_clips']} else base) for aid,n,d in wanted]
 if slot==7:wanted=[(1,'aim_m',data),(4,'throw_m_fb_stand',data)]
 carry=json.loads((folder/'move_weapon@rifle@generic.json').read_text())
 for aid,n in [(0,'idle'),(5,'walk'),(6,'run'),(7,'sprint')]:wanted.append((aid,n,data if 'pack:/'+n+'.clip' in available else carry))
 for aid,n,d in wanted:
  if 'pack:/'+n+'.clip' not in {c['name'] for c in d['weapon_clips']}:continue
  cl=Clip(d,n,p.tagindex)
  if cl.duration<=0:cl.duration=max((layer['end']-layer['start'])/max(layer['rate'],1e-9) for layer in cl.clip['layers'])
  assert cl.duration>0,(slot,aid,n)
  nf=max(2,round(cl.duration*30)+1);ts=[];qs=[]
  aimbase=None
  if aid in (1,2,3,4):
   aimdata=json.loads((folder/('weapons@pistol@.json' if slot==1 else 'weapons@heavy@grenade_launcher.json' if slot==6 else 'weapons@projectile@.json') if slot in (1,6,7) else R/'Build/rifle-animation-source/weapons_rifle_hi_.json').read_text());aimbase=Clip(aimdata,'aim_m' if slot==7 else 'aim_med_loop',p.tagindex)
  for tm in np.linspace(0,cl.duration,nf):
   t,q=ex.rest_t.copy(),ex.rest_q.copy();ex.base_idle.apply(tm%ex.base_idle.duration,t,q)
   if aimbase is not None:aimbase.apply(0,t,q)
   cl.apply(float(tm),t,q);ts.append(t);qs.append(q)
  ts=np.asarray(ts,dtype='<f4');qs=np.asarray(qs,dtype='<f4');assert np.isfinite(ts).all() and np.isfinite(qs).all();cliprecords.append((slot,aid,nf,cl.duration,ts,qs));clipreports.append(dict(slot=slot,action=aid,name=n,frames=nf,duration=cl.duration,source=d.get('source'),track_bones=len(cl.coverage)))
with (O/'arsenal-joints.bin').open('wb') as f:
 f.write(struct.pack('<6I',0x414a4e54,1,len(p.names),ex.nv,len(cliprecords),ex.hand));f.write(np.asarray(p.parents,dtype='<i4').tobytes());f.write(np.asarray(p.rest,dtype='<f4').tobytes());f.write(positions.tobytes());f.write(joints.tobytes());f.write(weights.tobytes())
 for slot,aid,nf,dur,ts,qs in cliprecords:f.write(struct.pack('<4If',slot,aid,nf,0,dur));f.write(ts.tobytes());f.write(qs.tobytes())
(O/'joint-report.json').write_text(json.dumps(dict(body_vertices=ex.nv,bones=len(p.names),upper_joint_indices=sorted(ex.upper),root_index=0,pelvis_index=1,spine_root_index=ex.spine_root,bind_identity_max_error=float(np.abs(ex.p.restworld@ex.inv_rest-np.eye(4)).max()),skin_rest_max_error=float(np.abs(ex.skin(ex.p.restworld)[0][:ex.nv]-positions).max()),bone_names=p.names,hand=ex.hand,clips=clipreports,limits=['Dedicated SMG/pump/sniper reload clips; aim/fire fall back original common rifle where subtype only supplies overlays','Only local position/quaternion tracks; IK/graph channels excluded','Carry sprint for pistol/launcher/grenade uses original generic rifle fallback','Static weapon meshes preserve vertices; translucent decal indices omitted','No active stage modifications']),indent=2))
print(json.dumps(dict(meshes=len(records),mesh_bytes=(O/'arsenal-mesh.bin').stat().st_size,joint_bytes=(O/'arsenal-joints.bin').stat().st_size,clips=len(cliprecords),body_vertices=ex.nv,bones=len(p.names),upper_joint_indices=sorted(ex.upper),root_index=0,pelvis_index=1,spine_root_index=ex.spine_root,bind_identity_max_error=float(np.abs(ex.p.restworld@ex.inv_rest-np.eye(4)).max()),skin_rest_max_error=float(np.abs(ex.skin(ex.p.restworld)[0][:ex.nv]-positions).max()))))
