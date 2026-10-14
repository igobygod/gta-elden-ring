import os
import json,importlib.util,xml.etree.ElementTree as E
from pathlib import Path
import numpy as np
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).parent
spec=importlib.util.spec_from_file_location('motion',R/'PlayerRenderer/rifle-next/armed_motion.py');motion=importlib.util.module_from_spec(spec);spec.loader.exec_module(motion)
nodes=E.parse(next((R/'Assets/GTA-Franklin').rglob('player_one.yft.xml'))).getroot().findall('.//Skeleton/Bones/Item');names=[b.findtext('Name') for b in nodes];parents=[int(b.find('ParentIndex').get('value')) for b in nodes];tags={int(b.find('Tag').get('value')):i for i,b in enumerate(nodes)}
rest_t=np.array([[float(b.find('Translation').get(k)) for k in 'xyz'] for b in nodes]);rest_q=np.array([[float(b.find('Rotation').get(k)) for k in 'xyzw'] for b in nodes]);scales=np.array([[float(b.find('Scale').get(k)) for k in 'xyz'] for b in nodes])
def qm(q):
 x,y,z,w=q/np.linalg.norm(q);return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def world(t,q):
 result=[]
 for i in range(len(names)):
  m=np.eye(4);m[:3,:3]=qm(q[i])@np.diag(scales[i]);m[:3,3]=t[i];result.append(result[parents[i]]@m if parents[i]>=0 else m)
 return np.array(result)
def load(fname):
 d=json.load(open(O/fname));am={a['hash']:a for a in d['animations']}
 for c in d['weapon_clips']:
  if 'animations' not in c:c['animations']=[am[h] for h in c['animation_refs']]
  if not c['duration']:c['duration']=max((l['end']-l['start'])/l['rate'] for l in c['layers'])
 return d
if __name__=='__main__':
 d=load('move_strafe@generic.ycd.json');report=[]
 for c in d['weapon_clips']:
  if '_loop' not in c['name']:continue
  cl=motion.Clip(d,c['name'].split('/')[-1].split('.')[0],tags);feet=[]
  for ph in np.arange(240)/240:
   t,q=rest_t.copy(),rest_q.copy();cl.apply(ph*cl.duration,t,q);w=world(t,q);feet.append(w[names.index('SKEL_L_Foot'),:3,3])
  feet=np.array(feet);signal=feet[:,2];fourier=np.abs(np.fft.rfft(signal-signal.mean()));cycles=int(np.argmax(fourier[1:5])+1)
  print(c['name'],'cycles',cycles,'spectra',fourier[1:5].round(3).tolist());report.append(dict(clip=c['name'],cycles=cycles,spectra=fourier[1:5].tolist()))
 (O/'cycle-audit.json').write_text(json.dumps(report,indent=2))
