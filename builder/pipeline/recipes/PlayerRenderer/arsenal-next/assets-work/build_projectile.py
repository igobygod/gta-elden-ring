import os
from pathlib import Path
import xml.etree.ElementTree as E
import struct,json
import numpy as np
from PIL import Image
O=Path(__file__).parent;xml=O/'donor/x64e.rpf/w_lr_40mm.ydr/w_lr_40mm.ydr.xml';root=E.parse(xml).getroot();atlas=Image.open(O/'atlas.png').convert('RGBA');pos=[];uv=[];ind=[];off=0
W={'Position':3,'BlendWeights':4,'BlendIndices':4,'Normal':3,'Colour0':4,'Colour1':4,'TexCoord0':2,'TexCoord1':2,'Tangent':4};report=[]
for i,g in enumerate(root.findall('DrawableModelsHigh/Item/Geometries/Item')):
 rows=np.array([[float(v) for v in row.split()] for row in g.findtext('VertexBuffer/Data').strip().splitlines()]);fields={};start=0
 for c in g.find('VertexBuffer/Layout'):fields[c.tag]=rows[:,start:start+W[c.tag]];start+=W[c.tag]
 shader=root.findall('ShaderGroup/Shaders/Item')[int(g.find('ShaderIndex').get('value'))];tn=shader.findtext("Parameters/Item[@name='DiffuseSampler']/Name");image=Image.open(O/f'donor/x64e.rpf/w_lr_40mm.ytd/{tn.lower()}.dds').convert('RGBA');image.putalpha(255);atlas.paste(image.resize((256,256)),(3584+i*256,3072));v=fields['Position'];u=(np.mod(fields['TexCoord0'],1)*256+[3584+i*256,3072])/4096;f=np.fromstring(g.findtext('IndexBuffer/Data'),sep=' ',dtype=np.uint32);pos.extend(v);uv.extend(u);ind.extend(f+off);off+=len(v);report.append(dict(geometry=i,vertices=len(v),indices=len(f),diffuse=tn))
pos=np.asarray(pos,dtype='<f4');uv=np.asarray(uv,dtype='<f4');ind=np.asarray(ind,dtype='<u4');assert np.isfinite(pos).all() and np.isfinite(uv).all() and ind.max()<len(pos)
with (O/'projectile-mesh.bin').open('wb') as f:
 f.write(struct.pack('<4I',0x414d5348,1,1,4096));f.write(struct.pack('<4I',6,len(pos),len(ind),1));f.write(np.eye(4,dtype='<f4').tobytes());f.write(np.zeros(3,dtype='<f4').tobytes());f.write(pos.tobytes());f.write(uv.tobytes());f.write(ind.tobytes())
atlas.save(O/'atlas.png');(O/'atlas.rgba').write_bytes(atlas.tobytes());r=dict(source=str(xml),ammo='AMMO_GRENADELAUNCHER',model='w_lr_40mm',slot=6,vertices=len(pos),indices=len(ind),bounds=[pos.min(0).tolist(),pos.max(0).tolist()],atlas_tile_px=[3584,3072,256,256],meshes=report,file_bytes=(O/'projectile-mesh.bin').stat().st_size);(O/'projectile-report.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
