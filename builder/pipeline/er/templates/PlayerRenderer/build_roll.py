import os
from pathlib import Path
import numpy as np,sys,contextlib,json,struct
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=R/'PlayerRenderer';sys.path.insert(0,str(R/'Build/original-rig-proof'));import probe as p
with open(O/'roll-build.log','w') as f,contextlib.redirect_stdout(f):p.build()
e=np.load(O/'er-actual-roll.npz');en=list(e['names']);er=e['rest'];C=np.array([[1,0,0],[0,0,1],[0,1,0.]])
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
clips={};report=[]
for key in [k for k in e.files if k.isdigit() and 27000<=int(k)<=27137]:
 a=np.array([frame(ep) for ep in e[key]]);assert np.isfinite(a).all();clips[key]=a;report.append({'id':int(key),'frames':len(a),'duration':float(e[key+'_duration']),'bounds':[a.min((0,1)).tolist(),a.max((0,1)).tolist()]})
with open(O/'roll.bin','wb') as f:
 f.write(struct.pack('<III',0x524f4c4c,len(clips),len(a[0])))
 for key,a in clips.items():f.write(struct.pack('<IIf',int(key),len(a),float(e[key+'_duration'])));f.write(a.tobytes())
np.savez_compressed(O/'roll-preview.npz',**clips);(O/'roll-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
