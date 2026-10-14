import os
"""ER weapon placement follows WepAbsorpPosParam and the original c0000 dummy.
Row-vector DSAnimStudio formulation transposed to this renderer's column matrices.
"""
from pathlib import Path
import numpy as np,json
from soulstruct.flver import FLVER
P=Path(__file__).parents[2];R=P.parent;O=Path(__file__).parent
f=FLVER.from_path(R/'Assets/ER/chr/c0000.flver')
def Rx(a):
 c,s=np.cos(a),np.sin(a);return np.array([[1,0,0],[0,c,-s],[0,s,c]])
def Ry(a):
 c,s=np.cos(a),np.sin(a);return np.array([[c,0,s],[0,1,0],[-s,0,c]])
def Rz(a):
 c,s=np.cos(a),np.sin(a);return np.array([[c,-s,0],[s,c,0],[0,0,1]])
fw=[]
for b in f.bones:
 m=np.eye(4);x,y,z=b.rotate;m[:3,:3]=Ry(y)@Rz(z)@Rx(x)@np.diag(list(b.scale));m[:3,3]=list(b.translate)
 if b.parent_bone_index>=0:m=fw[b.parent_bone_index]@m
 fw.append(m)
fw=np.array(fw)
def extract(dummy_id):
 d=next(d for d in f.dummies if d.reference_id==dummy_id);z=-np.array(list(d.forward));z/=np.linalg.norm(z);up=np.array(list(d.upward))if d.use_upward_vector else np.array([0,1.,0]);x=np.cross(up,z);x/=np.linalg.norm(x);y=np.cross(z,x)
 ref=np.eye(4);ref[:3,:3]=np.column_stack([x,y,z]);ref[:3,3]=list(d.translate)
 if d.parent_bone_index>=0:ref=fw[d.parent_bone_index]@ref
 # ER right hand: dummy @Rx(pi) @Ry(pi) @Rx(pi) == dummy @Ry(pi).
 local=np.linalg.inv(fw[d.attach_bone_index])@ref@np.diag([-1.,1.,-1.,1.])
 return local,dict(dummy_id=dummy_id,attach_bone=f.bones[d.attach_bone_index].name,parent_bone=f.bones[d.parent_bone_index].name,local_socket=local.tolist(),reference_weapon_world=fw[d.attach_bone_index].tolist(),reference_dummy_world=ref.tolist())
if __name__=='__main__':
 for i in[18,20,24]:
  m,rep=extract(i);(O/f'socket-{i}.json').write_text(json.dumps(rep,indent=2));print(i,m.round(4).tolist())
