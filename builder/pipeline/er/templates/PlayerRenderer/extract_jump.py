import os
from pathlib import Path
import sys,json,numpy as np
sys.path.insert(0,str(Path(os.environ['IGOBYGOD_BUILD_ROOT'])/'Source/Conversion'))
from er_animation_io import skeleton,AnimationHKX
from soulstruct.havok.core import HKX
from soulstruct.containers import Binder
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']); O=R/'PlayerRenderer';s=skeleton(); bones=s.skeleton.bones; names=[b.name for b in bones];parents=[names.index(b.parent.name) if b.parent else -1 for b in bones]
def mat(t):
 m=np.eye(4);m[:3,:3]=np.asarray(t.rotation.to_matrix3().data);m[:3,3]=list(t.translation);return m
def world(loc):
 a=[]
 for i,m in enumerate(loc):a.append(a[parents[i]]@m if parents[i]>=0 else m)
 return np.array(a)
rest=np.array([mat(b.get_reference_pose()) for b in bones]);out={'rest':world(rest),'names':np.array(names),'parents':np.array(parents)};b=Binder.from_path(R/'Build/native-animation-source/c0000_a00_hi.anibnd');comp=HKX.from_bytes(next(e.get_uncompressed_data() for e in b.entries if e.name.endswith('.compendium')))
for e in b.entries:
 name=e.name.rsplit('\\',1)[-1]
 if not any(name.startswith('a000_'+x) for x in ['202']) or not name.endswith('.hkx'):continue
 a=AnimationHKX.from_bytes(e.get_uncompressed_data(),compendium=comp); ac=a.to_interleaved_hkx().animation_container;ac.load_interleaved_data(); binding=ac.hkx_container.bindings[0];ids=list(binding.transformTrackToBoneIndices); frames=[]
 for f in ac.interleaved_data:
  loc=rest.copy()
  for idx,t in zip(ids,f):
   if idx>=0:loc[idx]=mat(t)
  frames.append(world(loc))
 key=name[5:11];out[key]=np.array(frames);out[key+'_duration']=np.array(ac.hkx_container.animations[0].duration);print(key,len(frames),out[key+'_duration'])
np.savez_compressed(O/'er-jump-source.npz',**out)
