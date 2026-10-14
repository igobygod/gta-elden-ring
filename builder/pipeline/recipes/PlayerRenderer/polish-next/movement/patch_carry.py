import os
import struct,json
import numpy as np
from audit_gaits import *
O=Path(__file__).parent;b=(R/'PlayerRenderer/rifle-stage/arsenal-joints.bin').read_bytes();magic,ver,nb,nv,nc,hand=struct.unpack_from('<6I',b);at=24+nb*4+nb*64+nv*44;prefix=b[:at];clips=[]
for _ in range(nc):
 slot,act,nf,flags,dur=struct.unpack_from('<4If',b,at);at+=20;t=np.frombuffer(b,'<f4',nf*nb*3,at).reshape(nf,nb,3).copy();at+=t.nbytes;q=np.frombuffer(b,'<f4',nf*nb*4,at).reshape(nf,nb,4).copy();at+=q.nbytes;clips.append(dict(slot=slot,act=act,nf=nf,flags=flags,dur=dur,t=t,q=q))
assert at==len(b);upper=[]
for i in range(nb):
 j=i
 while j>=0:
  if j==45:upper.append(i);break
  j=parents[j]
reports=[]
for c in clips:
 if c['slot'] not in (1,7) or c['act'] not in (5,6):continue
 family=c['slot'];gait='walk' if c['act']==5 else 'run';data=load('move_weapon@pistol@generic.ycd.json' if family==1 else 'move_strafe@grenade.ycd.json');cl=motion.Clip(data,gait if family==1 else gait+'_fwd_0_loop',tags)
 indices={j for j,k in cl.coverage if j in upper};cycles=max(1,round(c['dur']/cl.duration));old_t=c['t'].copy();old_q=c['q'].copy()
 for f in range(c['nf']):
  ph=f/(c['nf']-1);tm=(ph*cycles%1)*cl.duration;t,q=rest_t.copy(),rest_q.copy();cl.apply(tm,t,q)
  if 45 in indices:q[45]=motion.qmul(motion.qmul(c['q'][f,0]*np.array([-1,-1,-1,1]),q[0]),q[45])
  for j in indices:c['q'][f,j]=q[j];c['t'][f,j]=t[j] if j!=45 else c['t'][f,j]
 untouched=[j for j in range(nb) if j not in indices];assert np.array_equal(c['q'][:,untouched],old_q[:,untouched]);assert np.array_equal(c['t'][:,untouched],old_t[:,untouched]);assert np.isfinite(c['q']).all();assert np.max(abs(np.linalg.norm(c['q'],axis=2)-1))<1e-5
 reports.append(dict(slot=family,action=c['act'],authored_upper_bones=[names[j] for j in sorted(indices)],cycles=cycles,lower_root_exact_unchanged=True,source=cl.clip['name']))
with (O/'arsenal-joints.bin').open('wb') as f:
 f.write(prefix)
 for c in clips:f.write(struct.pack('<4If',c['slot'],c['act'],c['nf'],c['flags'],c['dur']));f.write(c['t'].astype('<f4').tobytes());f.write(c['q'].astype('<f4').tobytes())
(O/'carry-patch-report.json').write_text(json.dumps(reports,indent=2));print(json.dumps(reports,indent=2))
