import os
"""Read-only original Sword of Night and Flame extraction -> staged NPZ/DDS/PNG."""
from pathlib import Path
import json,hashlib,inspect,io,struct
import numpy as np
from PIL import Image
from soulstruct.flver import FLVER
from soulstruct.containers.tpf import TPF
P=Path(__file__).parent
f=FLVER.from_path(P/'WP_A_0223.flver'); t=TPF.from_path(P/'WP_A_0223.tpf')
arrays={};report={'weapon_id':2140000,'model_id':223,'source_sha256':hashlib.sha256((P/'wp_a_0223.partsbnd.dcx').read_bytes()).hexdigest(),'space':'Original ER weapon-local coordinates; no guessed grip transform applied.','meshes':[],'textures':[]}
for tex in t.textures:
 d=P/(tex.stem+'.dds');d.write_bytes(tex.data)
 preview=bytearray(tex.data)
 if preview[84:88]==b'DX10':
  fmt=struct.unpack_from('<I',preview,128)[0]
  if fmt in (72,75,78,99):struct.pack_into('<I',preview,128,fmt-1)
 im=Image.open(io.BytesIO(preview));im.convert('RGBA').save(d.with_suffix('.png'));report['textures'].append({'name':tex.stem,'size':im.size,'sha256':hashlib.sha256(tex.data).hexdigest()})
for i,m in enumerate(f.meshes):
 v=m.vertex_arrays[0].array
 for name in v.dtype.names:arrays[f'mesh{i}_{name}']=v[name]
 fs=next(x for x in m.face_sets if int(x.flags)==0)
 triangles=fs.triangulate(uses_0xffff_separators=len(v)<65536,include_degenerate_faces=False)
 arrays[f'mesh{i}_triangles']=triangles
 report['meshes'].append({'index':i,'name':m.material.name,'vertices':len(v),'triangles':len(triangles),'bounds':[v['position'].min(0).tolist(),v['position'].max(0).tolist()],'default_bone_index':m.default_bone_index,'bone_indices':m.bone_indices.tolist() if m.bone_indices is not None else [],'material':m.material.mat_def_path,'backface_culling':fs.use_backface_culling})
np.savez_compressed(P/'sword-original.npz',**arrays)
(P/'mesh-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
