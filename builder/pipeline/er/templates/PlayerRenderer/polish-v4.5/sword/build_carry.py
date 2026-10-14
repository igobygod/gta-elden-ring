import os
"""Sword-only GTA gait carry pose. Retains native melee packets and attack anchors byte-for-byte."""
from pathlib import Path
import numpy as np,struct,json,xml.etree.ElementTree as ET,ast
O=Path(__file__).parent;P=O.parents[1];R=P.parent;OLD=P/'polish-v4.4/melee-assets'
raw=(P/'rifle-stage/arsenal-joints.bin').read_bytes();_,_,nb,nv,nc,hand=struct.unpack_from('<6I',raw);at=24
parent=np.frombuffer(raw,'<i4',nb,at);at+=nb*4
rest=np.frombuffer(raw,'<f4',nb*16,at).reshape(nb,4,4).astype(float);at+=nb*64
body=np.frombuffer(raw,'<f4',nv*3,at).reshape(nv,3);at+=nv*12
joints=np.frombuffer(raw,'<u4',nv*4,at).reshape(nv,4);at+=nv*16
weights=np.frombuffer(raw,'<f4',nv*4,at).reshape(nv,4);at+=nv*16
scale=np.linalg.norm(rest[:,:3,:3],axis=1)
names=[n.findtext('Name')for n in ET.parse(next((R/'Assets/GTA-Franklin').rglob('player_one.yft.xml'))).getroot().findall('.//Skeleton/Bones/Item')]
def qmat(q):
 x,y,z,w=np.asarray(q).T;m=np.empty((*np.asarray(q).shape[:-1],3,3));m[...,0,0]=1-2*(y*y+z*z);m[...,0,1]=2*(x*y-z*w);m[...,0,2]=2*(x*z+y*w);m[...,1,0]=2*(x*y+z*w);m[...,1,1]=1-2*(x*x+z*z);m[...,1,2]=2*(y*z-x*w);m[...,2,0]=2*(x*z-y*w);m[...,2,1]=2*(y*z+x*w);m[...,2,2]=1-2*(x*x+y*y);return m
source=ast.parse((OLD/'build_melee.py').read_text());node=next(x for x in source.body if isinstance(x,ast.FunctionDef)and x.name=='quat');ns={'np':np};exec(compile(ast.Module(body=[node],type_ignores=[]),'<quat>','exec'),ns);quat=ns['quat']
def worlds(t,q):
 loc=np.tile(np.eye(4),(nb,1,1));loc[:,:3,:3]=qmat(q)*scale[:,None,:];loc[:,:3,3]=t;w=np.empty_like(loc)
 for i in range(nb):w[i]=w[parent[i]]@loc[i]if parent[i]>=0 else loc[i]
 return w
raw=(OLD/'melee-joints.bin').read_bytes();at=16;clips={}
for _ in range(struct.unpack_from('<4I',raw)[3]):
 rid,nf,d,fl=struct.unpack_from('<IIfI',raw,at);at+=16;t=np.frombuffer(raw,'<f4',nf*nb*3,at).reshape(nf,nb,3).copy();at+=nf*nb*12;q=np.frombuffer(raw,'<f4',nf*nb*4,at).reshape(nf,nb,4).copy();at+=nf*nb*16;w=np.frombuffer(raw,'<f4',nf*16,at).reshape(nf,4,4).copy();at+=nf*64;clips[rid]=(t,q,w)
attachment=np.frombuffer((OLD/'sword-fallback-attachment.bin').read_bytes(),'<f4').reshape(4,4).astype(float)
# Original native one-hand sword attack startup contains its actual holding grip.
source_t,source_q,source_w=clips[23030000]
carry_ids=[names.index('SKEL_R_'+x)for x in ['UpperArm','Forearm','Hand']]+[i for i,n in enumerate(names)if n.startswith('SKEL_R_Finger')]
carry_q=source_q[0,carry_ids].copy()
def apply(t,q):
 q=q.copy();upper=q[carry_ids[0]].copy();q[carry_ids]=carry_q
 if np.dot(upper,carry_q[0])<0:upper=-upper
 q[carry_ids[0]]=.15*upper+.85*carry_q[0];q[carry_ids[0]]/=np.linalg.norm(q[carry_ids[0]])
 return t.copy(),q
