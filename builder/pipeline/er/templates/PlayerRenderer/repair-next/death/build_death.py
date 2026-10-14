import os
from pathlib import Path
import numpy as np,json,struct,types,xml.etree.ElementTree as ET
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).parent
raw=(R/'PlayerRenderer/rifle-stage/arsenal-joints.bin').read_bytes();_,_,nb,nv,nc,hand=struct.unpack_from('<6I',raw)
at=24;parents=np.frombuffer(raw,'<i4',nb,at);at+=nb*4
rest=np.frombuffer(raw,'<f4',nb*16,at).reshape(nb,4,4).astype(float);at+=nb*64
positions=np.frombuffer(raw,'<f4',nv*3,at).reshape(nv,3);at+=nv*12
joints=np.frombuffer(raw,'<u4',nv*4,at).reshape(nv,4);at+=nv*16
weights=np.frombuffer(raw,'<f4',nv*4,at).reshape(nv,4)
nodes=ET.parse(next((R/'Assets/GTA-Franklin').rglob('player_one.yft.xml'))).getroot().findall('.//Skeleton/Bones/Item');names=[n.findtext('Name') for n in nodes]
world=[]
for i,m in enumerate(rest):world.append(world[parents[i]]@m if parents[i]>=0 else m)
p=types.SimpleNamespace(names=names,parents=parents,rest=rest,restworld=np.array(world),meshes=[dict(gun=False,positions=positions,joints=joints,weights=weights,bones=names)])
e=np.load(O/'er-death-candidates.npz');en=list(e['names']);er=e['rest']
C=np.array([[1,0,0],[0,0,1],[0,1,0.]])
mp={'SKEL_Pelvis':'Pelvis','SKEL_Spine_Root':'Pelvis','SKEL_Spine0':'Spine','SKEL_Spine1':'Spine1','SKEL_Spine2':'Spine2','SKEL_Spine3':'Spine3','SKEL_Neck_1':'Neck','SKEL_Head':'Head'}
for side in 'LR':
 for b in ['Thigh','Calf','Foot','Toe0','Clavicle','UpperArm','Forearm','Hand']:mp[f'SKEL_{side}_{b}']=f'{side}_{b}'
 for f in range(5):
  for j in range(3):mp[f'SKEL_{side}_Finger{f}{j}']=f'{side}_Finger{f}'+('' if j==0 else str(j))
mp={k:v for k,v in mp.items() if k in p.names and v in en}
def align(a,b):
 a=a/np.linalg.norm(a);b=b/np.linalg.norm(b);v=np.cross(a,b);c=np.dot(a,b)
 if c<-.99999:return np.eye(3)
 k=np.array([[0,-v[2],v[1]],[v[2],0,-v[0]],[-v[1],v[0],0]]);return np.eye(3)+k+k@k/(1+c)
body=[m for m in p.meshes if not m['gun']];inv=np.linalg.inv(p.restworld);prepared=[]
for m in body:prepared.append((m,np.column_stack((m['positions'],np.ones(len(m['positions'])))),np.array([p.names.index(n) for n in m['bones']])))
def frame(ep):
 worlds=[]
 for i,name in enumerate(p.names):
  parent=worlds[p.parents[i]] if p.parents[i]>=0 else np.eye(4);g=parent@p.rest[i]
  if name in mp:
   ei=en.index(mp[name]);g[:3,:3]=C@ep[ei,:3,:3]@er[ei,:3,:3].T@C.T@p.restworld[i,:3,:3]
   if name in ('SKEL_Pelvis','SKEL_Spine_Root'):g[:3,3]=p.restworld[i,:3,3]+C@(ep[ei,:3,3]-er[ei,:3,3])
   child=next((name.replace(a,b) for a,b in [('Thigh','Calf'),('Calf','Foot'),('UpperArm','Forearm'),('Forearm','Hand')] if name.endswith(a)),None)
   if child in mp:
    direction=C@(ep[en.index(mp[child]),:3,3]-ep[ei,:3,3]);axis=p.rest[p.names.index(child),:3,3]
    if np.linalg.norm(direction)>1e-6:g[:3,:3]=align(g[:3,:3]@axis,direction)@g[:3,:3]
  worlds.append(g)
 skin=np.array(worlds)@inv;rows=[]
 for m,coords,lookup in prepared:
  v=np.zeros((len(coords),3))
  for k in range(4):v+=np.einsum('nij,nj->ni',skin[lookup[m['joints'][:,k]]],coords)[:,:3]*m['weights'][:,k,None]
  rows.append(v)
 return np.concatenate(rows).astype('<f4')

records=[];report=[];bounds=[]
for deathid in [17002+i*10+s for i in range(5) for s in (0,1)]+[18002+i*10+s for i in range(5) for s in (0,1)]:
 key=f'{deathid:06d}';
 if key not in e.files:print('MISSING physical source',deathid,flush=True);continue
 source=e[key];duration=float(e[key+'_duration']);nf=min(96,max(2,round(duration*(24 if deathid<18000 else 12))+1));indices=np.rint(np.linspace(0,len(source)-1,nf)).astype(int)
 a=np.array([frame(source[j]) for j in indices]);assert np.isfinite(a).all();assert a.shape==(nf,nv,3)
 head=en.index('Head');pelvis=en.index('Pelvis');head_start=float(source[0,head,1,3]);head_end=float(source[-1,head,1,3]);pelvis_end=float(source[-1,pelvis,1,3])
 if deathid<18000:assert head_start>1 and head_end<.5 and pelvis_end<.4
 else:assert head_end<.5 and pelvis_end<.4
 vertexmotion=float(np.linalg.norm(a[-1]-a[0],axis=1).max());assert np.abs(a).max()<5
 row=dict(id=deathid,frames=nf,duration=duration,source_frames=len(source),source_bank='Build/native-animation-source/c0000_a00_hi.anibnd',source_clip=f'a000_{key}.hkx',head_start_m=head_start,head_end_m=head_end,pelvis_end_m=pelvis_end,max_vertex_endpoint_motion_m=vertexmotion,bounds=[a.min((0,1)).tolist(),a.max((0,1)).tolist()]);report.append(row);records.append((deathid,duration,a));print(json.dumps(row),flush=True)
with (O/'death.bin').open('wb') as f:
 f.write(struct.pack('<III',0x524f4c4c,len(records),nv))
 for ident,duration,a in records:f.write(struct.pack('<IIf',ident,len(a),duration));f.write(a.astype('<f4').tobytes())
(O/'death-report.json').write_text(json.dumps(dict(schema='ROLL same as approved roll packet',retarget_function='exact build_roll.py frame+align copied; stage AJNT rest/body/weights replaces probe.build side effects',position_policy='No ER position mutation; same pelvis delta and orientation as approved native-roll converter',clips=report,file_bytes=(O/'death.bin').stat().st_size),indent=2))
np.savez_compressed(O/'preview.npz',**{str(ident):a[[0,len(a)//2,-1]] for ident,duration,a in records})
