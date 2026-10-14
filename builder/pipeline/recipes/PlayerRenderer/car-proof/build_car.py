import os
"""One bounded original Buffalo2 asset conversion; no game or production files edited."""
from pathlib import Path
import json, struct, hashlib, xml.etree.ElementTree as ET
import numpy as np
from PIL import Image
P=Path(__file__).resolve().parent; O=P/'stage'; D=O/'donor'
root=ET.parse(D/'buffalo2.yft/buffalo2.yft.xml').getroot(); draw=root.find('Drawable')
W={'Position':3,'BlendWeights':4,'BlendIndices':4,'Normal':3,'Colour0':4,'Colour1':4,'TexCoord0':2,'TexCoord1':2,'TexCoord2':2,'Tangent':4}
def vec(e):return np.array([float(e.attrib[a]) for a in 'xyz'])
def geom(g):
 rows=np.fromstring(g.findtext('VertexBuffer/Data'),sep=' ').reshape(-1,sum(W[e.tag] for e in g.find('VertexBuffer/Layout'))); c=0;f={}
 for e in g.find('VertexBuffer/Layout'):f[e.tag]=rows[:,c:c+W[e.tag]];c+=W[e.tag]
 faces=np.fromstring(g.findtext('IndexBuffer/Data'),sep=' ',dtype=np.uint32).reshape(-1,3)
 area=np.cross(f['Position'][faces[:,1]]-f['Position'][faces[:,0]],f['Position'][faces[:,2]]-f['Position'][faces[:,0]])
 keep=np.linalg.norm(area,axis=1)>1e-12;faces=faces[keep];area=area[keep]
 norm=np.linalg.norm(f['Normal'],axis=1);bad=norm<1e-6
 if bad.any():
  geometric=np.zeros_like(f['Normal'])
  for j in range(3):np.add.at(geometric,faces[:,j],area)
  for v in np.flatnonzero(bad):
   if np.linalg.norm(geometric[v])<1e-9:
    related=np.flatnonzero((faces==v).any(1))
    if len(related):geometric[v]=area[related[0]]
  f['Normal'][bad]=geometric[bad];norm=np.linalg.norm(f['Normal'],axis=1)
 f['Normal']/=np.maximum(norm[:,None],1e-12)
 return f,faces,int(g.find('ShaderIndex').get('value'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
# CodeWalker standalone exports may expose shader IDs before its global name index
# is warmed. Resolve only the shader names this converter actually implements.
def shader_hash(name):
 h=0
 for b in name.lower().encode():
  h=(h+b)&0xffffffff;h=(h+(h<<10))&0xffffffff;h^=h>>6
 h=(h+(h<<3))&0xffffffff;h^=h>>11
 return 'hash_%08X'%((h+(h<<15))&0xffffffff)
SHADERS=['vehicle_paint1.sps','vehicle_mesh.sps','vehicle_vehglass_inner.sps',
 'vehicle_tire.sps','vehicle_interior2.sps','vehicle_shuts.sps',
 'vehicle_lightsemissive.sps','vehicle_badges.sps','vehicle_licenseplate.sps',
 'vehicle_lights.sps','vehicle_dash_emissive.sps','vehicle_vehglass.sps']
SHADER_NAMES={shader_hash(name):name for name in SHADERS}
def shader_name(name):
 resolved=SHADER_NAMES.get(name,name)
 if resolved not in SHADERS:raise ValueError('Unsupported original car shader: '+str(name))
 return resolved
# Preserve original UVs; runtime frac before mapping inside padded material rectangle.
tex={p.stem.lower():p for p in D.rglob('*.dds')}; atlas=Image.new('RGBA',(2048,2048));materials=[]
for i,s in enumerate(draw.findall('ShaderGroup/Shaders/Item')):
 shader=shader_name(s.findtext('FileName'));diff=s.findtext("Parameters/Item[@name='DiffuseSampler']/Name") or s.findtext("Parameters/Item[@name='PlateBgSampler']/Name")
 kind=1 if 'paint' in shader else 3 if 'vehglass' in shader else 4 if 'dash' in shader else 2 if any(x in shader for x in ['badges','lights','lightsemissive']) else 0
 missing=diff not in tex
 if missing and kind!=4:raise ValueError(('required texture missing',diff))
 image=Image.new('RGBA',(1,1),(0,0,0,255)) if missing else Image.open(tex[diff]).convert('RGBA')
 if kind in (0,1):image.putalpha(255)
 x,y=(i%4)*512,(i//4)*512;tile=image.resize((508,508),Image.Resampling.LANCZOS);atlas.paste(tile,(x+2,y+2))
 # Border duplicates prevent filtering into adjacent materials, even exact UV boundaries.
 atlas.paste(tile.crop((0,0,1,508)).resize((2,508)),(x,y+2));atlas.paste(tile.crop((507,0,508,508)).resize((2,508)),(x+510,y+2))
 atlas.paste(atlas.crop((x,y+2,x+512,y+3)).resize((512,2)),(x,y));atlas.paste(atlas.crop((x,y+509,x+512,y+510)).resize((512,2)),(x,y+510))
 materials.append(dict(id=i,shader=shader,diffuse=diff,source=str(tex[diff].relative_to(P)) if not missing else None,source_sha256=sha(tex[diff]) if not missing else None,original_size=list(image.size),kind=kind,rect=[(x+2.5)/2048,(y+2.5)/2048,507/2048,507/2048],tint=[.90,.91,.90,1] if kind==1 else [1,1,1,1],omitted=kind==4))
bones={e.findtext('Name'):(j,e) for j,e in enumerate(draw.findall('Skeleton/Bones/Item'))}; names=['chassis','wheel_lf','wheel_rf','wheel_lr','wheel_rr'];meshes=[]
for n in names:meshes.append(dict(name=n,pivot=vec(bones[n][1].find('Translation')),positions=[],normals=[],uv=[],indices=[],ranges=[]))
meshes[0]['pivot']=np.zeros(3)
def append(mi,f,faces,mat,rotation=None,translation=None):
 if not len(faces):return
 used,ix=np.unique(faces,return_inverse=True);ix=ix.ravel();pos=f['Position'][used].copy();normal=f['Normal'][used].copy()
 if rotation is not None:pos=pos@rotation.T;normal=normal@rotation.T
 if translation is not None:pos+=translation
 m=meshes[mi];off=len(m['positions']);first=len(m['indices']);m['positions'].extend(pos);m['normals'].extend(normal);m['uv'].extend(f['TexCoord0'][used]);m['indices'].extend(ix+off);m['ranges'].append([mat,first,len(ix)])
# Isolate original hub/disc triangles with each wheel; intact remaining bind vertices are already model-space.
hubs={bones['hub_'+n[6:]][0]:i for i,n in enumerate(names) if i}
source_geoms=[]
for gi,g in enumerate(draw.findall('DrawableModelsHigh/Item/Geometries/Item')):
 f,faces,mat=geom(g);source_geoms.append(dict(geometry=gi,material=mat,vertices=len(f['Position']),indices=int(faces.size)))
 if materials[mat]['omitted']:continue
 b=f['BlendIndices'][np.arange(len(f['Position'])),np.argmax(f['BlendWeights'],axis=1)].astype(int);hubmask=np.zeros(len(faces),bool)
 for bone,mi in hubs.items():
  mask=(b[faces]==bone).all(1);hubmask|=mask;append(mi,f,faces[mask],mat,translation=-meshes[mi]['pivot'])
 append(0,f,faces[~hubmask],mat)
# Original fragment instancing: same front wheel reused rear. Right-side rotation from CodeWalker Renderable.cs 224.
children=root.findall('Physics/LOD1/Children/Item');wheel=next(c.find('Drawable') for c in children if int(c.find('BoneTag').get('value'))==27922)
for mi,n in enumerate(names[1:],1):
 rot=np.diag([-1,1,-1]) if n in ['wheel_rf','wheel_rr'] else np.eye(3)
 for g in wheel.findall('DrawableModelsHigh/Item/Geometries/Item'):f,faces,mat=geom(g);append(mi,f,faces,mat,rotation=rot)
for m in meshes:
 for k in ['positions','normals','uv']:m[k]=np.array(m[k],dtype='<f4')
 m['indices']=np.array(m['indices'],dtype='<u4');assert np.isfinite(m['positions']).all() and m['indices'].max()<len(m['positions']);assert np.max(np.abs(np.linalg.norm(m['normals'],axis=1)-1))<.01
allpos=np.concatenate([m['positions']+m['pivot'] for m in meshes]);bounds=np.array([allpos.min(0),allpos.max(0)])
seat=vec(bones['seat_dside_f'][1].find('Translation'))
with (O/'buffalo2.carbin').open('wb') as out:
 out.write(struct.pack('<6I',0x42524143,1,len(meshes),len(materials),2048,0));out.write(bounds.astype('<f4').tobytes());out.write(seat.astype('<f4').tobytes())
 for mat in materials:out.write(struct.pack('<2I8f',mat['id'],mat['kind'],*mat['rect'],*mat['tint']))
 for m in meshes:
  out.write(m['name'].encode().ljust(32,b'\0'));out.write(struct.pack('<3f3I',*m['pivot'],len(m['positions']),len(m['indices']),len(m['ranges'])));out.write(np.concatenate([m['positions'],m['normals'],m['uv']],1).astype('<f4').tobytes());out.write(m['indices'].tobytes());out.write(np.array(m['ranges'],dtype='<u4').tobytes())
atlas.save(O/'buffalo2-atlas.png');(O/'buffalo2-atlas.rgba').write_bytes(atlas.tobytes())
np.savez(O/'buffalo2-preview.npz',**{f'{k}{i}':v for i,m in enumerate(meshes) for k,v in m.items() if k!='name'})
# Original collision geometry, without manufacturing Bullet topology/convexity claims.
cb=root.find('Physics/LOD1/Archetype/Bounds');c0=cb.find('Children/Item');center=vec(c0.find('GeometryCenter'))
cv=np.array([[float(s) for s in row.replace(',',' ').split()] for row in c0.findtext('Vertices').strip().splitlines()])+center
polys=[]
for p in c0.find('Polygons'):
 if p.tag=='Triangle':polys.append([int(p.get(a)) for a in ['v1','v2','v3']])
collision=dict(source='buffalo2.yft/Physics/LOD1/Archetype/Bounds/Children/Item[0]',basis='GTA +X right,+Y forward,+Z up; metres',geometry_center_already_applied=center.tolist(),vertices=cv.tolist(),triangles=polys,bounds=[cv.min(0).tolist(),cv.max(0).tolist()],composite_bounds=[vec(cb.find('BoxMin')).tolist(),vec(cb.find('BoxMax')).tolist()],margin=float(c0.find('Margin').get('value')),primitive_children=[dict(index=i,type=e.get('type'),bounds=[vec(e.find('BoxMin')).tolist(),vec(e.find('BoxMax')).tolist()]) for i,e in enumerate(cb.find('Children'))])
(O/'buffalo2-collision.json').write_text(json.dumps(collision,indent=2))
report=dict(model='buffalo2',source='x64e.rpf/levels/gta5/vehicles.rpf/buffalo2.yft',source_xml_sha256=sha(D/'buffalo2.yft/buffalo2.yft.xml'),format='CARB v1',basis='Native GTA metres: +X right,+Y forward,+Z up. Proper Y-up conversion (x,z,-y). No Franklin pelvis offset.',seat_driver=seat.tolist(),bounds=bounds.tolist(),materials=materials,geometry_sources=source_geoms,meshes=[dict(name=m['name'],pivot=m['pivot'].tolist(),vertices=len(m['positions']),triangles=len(m['indices'])//3,bounds=[m['positions'].min(0).tolist(),m['positions'].max(0).tolist()],ranges=m['ranges']) for m in meshes],collision_vertices=len(cv),collision_triangles=len(polys),limitations=['Original diffuse/alpha assets; white paint tint is staging choice, not engine car-paint shader parity.','Original normals retained; no GTA environment reflections, damage shader, dirt, lights switching, breakable doors or interior dashboard render-target simulation.','Four dashboard display vertices omitted because script_rt_dials_gen_taxi is a runtime render target, not a source texture.','Original plate background retained; no dynamic registration characters.','Native UVs require frac per-material then padded atlas mapping; do not modulo vertex UV before interpolation.','Runtime render/physics/seat/input integration is separate and not tested in game.'])
for p in [O/'buffalo2.carbin',O/'buffalo2-atlas.rgba',O/'buffalo2-collision.json']:report.setdefault('outputs',[]).append(dict(file=p.name,bytes=p.stat().st_size,sha256=sha(p)))
(O/'manifest.json').write_text(json.dumps(report,indent=2));print(json.dumps(dict(vertices=sum(len(m['positions']) for m in meshes),triangles=sum(len(m['indices'])//3 for m in meshes),bounds=bounds.tolist(),collision=(len(cv),len(polys)),outputs=report['outputs'])))
