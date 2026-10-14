import os
from pathlib import Path
import json,wave,hashlib,xml.etree.ElementTree as E
import numpy as np
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).parent
# Reuse original read-only REL resolver; exclude exporter side effects.
exec((R/'Source/Conversion/export_gta_audio.py').read_text().split('pistol=nodes')[0])
SRC=R/'Build/gta-fx-source'
specs=[('smg',4,'hash_A4B3758D',leaves('hash_A4B3758D')+[('hash_BD37DB35',SRC/'x64_audio_sfx_WEAPONS_PLAYER.rpf/smg_smg.awc/0x1D37DB35.wav')]),('shotgun',5,'hash_91475774',leaves('hash_91475774')),('sniper',6,'hash_3FAAD9BC',leaves('hash_3FAAD9BC')+[('hash_3DA38C08',SRC/'x64_audio_sfx_WEAPONS_PLAYER.rpf/snp_rifle.awc/0x1DA38C08.wav')]),('explosion',3,'hash_72779D03',[('hash_8F201B1B',SRC/'x64_audio_sfx_RESIDENT.rpf/explosions.awc/0x0C13CF80.wav'),('hash_555F9648',SRC/'x64_audio_sfx_RESIDENT.rpf/explosions.awc/0x0D560342.wav')])]
reports=[]
for name,slot,root,components in specs:
 converted=[];sources=[];seen=set()
 for h,p in components:
  if p in seen:continue
  seen.add(p)
  with wave.open(str(p)) as w:
   assert w.getsampwidth()==2;samples=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float).reshape(-1,w.getnchannels()).mean(1);n=round(len(samples)*44100/w.getframerate());samples=np.interp(np.arange(n)*w.getframerate()/44100,np.arange(len(samples)),samples)
  converted.append(samples);sources.append(dict(hash=h,path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
 mix=np.zeros(min(220500,max(map(len,converted))))
 for c in converted:mix[:min(len(c),len(mix))]+=c[:len(mix)]
 # Conservative fixed original exporter gain; short fade avoids truncated tail discontinuity.
 mix*=.35;mix[-min(441,len(mix)):]*=np.linspace(1,0,min(441,len(mix)));out=np.clip(mix,-32768,32767).astype('<i2');assert np.isfinite(mix).all() and np.any(out);(O/f'{name}.pcm').write_bytes(out.tobytes());reports.append(dict(name=name,audio_slot=slot,root=root,format='44100 Hz mono PCM signed16 little endian',samples=len(out),bytes=out.nbytes,peak=int(np.max(np.abs(out.astype(np.int32)))),streams=sources,limits='Original sample layers mixed; original REL synth/filter/random/reverb graph not reproduced.'+(' Grenade exact graph is synth-only; original explosion-bank impact layers used as fallback.' if name=='explosion' else ' SMG AutoSound synth-only; original report plus SMG-bank transient used.' if name=='smg' else '')))
(O/'manifest.json').write_text(json.dumps(reports,indent=2));print(json.dumps([{k:r[k] for k in ['name','audio_slot','bytes','peak']} for r in reports]))
