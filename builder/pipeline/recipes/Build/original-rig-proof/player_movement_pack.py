import os
"""Original GTA skeleton audit and offscreen gait animation data, no installation."""
import contextlib,importlib.util,json
from pathlib import Path
import numpy as np
O=Path(__file__).parent
spec=importlib.util.spec_from_file_location('donor_probe',O/'probe.py');p=importlib.util.module_from_spec(spec)
with open(O/'audit-import.log','w') as log,contextlib.redirect_stdout(log):spec.loader.exec_module(p);p.build()
source=json.loads((p.R/'Build/movement-animation-source/move_p_m_one.json').read_text())
reports=[];output={}; body=[m for m in p.meshes if not m['gun']]
meta=[]
for m in body:
 meta.append(dict(name=m['name'],count=len(m['positions']),faces=m['faces'].tolist(),uv=m['uv'].tolist(),texture=m['texture']))
for clip in source['weapon_clips']:
 p.clip=clip;p.amap={a['hash']:a for a in clip['animations']};duration=clip['duration'];samples=[];roots=[];bone_error=0
 for t in np.linspace(0,duration,25):
  w=p.pose(t);roots.append(w[0,:3,3]);samples.append(w)
  assert np.isfinite(w).all()
  for i,parent in enumerate(p.parents):
   if parent>=0:bone_error=max(bone_error,abs(np.linalg.norm(w[i,:3,3]-w[parent,:3,3])-np.linalg.norm(p.rest[i,:3,3])))
 positions=np.array(samples)[:,:,:3,3];motion=np.linalg.norm(positions-positions[0],axis=2)
 reports.append(dict(clip=clip['name'],duration=duration,finite=True,max_joint_motion_m=float(motion.max()),root_displacement_m=float(np.linalg.norm(np.array(roots)[-1]-roots[0])),max_bone_length_change_m=float(bone_error),track_types=sorted({tr['track'] for a in clip['animations'] for tr in a['tracks']})))
 label=clip['name'].split('/')[-1].split('.')[0]
 if label not in ['idle','walk','run','sprint']:continue
 frames=[]
 for t in np.linspace(0,duration,120,endpoint=False):
  w=p.pose(float(t%duration));skin=np.array([w[i]@np.linalg.inv(p.restworld[i]) for i in range(len(p.names))]);row=[]
  for m in body:
   coords=np.column_stack((m['positions'],np.ones(len(m['positions']))));lookup=np.array([p.names.index(n) for n in m['bones']]);v=np.zeros((len(coords),3))
   for k in range(4):v+=np.einsum('nij,nj->ni',skin[lookup[m['joints'][:,k]]],coords)[:,:3]*m['weights'][:,k,None]
   row.append(v)
  frames.append(np.concatenate(row))
 output[label]=np.array(frames,dtype=np.float32);output[label+'_duration']=np.array(duration,dtype=np.float32)
np.savez_compressed(O/'player-movement-frames.npz',**output);(O/'movement-meshes.json').write_text(json.dumps(meta));(O/'movement-audit.json').write_text(json.dumps({'scope':'Original donor transform tracks only; offline, no ER physics or controller','clips':reports},indent=2));print(json.dumps(reports,indent=2))
