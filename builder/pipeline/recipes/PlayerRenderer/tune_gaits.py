import os
from pathlib import Path
import numpy as np,json,struct,shutil
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=R/'PlayerRenderer';P=R/'Build/original-rig-proof';n=np.load(P/'player-movement-frames.npz');meta=json.loads((P/'movement-meshes.json').read_text());offset=0;feet=[]
for m in meta:
 if m['name'].startswith('feet'):feet.extend(range(offset,offset+m['count']))
 offset+=m['count']
left=np.array(feet)[n['idle'][0,feet,0]>0];poses=[];dur=[];report=[]
for k,cycles in [('idle',1),('walk',4),('run',2),('sprint',4)]:
 a=n[k];length=120/cycles;phase=0
 if k!='idle':phase=int(np.argmax(a[:int(length),left,1].mean(1)))
 t=(np.arange(120)*length/120+phase)%length;i=np.floor(t).astype(int);j=(i+1)%int(length);u=(t-i)[:,None,None];out=a[i]*(1-u)+a[j]*u
 poses.append(out);dur.append(float(n[k+'_duration'])/cycles);report.append(dict(clip=k,cycle_duration=dur[-1],aligned_start_frame=phase))
raw=(O/'model.bin').read_bytes();magic,nv,ni,nf,nc=struct.unpack_from('<5I',raw);assert nf==120 and nc==4
start=20+4*ni+8*nv;out=raw[:start]+np.array(dur,dtype='<f4').tobytes()+np.array(poses,dtype='<f4').tobytes();assert len(out)==len(raw)
(O/'model-tuned.bin').write_bytes(out);(O/'gait-report.json').write_text(json.dumps(report,indent=2));print(report)
