import os
"""Repair sparse weapon carry layers over full original donor gait; no installation."""
import argparse,importlib.util,json,struct,xml.etree.ElementTree as ET
from pathlib import Path
import numpy as np
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']); O=Path(__file__).parent
spec=importlib.util.spec_from_file_location('motion',R/'PlayerRenderer/rifle-next/armed_motion.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,default=R/'PlayerRenderer/rifle-stage/arsenal-joints.bin');ap.add_argument('--output',type=Path,default=O/'arsenal-joints.bin');args=ap.parse_args()
 b=args.input.read_bytes();magic,version,nb,nv,nc,hand=struct.unpack_from('<6I',b);assert magic==0x414a4e54 and version==1
 at=24+nb*4+nb*64+nv*(12+16+16);prefix=b[:at];clips=[]
 for _ in range(nc):
  slot,action,nf,flags,duration=struct.unpack_from('<4If',b,at);at+=20
  t=np.frombuffer(b,'<f4',nf*nb*3,at).reshape(nf,nb,3).copy();at+=t.nbytes
  q=np.frombuffer(b,'<f4',nf*nb*4,at).reshape(nf,nb,4).copy();at+=q.nbytes
  clips.append(dict(slot=slot,action=action,nf=nf,flags=flags,duration=duration,t=t,q=q))
 assert at==len(b)
 bones=ET.parse(next((R/'Assets/GTA-Franklin').rglob('player_one.yft.xml'))).getroot().findall('.//Skeleton/Bones/Item');tags={int(x.find('Tag').get('value')):i for i,x in enumerate(bones)};names=[x.findtext('Name') for x in bones]
 reports=[]
 for c in clips:
  if c['slot'] not in (1,7) or c['action'] not in (5,6):continue
  source=R/'Build/movement-animation-source'/('weapons@pistol@.json' if c['slot']==1 else 'weapons@projectile@.json')
  cl=mod.Clip(json.loads(source.read_text()),'walk' if c['action']==5 else 'run',tags)
  generic=next(x for x in clips if x['slot']==4 and x['action']==c['action'])
  original_duration=c['duration'];c.update(nf=generic['nf'],duration=generic['duration'],t=generic['t'].copy(),q=generic['q'].copy())
  cycles=max(1,round(c['duration']/cl.duration));indices={j for j,k in cl.coverage}
  assert all(not any(n in names[j] for n in ('Pelvis','Thigh','Calf','Foot','Toe','ROOT','Spine','UpperArm')) for j in indices)
  for frame in range(c['nf']):
   ph=frame/(c['nf']-1);overlay_time=(ph*cycles%1)*cl.duration
   cl.apply(overlay_time,c['t'][frame],c['q'][frame])
  untouched=[j for j in range(nb) if j not in indices]
  assert np.array_equal(c['t'][:,untouched],generic['t'][:,untouched])
  assert np.array_equal(c['q'][:,untouched],generic['q'][:,untouched])
  assert np.isfinite(c['t']).all() and np.isfinite(c['q']).all()
  assert np.max(np.abs(np.linalg.norm(c['q'],axis=2)-1))<1e-5
  legs=[j for j,n in enumerate(names) if any(s in n for s in ('Thigh','Calf','Foot'))]
  legmotion=float(np.linalg.norm(c['q'][:,legs]-c['q'][0,legs],axis=2).max());assert legmotion>.1
  reports.append(dict(slot=c['slot'],action=c['action'],old_duration=original_duration,new_duration=c['duration'],frames=c['nf'],overlay_cycles=cycles,overlay_bones=[names[j] for j in sorted(indices)],leg_quaternion_motion=legmotion,untouched_body_exact_generic=True))
 with args.output.open('wb') as f:
  f.write(prefix)
  for c in clips:f.write(struct.pack('<4If',c['slot'],c['action'],c['nf'],c['flags'],c['duration']));f.write(c['t'].astype('<f4').tobytes());f.write(c['q'].astype('<f4').tobytes())
 report=dict(input=str(args.input),output=str(args.output),clips_changed=reports,body_rest_and_skin_prefix_unchanged=True,other_clips_unchanged=True)
 args.output.with_suffix('.report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