# Carry's fingers use the already decoded original GTA cylindrical right-hand grip,
# with index joints curled to match the other fingers instead of resting on a trigger.
raw=(P/'rifle-stage/arsenal-joints.bin').read_bytes();at=24+nb*4+nb*64+nv*(12+16+16);gun=None
for _ in range(nc):
 slot,action,nf,fl,duration=struct.unpack_from('<IIIIf',raw,at);at+=20+nf*nb*12
 q=np.frombuffer(raw,'<f4',nf*nb*4,at).reshape(nf,nb,4);at+=nf*nb*16
 if slot==4 and action==0:gun=q[0].copy()
assert gun is not None
base_t,base_q,_=clips[90000000];carry_q=base_q[0,carry_ids].copy()
for k,i in enumerate(carry_ids[3:],3):carry_q[k]=gun[i]
# Same hinge flexion as middle/ring at the index PIP/DIP, keeping index's metacarpal basis.
for a,b in [('SKEL_R_Finger11','SKEL_R_Finger21'),('SKEL_R_Finger12','SKEL_R_Finger22')]:carry_q[carry_ids.index(names.index(a))]=gun[names.index(b)]
# The physical hilt runs across the closed fingers. Its blade must leave on the
# thumb side (+hand Y), unlike the source native socket used on the relaxed GTA hand.
B=np.diag([1.,-1.,1.]);target=np.array([.20,.9365,-.2899]);target/=np.linalg.norm(target)
qt=base_q[0].copy();qt[carry_ids]=carry_q;w=worlds(base_t[0],qt);h=names.index('SKEL_R_Hand')
cur=w[h,:3,:3]@(B@np.array([0,-1.,0]));cur/=np.linalg.norm(cur)
v=np.cross(cur,target);co=float(cur@target);K=np.array([[0,-v[2],v[1]],[v[2],0,-v[0]],[-v[1],v[0],0]]);rot=np.eye(3)+K+K@K/(1+co)
newhand=rot@w[h,:3,:3]
# Share the correction with forearm pronation instead of putting 74 degrees
# into the wrist. Twist about the elbow->wrist axis leaves wrist origin fixed.
f=names.index('SKEL_R_Forearm');axis=w[h,:3,3]-w[f,:3,3];axis/=np.linalg.norm(axis)
rq=quat(rot);twq=np.r_[axis*np.dot(rq[:3],axis),rq[3]];twq/=np.linalg.norm(twq);tw=qmat(twq)
newforearm=tw@w[f,:3,:3]
carry_q[carry_ids.index(f)]=quat((np.linalg.inv(w[parent[f],:3,:3])@newforearm)/scale[f])
carry_q[carry_ids.index(h)]=quat((np.linalg.inv(newforearm)@newhand)/scale[h])
# Locate handle center from the actual closed finger chains in hand-local space.
qt[carry_ids]=carry_q;w=worlds(base_t[0],qt);hi=np.linalg.inv(w[h]);centers=[]
for finger in [2,3,4]:
 points=np.array([(hi@w[names.index('SKEL_R_Finger'+str(finger)+str(j))])[:3,3]for j in range(3)])
 centers.append((points[0]+points[2])*.5)
center=np.mean(centers,axis=0);center[1]=0
# .120m is the center of the original hilt (crossguard Y~.035, pommel~.20).
local_sword=np.eye(4);local_sword[:3,:3]=B;local_sword[:3,3]=center-B@np.array([0,.12,0])
attachment=np.linalg.inv(rest[hand])@local_sword
if __name__=='__main__':
 (O/'carry-report.json').write_text(json.dumps({'source_grip':'Original GTA assault-rifle right-hand cylindrical grip with curled index','source_arm':'Original GTA Franklin idle; wrist corrected only','blade_direction':target.tolist(),'wrist_rotation_degrees':float(np.degrees(np.arccos(co))),'handle_center_hand_local':center.tolist(),'attachment':attachment.tolist(),'joints':[{'index':i,'name':names[i],'q':q.tolist()}for i,q in zip(carry_ids,carry_q)]},indent=2))
 print('Carry direction',target.round(3),'wrist correction degrees',round(float(np.degrees(np.arccos(co))),2),'grip center',center.round(3))
