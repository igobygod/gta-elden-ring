import os
"""Stage original full Gun_GripR attachment frame; other meshes remain byte-exact."""
from pathlib import Path
import json,struct,xml.etree.ElementTree as ET
import numpy as np
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).parent
source=R/'Assets/GTA-Franklin/x64e.rpf/w_ex_grenadefrag_hi.ydr/w_ex_grenadefrag_hi.ydr.xml'
bones=ET.parse(source).getroot().findall('Skeleton/Bones/Item');world=[]
for b in bones:
 q=np.array([float(b.find('Rotation').get(k))for k in 'xyzw']);q/=np.linalg.norm(q);x,y,z,w=q
 m=np.eye(4);m[:3,:3]=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]
 m[:3,3]=[float(b.find('Translation').get(k))for k in 'xyz'];parent=int(b.find('ParentIndex').get('value'));world.append(m if parent<0 else world[parent]@m)
grip=world[[b.findtext('Name')for b in bones].index('Gun_GripR')];attachment=np.linalg.inv(grip)
b=bytearray((R/'PlayerRenderer/rifle-stage/arsenal-mesh.bin').read_bytes());h=struct.unpack_from('<4I',b);assert h==(0x414d5348,1,8,4096);at=16;changed=None
for _ in range(h[2]):
 slot,nv,ni,valid=struct.unpack_from('<4I',b,at);at+=16
 if slot==7:
  old=np.frombuffer(b,dtype='<f4',count=16,offset=at).reshape(4,4).copy();changed=(at,old);b[at:at+64]=attachment.astype('<f4').tobytes()
 at+=64+12+nv*(12+8)+ni*4
assert at==len(b) and changed
assert np.max(np.abs(attachment@grip-np.eye(4)))<1e-12
before=(R/'PlayerRenderer/rifle-stage/arsenal-mesh.bin').read_bytes();offset=changed[0];assert b[:offset]==before[:offset] and b[offset+64:]==before[offset+64:]
(O/'arsenal-mesh.bin').write_bytes(b)
report=dict(source=str(source),source_grip=grip.tolist(),old_attachment=changed[1].tolist(),new_attachment=attachment.tolist(),grip_frame_identity_max_error=float(np.max(np.abs(attachment@grip-np.eye(4)))),changed_bytes=64,other_meshes_and_release_marker_unchanged=True)
(O/'grip-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
