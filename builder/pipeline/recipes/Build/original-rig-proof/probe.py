import os
"""Isolated donor-space skinning experiment. Never writes installed mod files."""
from pathlib import Path
import xml.etree.ElementTree as E
import json
import numpy as np
from PIL import Image
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']); O=Path(__file__).parent; D=R/'Assets/GTA-Franklin'
def qm(q):
 q=np.array(q,float);q/=np.linalg.norm(q);x,y,z,w=q
 return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def rig(nodes):
 ns=[];ts=[];parents=[];loc=[]
 for b in nodes:
  ns.append(b.findtext('Name'));ts.append(int(b.find('Tag').get('value')));parents.append(int(b.find('ParentIndex').get('value')))
  m=np.eye(4);m[:3,:3]=qm([b.find('Rotation').get(k) for k in 'xyzw'])@np.diag([float(b.find('Scale').get(k)) for k in 'xyz']);m[:3,3]=[float(b.find('Translation').get(k)) for k in 'xyz'];loc.append(m)
 return ns,ts,parents,np.array(loc)
def worlds(loc,parents):
 out=[]
 for m,p in zip(loc,parents):out.append(out[p]@m if p>=0 else m)
 return np.array(out)
names,tags,parents,rest=rig(E.parse(next(D.rglob('player_one.yft.xml'))).getroot().findall('.//Skeleton/Bones/Item'))
restworld=worlds(rest,parents);tagindex={t:i for i,t in enumerate(tags)}
data=json.loads((R/'Build/rifle-animation-source/weapons_rifle_hi_.json').read_text());clip=next(c for c in data['weapon_clips'] if c['name']=='pack:/fire_med.clip');amap={a['hash']:a for a in clip['animations']}
def pose(time):
 loc=rest.copy()
 for layer in clip['layers']:
  a=amap[layer['hash']]; t=min(layer['end'],layer['start']+time*layer['rate'])
  for tr in a['tracks']:
   if tr['bone'] not in tagindex or tr['track'] not in (0,1):continue
   vals=np.array(tr['samples']);phase=np.clip(t*(a['frames']-1)/a['duration'],0,len(vals)-1);i=int(phase);j=min(i+1,len(vals)-1);u,v=vals[i],vals[j]
   if tr['track']==1 and np.dot(u,v)<0:v=-v
   v=u+(v-u)*(phase-i); bi=tagindex[tr['bone']]
   if tr['track']==1:loc[bi,:3,:3]=qm(v)
   else:loc[bi,:3,3]=v[:3]
 return worlds(loc,parents)
W={'Position':3,'BlendWeights':4,'BlendIndices':4,'Normal':3,'Colour0':4,'Colour1':4,'TexCoord0':2,'TexCoord1':2,'Tangent':4}
def build():
 global meshes
 meshes=[];report={'clip':clip['name'],'duration':clip['duration'],'excluded_cloth_triangles':{},'ignored_track_types':sorted({t['track'] for a in clip['animations'] for t in a['tracks'] if t['track'] not in (0,1)}),'limitations':['Only transform tracks 0/1 evaluated; IK/animation-graph channels not implemented.','Cloth-encoded vertices excluded, not approximated.','Weapon attached at PH_R_Hand with identity offset as a hypothesis; not validated.'],'frames':[]}
 for model in ['uppr_000_u','head_000_r','hair_000_u','teef_000_u','hand_000_u','lowr_000_r','feet_000_u','w_ar_assaultrifle_hi']:
  gun=model.startswith('w_'); p=next(D.rglob(model+('.ydr.xml' if gun else '.ydd.xml')));root=E.parse(p).getroot();draw=root if gun else root.find('Item');bn,bt,bp,bl=rig(draw.findall('Skeleton/Bones/Item'));bw=worlds(bl,bp)
  for gi,g in enumerate(draw.findall('DrawableModelsHigh/Item/Geometries/Item')):
   rows=np.array([[float(v) for v in row.split()] for row in g.findtext('VertexBuffer/Data').strip().splitlines()]);f={};start=0
   for col in g.find('VertexBuffer/Layout'):f[col.tag]=rows[:,start:start+W[col.tag]];start+=W[col.tag]
   faces=np.fromstring(g.findtext('IndexBuffer/Data'),sep=' ',dtype=int).reshape(-1,3);pos=f['Position']; uv=f.get('TexCoord0',np.zeros((len(pos),2)))
   shader=draw.findall('ShaderGroup/Shaders/Item')[int(g.find('ShaderIndex').get('value'))]; tn=shader.findtext("Parameters/Item[@name='DiffuseSampler']/Name");tex=next((p for p in D.rglob('*.dds') if p.stem.lower()==tn.lower()),None)
   png=O/(model+'-'+str(gi)+'.png')
   if tex:Image.open(tex).convert('RGBA').save(png)
   weights=f.get('BlendWeights',np.tile([255,0,0,0],(len(pos),1)))/255;ids=f.get('BlendIndices',np.zeros((len(pos),4))).astype(int);palette=np.fromstring(g.findtext('BoneIDs') or '0',sep=',',dtype=int)
   valid=(ids>=0).all(1)&(ids<len(palette)).all(1)&(np.abs(weights.sum(1)-1)<.01);good=valid[faces].all(1);report['excluded_cloth_triangles'][model+'-'+str(gi)]=int((~good).sum());faces=faces[good];ids[~valid]=0;weights[~valid]=[1,0,0,0]
   joints=palette[ids]; meshes.append(dict(name=model+'-'+str(gi),positions=pos,uv=uv,faces=faces,texture=str(png) if tex else None,weights=weights,joints=joints,bones=bn,bind=bw,gun=gun))
 frames=[]
 for time in [None,0,.15,.45,.9,1.3]:
  world=restworld if time is None else pose(time); row=[]
  for m in meshes:
   p=np.column_stack([m['positions'],np.ones(len(m['positions']))])
   if m['gun']:positions=(world[names.index('PH_R_Hand')]@p.T).T[:,:3]
   else:
    skin=np.array([world[names.index(n)]@np.linalg.inv(restworld[names.index(n)]) for n in m['bones']]);positions=np.zeros((len(p),3))
    for k in range(4):positions+=np.einsum('nij,nj->ni',skin[m['joints'][:,k]],p)[:,:3]*m['weights'][:,k,None]
   assert np.isfinite(positions).all()
   row.append({k:m[k] for k in ['name','texture']}|dict(vertices=positions.tolist(),faces=m['faces'].tolist(),uv=m['uv'].tolist()))
  frames.append(row);report['frames'].append({'time':time,'bounds':[np.concatenate([np.array(x['vertices']) for x in row]).min(0).tolist(),np.concatenate([np.array(x['vertices']) for x in row]).max(0).tolist()]})
 (O/'frames.json').write_text(json.dumps(frames));(O/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

if __name__ == "__main__":
 build()
