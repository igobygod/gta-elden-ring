import os
from pathlib import Path
import sys,struct,json,numpy as np
O=Path(__file__).parent;R=O.parents[2];sys.path.insert(0,str(R/'Build/original-rig-proof'));import probe as p
# Reuse only the quaternion function, without loading HKX banks or rebuilding anything.
import ast
source=ast.parse((O/'build_melee.py').read_text());node=next(x for x in source.body if isinstance(x,ast.FunctionDef)and x.name=='quat');ns={'np':np};exec(compile(ast.Module(body=[node],type_ignores=[]),str(O/'build_melee.py'),'exec'),ns)
quat=ns['quat'];nb=len(p.names);scales=np.linalg.norm(p.rest[:,:3,:3],axis=1)
with(R/'PlayerRenderer/rifle-stage/arsenal-joints.bin').open('rb')as f:hand=struct.unpack('<6I',f.read(24))[-1]
raw=(O/'melee-joints.bin').read_bytes();h=struct.unpack_from('<4I',raw);at=16;chunks=[]
for _ in range(h[3]):
 ident,nf,dur,flags=struct.unpack_from('<IIfI',raw,at);size=16+nf*nb*28+nf*64
 if ident!=100 and ident<90000000:chunks.append(raw[at:at+size])
 at+=size
assert at==len(raw)
base=json.loads((R/'Build/movement-animation-source/move_p_m_one.json').read_text());gait=json.loads((R/'PlayerRenderer/gait-report.json').read_text());manifest=[]
attachment=np.frombuffer((O/'sword-fallback-attachment.bin').read_bytes(),'<f4').reshape(4,4)
for k,row in enumerate(gait):
 label=row['clip'];p.clip=next(c for c in base['weapon_clips']if c['name'].split('/')[-1].split('.')[0]==label);p.amap={a['hash']:a for a in p.clip['animations']};cycles={'idle':1,'walk':4,'run':2,'sprint':4}[label];period=120/cycles;duration=row['cycle_duration'];ts=[];qs=[];ws=[];nf=121
 for phase in np.linspace(0,1,nf):
  source_frame=(phase*period+row['aligned_start_frame'])%period;tm=source_frame/120*p.clip['duration'];w=p.pose(float(tm));loc=np.empty_like(p.rest)
  for j in range(nb):loc[j]=np.linalg.inv(w[p.parents[j]])@w[j]if p.parents[j]>=0 else w[j]
  ts.append(loc[:,:3,3]);qs.append(np.array([quat(loc[j,:3,:3]/scales[j])for j in range(nb)]));ws.append(w[hand]@attachment)
 t=np.asarray(ts,dtype='<f4');q=np.asarray(qs,dtype='<f4');w=np.asarray(ws,dtype='<f4');ident=90000000+k;chunks.append(struct.pack('<IIfI',ident,nf,duration,1)+t.tobytes()+q.tobytes()+w.tobytes());manifest.append(dict(raw_id=ident,name=label,frames=nf,duration=duration,source=p.clip['name'],source_phase_frame=row['aligned_start_frame']))
assert len(chunks)<=64
with(O/'melee-joints.bin').open('wb')as f:f.write(struct.pack('<4I',0x4d4a4e54,1,nb,len(chunks)));f.writelines(chunks)
a=json.loads((O/'animation-report.json').read_text());a['clips']=[c for c in a['clips']if c['raw_id']!=100];a['gaits']=manifest;a['file_bytes']=(O/'melee-joints.bin').stat().st_size;(O/'animation-report.json').write_text(json.dumps(a,indent=2));print('Gaits appended, total',len(chunks),'bytes',a['file_bytes'])
