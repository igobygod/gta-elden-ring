import os
"""Native ER melee -> compact Franklin local T/Q and exact native R_Weapon anchors.
Only writes beside this file. No game/save/source mutation. One clip resident at a time.
"""
from pathlib import Path
import sys,json,struct,xml.etree.ElementTree as ET,hashlib,gc
import numpy as np
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).parent
sys.path.insert(0,str(R/'Source/Conversion'))
from er_animation_io import skeleton,AnimationHKX
from soulstruct.havok.core import HKX
from soulstruct.containers import Binder
s=skeleton();eb=s.skeleton.bones;en=[b.name for b in eb];ep=np.array([en.index(b.parent.name) if b.parent else -1 for b in eb])
def mat(t):
 m=np.eye(4);m[:3,:3]=np.asarray(t.rotation.to_matrix3().data);m[:3,3]=list(t.translation);return m
def worlds(loc,parents):
 w=np.empty_like(loc)
 for i in range(len(loc)):w[i]=w[parents[i]]@loc[i] if parents[i]>=0 else loc[i]
 return w
el=np.array([mat(b.get_reference_pose()) for b in eb]);er=worlds(el,ep)
raw=(R/'PlayerRenderer/rifle-stage/arsenal-joints.bin').read_bytes();magic,version,nb,nv,nc,hand=struct.unpack_from('<6I',raw);assert magic==0x414a4e54
parents=np.frombuffer(raw,'<i4',nb,24).copy();rest=np.frombuffer(raw,'<f4',nb*16,24+nb*4).reshape(nb,4,4).astype(float).copy();del raw
rw=worlds(rest,parents);scales=np.linalg.norm(rest[:,:3,:3],axis=1)
nodes=ET.parse(next((R/'Assets/GTA-Franklin').rglob('player_one.yft.xml'))).getroot().findall('.//Skeleton/Bones/Item');names=[n.findtext('Name') for n in nodes];assert len(names)==nb
C=np.array([[1,0,0],[0,0,1],[0,1,0.]])
D=np.diag([-1.,-1.,1.,1.]);A=np.eye(4);A[:3,:3]=C;A=D@A;A[2,3]=-.92871094
mp={'SKEL_Pelvis':'Pelvis','SKEL_Spine_Root':'Pelvis','SKEL_Spine0':'Spine','SKEL_Spine1':'Spine1','SKEL_Spine2':'Spine2','SKEL_Spine3':'Spine3','SKEL_Neck_1':'Neck','SKEL_Head':'Head'}
for side in 'LR':
 for b in ['Thigh','Calf','Foot','Toe0','Clavicle','UpperArm','Forearm','Hand']:mp[f'SKEL_{side}_{b}']=f'{side}_{b}'
 for f in range(5):
  for j in range(3):mp[f'SKEL_{side}_Finger{f}{j}']=f'{side}_Finger{f}'+('' if j==0 else str(j))
mp={names.index(k):en.index(v) for k,v in mp.items() if k in names and v in en}
children={}
for i,name in enumerate(names):
 child=next((name.replace(a,b) for a,b in [('Thigh','Calf'),('Calf','Foot'),('UpperArm','Forearm'),('Forearm','Hand')] if name.endswith(a)),None)
 if child in names and names.index(child) in mp:children[i]=names.index(child)
def align(a,b):
 a=a/np.linalg.norm(a);b=b/np.linalg.norm(b);v=np.cross(a,b);c=float(a@b)
 if c<-.99999:return np.eye(3)
 k=np.array([[0,-v[2],v[1]],[v[2],0,-v[0]],[-v[1],v[0],0]]);return np.eye(3)+k+k@k/(1+c)
def quat(m):
 # Symmetric eigen solve handles 180-degree rotations without trace instability.
 K=np.array([[m[0,0]-m[1,1]-m[2,2],m[0,1]+m[1,0],m[0,2]+m[2,0],m[2,1]-m[1,2]],
 [m[0,1]+m[1,0],m[1,1]-m[0,0]-m[2,2],m[1,2]+m[2,1],m[0,2]-m[2,0]],
 [m[0,2]+m[2,0],m[1,2]+m[2,1],m[2,2]-m[0,0]-m[1,1],m[1,0]-m[0,1]],
 [m[2,1]-m[1,2],m[0,2]-m[2,0],m[1,0]-m[0,1],m.trace()]])/3
 return np.linalg.eigh(K)[1][:,-1]
def retarget(ew):
 w=np.empty_like(rest)
 for i,name in enumerate(names):
  g=(w[parents[i]]@rest[i]) if parents[i]>=0 else rest[i].copy()
  if i in mp:
   ei=mp[i];g[:3,:3]=C@ew[ei,:3,:3]@er[ei,:3,:3].T@C.T@rw[i,:3,:3]
   if name in ('SKEL_Pelvis','SKEL_Spine_Root'):g[:3,3]=rw[i,:3,3]+C@(ew[ei,:3,3]-er[ei,:3,3])
   if i in children:
    child=children[i];direction=C@(ew[mp[child],:3,3]-ew[ei,:3,3]);axis=rest[child,:3,3]
    if np.linalg.norm(direction)>1e-6:g[:3,:3]=align(g[:3,:3]@axis,direction)@g[:3,:3]
  # Match native arm joint origins so the blade remains on native hitboxes.
  # Skin weights smoothly absorb the small ER/GTA arm length difference.
  if name in [f"SKEL_{side}_{joint}" for side in "LR" for joint in ("UpperArm","Forearm","Hand")]:
   g[:3,3]=C@ew[mp[i],:3,3]+np.array([0,0,-.92871094])
  w[i]=g
 w=D@w;local=np.empty_like(rest)
 for i in range(nb):local[i]=np.linalg.inv(w[parents[i]])@w[i] if parents[i]>=0 else w[i]
 t=local[:,:3,3].copy();q=np.array([quat(local[i,:3,:3]/scales[i]) for i in range(nb)])
 return t,q,w
