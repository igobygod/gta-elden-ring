import os
from pathlib import Path
import sys,struct,json,numpy as np
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).resolve().parent
sys.path.insert(0,str(R/'PlayerRenderer/rifle-next'));from armed_motion import Exporter,Clip,qmix
sys.path.insert(0,str(R/'PlayerRenderer/polish-next/movement'));from audit_gaits import load
ex=Exporter(O);p=ex.p;data=load(O/'rpg.json');reload=load(O/'rpg_str.json');(O/'rpg-expanded.json').write_text(json.dumps(data));(O/'rpg_str-expanded.json').write_text(json.dumps(reload))
b=(R/'PlayerRenderer/rifle-stage/arsenal-joints.bin').read_bytes();magic,version,nb,nv,nc,hand=struct.unpack_from('<6I',b);assert nb==317 and nv==19200
prefix=24+nb*4+nb*16*4+nv*3*4+nv*4*4+nv*4*4;off=prefix;base={}
for j in range(nc):
 slot,aid,nf,flags,dur=struct.unpack_from('<4If',b,off);off+=20;t=np.frombuffer(b,'<f4',nf*nb*3,off).reshape(nf,nb,3);off+=t.nbytes;q=np.frombuffer(b,'<f4',nf*nb*4,off).reshape(nf,nb,4);off+=q.nbytes
 if slot==6:base[aid]=(dur,t,q)
assert off==len(b)
def sample(aid,phase):
 dur,t,q=base[aid];v=(phase%1)*(len(t)-1);i=int(v);j=min(i+1,len(t)-1);w=v-i;return (t[i]*(1-w)+t[j]*w).copy(),qmix(q[i],q[j],w).copy()
records=[];report=[];aim=Clip(data,'aim_med_loop',p.tagindex)
for aid,name,d in [(0,'idle',data),(1,'aim_med_loop',data),(2,'fire_med',data),(3,'reload_aim',reload),(5,'walk',data),(6,'run',data)]:
 cl=Clip(d,name,p.tagindex)
 if cl.duration<=0:cl.duration=max((x['end']-x['start'])/max(x['rate'],1e-9)for x in cl.clip['layers'])
 assert cl.duration>0;nf=max(2,round(cl.duration*30)+1);ts=[];qs=[]
 for tm in np.linspace(0,cl.duration,nf):
  if aid in [0,5,6]:t,q=sample(aid,tm/cl.duration)
  else:t,q=ex.rest_t.copy(),ex.rest_q.copy();ex.base_idle.apply(tm%ex.base_idle.duration,t,q);aim.apply(0,t,q)
  cl.apply(float(tm),t,q);ts.append(t);qs.append(q)
 ts=np.asarray(ts,dtype='<f4');qs=np.asarray(qs,dtype='<f4');assert np.isfinite(ts).all()and np.isfinite(qs).all();records.append((aid,nf,cl.duration,ts,qs));report.append({'action':aid,'name':name,'duration':cl.duration,'frames':nf,'source':d['source'],'trackBones':len(cl.coverage),'carryBase':'activeoriginalGLfullbodygait'if aid in[0,5,6]else 'ordinaryidleplusoriginalRPGaim'})
# Include provenfullbody sprint fallback unchanged.
dur,ts,qs=base[7];records.append((7,len(ts),dur,ts,qs));report.append({'action':7,'name':'sprint','source':'existingoriginalgenericriflefullbodyfallback'})
with(O/'rpg-joints.bin').open('wb')as f:
 f.write(struct.pack('<6I',magic,version,nb,nv,len(records),hand));f.write(b[24:prefix])
 for aid,nf,dur,ts,qs in records:f.write(struct.pack('<4If',6,aid,nf,0,dur));f.write(ts.tobytes());f.write(qs.tobytes())
(O/'rpg-joint-report.json').write_text(json.dumps({'bones':nb,'bodyVertices':nv,'metadataPrefixIdentical':True,'clips':report},indent=2));print('RPG localTRS clips',len(records),'bytes',(O/'rpg-joints.bin').stat().st_size)
