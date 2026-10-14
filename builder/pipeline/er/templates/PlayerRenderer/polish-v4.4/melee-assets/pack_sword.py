import os
from pathlib import Path
import numpy as np,struct,json
from PIL import Image
O=Path(__file__).parent;src=O.parents[1]/'polish-v4.3/sword-prototype';a=np.load(src/'sword-original.npz');ps=[];uvs=[];idx=[];offset=0
for i in range(2):
 p=a[f'mesh{i}_position'].astype('<f4');uv=a[f'mesh{i}_uv_0'].astype('<f4');tris=a[f'mesh{i}_triangles'].astype('<u4')[:,[0,2,1]] # ER->GTA reflected anchor
 ps.append(p);uvs.append(uv);idx.append(tris+offset);offset+=len(p)
p=np.concatenate(ps);uv=np.concatenate(uvs);indices=np.concatenate(idx).flatten();attachment=np.eye(4,dtype='<f4');muzzle=np.zeros(3,dtype='<f4')
with(O/'sword-mesh.bin').open('wb')as f:
 f.write(struct.pack('<4I',0x414d5348,1,1,4096)) # Existing Rig::loadWeapon packet marker; texture independently1024.
 f.write(struct.pack('<4I',0,len(p),len(indices),0));f.write(attachment.tobytes());f.write(muzzle.tobytes());f.write(p.tobytes());f.write(uv.tobytes());f.write(indices.astype('<u4').tobytes())
im=Image.open(src/'WP_A_0223_a.png').convert('RGBA');assert im.size==(1024,1024);(O/'sword.rgba').write_bytes(im.tobytes());im.save(O/'sword.png')
report={'vertices':len(p),'indices':len(indices),'material':'original WP_A_0223_a albedo; metallic/crystal ER shader not reproduced','texture_dimensions':[1024,1024],'uv_range':[uv.min(0).tolist(),uv.max(0).tolist()],'coordinate_policy':'Unchanged original ER weapon coordinates; MJNT R_Weapon supplies correct conversion and origin; winding reversed for reflection'};(O/'sword-package.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