families=[30000,30010,30020,30030,30040,30050,30200,30210,30300,30400,30500,30501,30505,30510,30515,30700,31030,31060,31070,31071,31072,31081,31082,31230,31260,31270,31271,31272,31281,31282]
# Original source identifiers are full bank IDs, never modulo one million.
requests=[(23,x) for x in families]+[(42,x) for x in families]+[(0,0),(0,100)]
source_paths=[O/'c0000_a2x.anibnd',R/'Build/native-animation-source/c0000_a4x.anibnd',R/'Build/native-animation-source/c0000_a00_lo.anibnd']
report=[];preview={};records=0
with (O/'melee-joints.bin').open('wb') as out:
 out.write(struct.pack('<4I',0x4d4a4e54,1,nb,0))
 for path in source_paths:
  binder=Binder.from_path(path); comp=HKX.from_bytes(next(e.get_uncompressed_data() for e in binder.entries if e.name.endswith('.compendium')))
  entries={e.name.rsplit('\\',1)[-1]:e for e in binder.entries}
  for bank,suffix in requests:
   name=f'a{bank:03d}_{suffix:06d}.hkx'
   if name not in entries:continue
   entry=entries[name];a=AnimationHKX.from_bytes(entry.get_uncompressed_data(),compendium=comp);ac=a.to_interleaved_hkx().animation_container;ac.load_interleaved_data();ids=list(ac.hkx_container.bindings[0].transformTrackToBoneIndices);duration=float(ac.hkx_container.animations[0].duration);frames=ac.interleaved_data
   # Interpolate only in runtime; 30Hz samples preserve native duration. No body vertex bake.
   nf=min(150,max(2,round(duration*30)+1));indices=np.rint(np.linspace(0,len(frames)-1,nf)).astype(int)
   ts=[];qs=[];anchors=[];hand_errors=[];rawid=bank*1000000+suffix
   for si in indices:
    loc=el.copy()
    for bi,tr in zip(ids,frames[si]):
     if bi>=0:loc[bi]=mat(tr)
    ew=worlds(loc,ep);t,q,w=retarget(ew);anchor=A@ew[en.index('R_Weapon')];ts.append(t);qs.append(q);anchors.append(anchor)
    hand_errors.append(float(np.linalg.norm(w[names.index('SKEL_R_Hand'),:3,3]-(A@ew[en.index('R_Hand')])[:3,3])))
    if bank==0 and suffix==0 and len(anchors)==1:
     # Attachment relative to PH_R_Hand for existing GTA locomotion fallback.
     fallback=np.linalg.inv(w[hand])@anchor
     (O/'sword-fallback-attachment.bin').write_bytes(fallback.astype('<f4').tobytes())
     (O/'sword-fallback-attachment.json').write_text(json.dumps(fallback.tolist(),indent=2))
   ts=np.asarray(ts,dtype='<f4');qs=np.asarray(qs,dtype='<f4');anchors=np.asarray(anchors,dtype='<f4');assert np.isfinite(ts).all() and np.isfinite(qs).all() and np.isfinite(anchors).all();assert np.max(np.abs(ts))<5;assert np.max(np.abs(np.linalg.norm(qs,axis=2)-1))<1e-5
   flags=1 if bank==0 else 0;out.write(struct.pack('<IIfI',rawid,nf,duration,flags));out.write(ts.tobytes());out.write(qs.tobytes());out.write(anchors.tobytes());records+=1
   row=dict(raw_id=rawid,source=name,source_archive=str(path),duration=duration,frames=nf,source_frames=len(frames),loop=bool(flags),native_hand_error_max_m=max(hand_errors),native_hand_error_mean_m=float(np.mean(hand_errors)));report.append(row);print(name,nf,'hand drift',round(max(hand_errors),3),flush=True)
   # Sparse compact validation snapshots; no all-frame body bakes.
   preview[str(rawid)+'_t']=ts[[0,nf//3,2*nf//3,-1]];preview[str(rawid)+'_q']=qs[[0,nf//3,2*nf//3,-1]];preview[str(rawid)+'_weapon']=anchors[[0,nf//3,2*nf//3,-1]]
   del frames,ac,a,ts,qs,anchors;gc.collect()
  del binder,comp,entries;gc.collect()
 out.seek(12);out.write(struct.pack('<I',records))
assert records<=64
np.savez_compressed(O/'pose-preview.npz',**preview)
(O/'animation-report.json').write_text(json.dumps({'bones':nb,'body_vertices':nv,'hand_index':hand,'skeleton_sha256':hashlib.sha256((R/'PlayerRenderer/rifle-stage/arsenal-joints.bin').read_bytes()).hexdigest(),'clips':report,'file_bytes':(O/'melee-joints.bin').stat().st_size},indent=2))
print('COMPLETED',records,'clips',(O/'melee-joints.bin').stat().st_size,'bytes')
