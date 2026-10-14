import os
import struct,json,math
from pathlib import Path
import numpy as np
from audit_gaits import *
O=Path(__file__).parent
def export():
 generic=load('move_strafe@generic.ycd.json');grenade=load('move_strafe@grenade.ycd.json');records=[];reports=[]
 angles=[0,45,90,135,180,-135,-90,-45]
 for family,data in ((0,generic),(7,grenade)):
  for gait in (0,1):
   for sector,angle in enumerate(angles):
    if family==7 and sector:continue
    prefix='walk' if gait==0 else 'run';which='fwd' if angle in (0,45,90,-45) else 'bwd'
    label=f'{prefix}_{which}_{angle}_loop'
    cl=motion.Clip(data,label,tags);clip=cl.clip;duration=cl.duration;am={a['hash']:a for a in clip['animations']};delta=None
    for layer in clip['layers']:
     anim=am[layer['hash']];tr={t['track']:np.array(t['samples']) for t in anim['tracks'] if t['bone']==0}
     if 5 not in tr:continue
     def sample(values,tm):
      f=np.clip(tm*(anim['frames']-1)/anim['duration'],0,len(values)-1);i=int(f);j=min(i+1,len(values)-1);return values[i]+(values[j]-values[i])*(f-i)
     delta=sample(tr[5],layer['end'])-sample(tr[5],layer['start']);rootq=sample(tr[6],layer['start']);canonical=qm(rootq).T@delta[:3]
    assert delta is not None;distance=float(np.linalg.norm(delta[:2]));sourceangle=float(np.rad2deg(np.arctan2(canonical[0],canonical[1])));assert abs((sourceangle-angle+180)%360-180)<10
    feet=[]
    for phase in np.arange(240)/240:
     t,q=rest_t.copy(),rest_q.copy();cl.apply(phase*duration,t,q);w=world(t,q);feet.append(w[names.index('SKEL_L_Foot'),:3,3])
    feet=np.array(feet);signal=feet[:,2];spectrum=np.abs(np.fft.rfft(signal-signal.mean()));cycles=int(np.argmax(spectrum[1:5])+1)
    peak=int(np.argmax(signal[:max(2,round(240/cycles))]));phaseoffset=peak/240*cycles
    nf=min(241,max(2,round(duration*30)+1));ts=[];qs=[]
    for tm in np.linspace(0,duration,nf):
     t,q=rest_t.copy(),rest_q.copy();cl.apply(float(tm),t,q);ts.append(t);qs.append(q)
    ts=np.asarray(ts,dtype='<f4');qs=np.asarray(qs,dtype='<f4');assert np.isfinite(ts).all() and np.isfinite(qs).all();assert np.max(abs(np.linalg.norm(qs,axis=2)-1))<1e-5
    assert any(names[i]=='SKEL_L_Thigh' for i,kind in cl.coverage);assert any(names[i]=='SKEL_R_Thigh' for i,kind in cl.coverage)
    records.append((family,gait,sector,nf,duration,cycles,distance,phaseoffset,ts,qs));reports.append(dict(family=family,gait=gait,sector=sector,name=label,angle_degrees=angle,root_motion_canonical_degrees=sourceangle,duration=duration,cycles=cycles,distance=distance,cycle_distance=distance/cycles,source_speed=distance/duration,phase_offset_strides=phaseoffset,frames=nf,coverage=len(cl.coverage)))
    print(json.dumps(reports[-1]),flush=True)
 with (O/'armed-directional.bin').open('wb') as f:
  f.write(struct.pack('<4I',0x41444952,1,len(names),len(records)))
  for family,gait,sector,nf,dur,cycles,distance,offset,t,q in records:
   f.write(struct.pack('<4I4f',family,gait,sector,nf,dur,cycles,distance,offset));f.write(t.tobytes());f.write(q.tobytes())
 (O/'directional-report.json').write_text(json.dumps(dict(schema='ADIR1 <4I magic,version,bones,clips>; each<4I4f family,gait,sector,frames,duration,cycles,distance,phaseOffsetStrides>; TRS translations then quaternions',clips=reports,limits=['Original 0/1 local tracks; source root-motion5/6 used for timing and direction only, never applied to ER position','Stride count from authored left-foot-height dominant frequency; preserve full multi-stride clips','Grenade dedicated forward loops; directional side/back lower-body uses original generic strafe','No edits to approved unarmed/roll/jump/death packets']),indent=2))
if __name__=='__main__':export()
