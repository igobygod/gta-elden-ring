import os
from pathlib import Path
import sys,json,struct,xml.etree.ElementTree as ET
import numpy as np
sys.path.insert(0,str(Path('rifle-next').resolve()))
from armed_motion import Clip
out=Path('car-drive');nodes=ET.parse('../Assets/GTA-Franklin/x64v.rpf/player_one.yft/player_one.yft.xml').findall('.//Skeleton/Bones/Item')
tags={int(b.find('Tag').get('value')):i for i,b in enumerate(nodes)}
t=np.array([[float(b.find('Translation').get(k)) for k in 'xyz'] for b in nodes]);q=np.array([[float(b.find('Rotation').get(k)) for k in 'xyzw'] for b in nodes])
choices=[('base','sit'),('base','steer_no_lean'),('enter_exit','get_in'),('enter_exit','get_out')]
with (out/'driver.bin').open('wb') as f:
 f.write(struct.pack('<4I',0x44524956,1,len(nodes),len(choices)))
 for action,(file,name) in enumerate(choices):
  data=json.loads((out/(file+'.json')).read_text());amap={a['hash']:a for a in data['animations']}
  for c in data['weapon_clips']:
   if 'animations' not in c:c['animations']=[amap[h] for h in c['animation_refs']]
  clip=Clip(data,name,tags);frames=int(np.ceil(clip.duration*30))+1;ts=[];qs=[]
  for time in np.linspace(0,clip.duration,frames):
   a=t.copy();b=q.copy();clip.apply(float(time),a,b);ts.append(a);qs.append(b)
  ts=np.array(ts);qs=np.array(qs);assert np.isfinite(ts).all() and np.isfinite(qs).all()
  assert abs(np.linalg.norm(qs,axis=-1)-1).max()<.01
  f.write(struct.pack('<If',frames,clip.duration));f.write(ts.astype('<f4').tobytes());f.write(qs.astype('<f4').tobytes())
  print(name,frames,clip.duration,'root start/end',ts[0,0],ts[-1,0],'pelvis',ts[0,1],ts[-1,1], 'coverage',len(clip.coverage))
print('driver.bin bytes',(out/'driver.bin').stat().st_size)
