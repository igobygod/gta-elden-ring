import os
"""Repair the native ER sword socket; preserve native timing/body paths byte-for-byte."""
from pathlib import Path
import struct,sys,json,hashlib
import numpy as np
O=Path(__file__).parent;P=O.parents[1];sys.path.insert(0,str(P/'polish-v4.5/sword'))
import build_carry as c
from extract_socket import extract
param=json.loads((P/'polish-v4.6/aim/sword-absorption.json').read_text())
assert param['weapon']==2140000 and param['absorption']==23
socket=struct.unpack_from('<h',bytes.fromhex(param['raw']),8)[0]
assert socket==20
L,evidence=extract(socket)
source=P/'polish-v4.4/melee-assets/melee-joints.bin'; raw=source.read_bytes();out=bytearray(raw)
h=struct.unpack_from('<4I',raw);assert h==(0x4d4a4e54,1,317,64);at=16;modified=[];unchanged=[];worst=0
for k in range(h[3]):
 start=at;rid,nf,d,flags=struct.unpack_from('<IIfI',raw,at);at+=16
 t_at=at;at+=nf*317*12;q_at=at;at+=nf*317*16;w_at=at;at+=nf*64
 if rid//1000000!=23:unchanged.append(rid);continue
 q=np.frombuffer(raw,'<f4',nf*317*4,q_at).reshape(nf,317,4).copy()
 q[:,c.carry_ids[3:]]=c.carry_q[3:]
 w=np.frombuffer(raw,'<f4',nf*16,w_at).reshape(nf,4,4).astype(float)@L
 assert np.isfinite(w).all()
 out[q_at:w_at]=q.astype('<f4').tobytes();out[w_at:at]=w.astype('<f4').tobytes()
 # Timings, root, wrist, and all non-finger local transforms are unmodified.
 assert out[start:q_at]==raw[start:q_at]
 oldq=np.frombuffer(raw,'<f4',nf*317*4,q_at).reshape(nf,317,4)
 keep=np.ones(317,bool);keep[c.carry_ids[3:]]=False
 assert np.array_equal(oldq[:,keep],q[:,keep])
 err=np.max(np.abs(w[:,:3,:3].transpose(0,2,1)@w[:,:3,:3]-np.eye(3)))
 worst=max(worst,float(err));assert err<1e-4
 modified.append({'raw_id':rid,'frames':nf,'duration':d})
assert at==len(raw)
(O/'melee-joints.bin').write_bytes(out)
# Reload through the same sparse samples used for rendered proofs.
x=np.load(P/'polish-v4.4/melee-assets/pose-preview.npz');preview={k:x[k].copy()for k in x.files}
for key in preview:
 if not key.startswith('23'):continue
 if key.endswith('_weapon'):preview[key]=preview[key]@L
 if key.endswith('_q'):preview[key][:,c.carry_ids[3:]]=c.carry_q[3:]
np.savez_compressed(O/'pose-preview.npz',**preview)
report={'weapon':2140000,'absorption_row':23,'right_hand_dummy':socket,'socket':evidence,'changed_native_sword_clips':modified,'unchanged_other_clips':unchanged,'finger_indices':c.carry_ids[3:],'max_anchor_orthogonality_error':worst,'bytes':len(out),'sha256':hashlib.sha256(out).hexdigest(),'source_sha256':hashlib.sha256(raw).hexdigest()}
(O/'attack-repair-report.json').write_text(json.dumps(report,indent=2))
print('PASS',len(modified),'native sword clips repaired;',len(unchanged),'other clips unchanged;',len(out),'bytes; matrix error',worst)
