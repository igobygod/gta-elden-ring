import os
from pathlib import Path
import numpy as np,wave,json,hashlib
O=Path(__file__).parent;D=O/'donor/x64_audio_sfx_SCRIPT.rpf/offmission_wasted.awc';records=[];sounds=[]
for name,hash in [('0x0465EDA9.wav','hash_5C9B2D03'),('0x0041E476.wav','hash_AC5026D5')]:
 p=D/name
 with wave.open(str(p)) as w:
  sr=w.getframerate();a=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float).reshape(-1,w.getnchannels()).mean(1);a=np.interp(np.arange(round(len(a)*44100/sr))*sr/44100,np.arange(len(a)),a)
 sounds.append(a);records.append(dict(file=str(p),source_archive='GTA V/x64/audio/sfx/SCRIPT.rpf/offmission_wasted.awc',simple_sound=hash,wave_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),seconds=len(a)/44100))
# Retain full onset/decay; donor accompanying bed has explicit hold2200/release800ms envelope.
main,bed=sounds;time=np.arange(len(bed))/44100;envelope=np.where(time<2.2,1,np.clip((3-time)/.8,0,1));mix=np.zeros(max(len(main),len(bed)));mix[:len(main)]+=main;mix[:len(bed)]+=bed*envelope;mix*=.55;out=np.clip(mix,-32768,32767).astype('<i2');(O/'wasted.pcm').write_bytes(out.tobytes())
with wave.open(str(O/'wasted.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(out.tobytes())
report=dict(format='44100 Hz mono signed PCM16 little endian',bytes=out.nbytes,seconds=len(out)/44100,peak=int(np.max(np.abs(out.astype(np.int32)))),streams=records,limits=['Original source samples retained and resampled; native GTA wrapper stereo pan/mixer not reproduced.','Bed source EnvelopeSound hash_2BE25381 hold2200ms release800ms applied linear approximation to donor release curve.','Not synthetic imitation; full7.4339s source onset stream preserved.']);(O/'audio-manifest.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
