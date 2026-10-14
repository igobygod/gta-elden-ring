import os
"""Resolve original donor REL references to actual decoded AWC sample streams."""
from pathlib import Path
import xml.etree.ElementTree as E
import json,wave,hashlib
import numpy as np
ROOT=Path(os.environ['IGOBYGOD_BUILD_ROOT']);SRC=ROOT/'Build/gta-fx-source';OUT=ROOT/'NativePort/combat-source/fx-assets'
OUT.mkdir(parents=True,exist_ok=True)
nodes={}
for f in (SRC/'x64_audio_audio_rel.rpf').glob('*/*.xml'):
    items=E.parse(f).getroot().find('Items')
    if items is not None:
        for e in items:
            if e.findtext('Name'):nodes[e.findtext('Name')]=e
def jenk(s):
    h=0
    for c in s.lower().encode():h=(h+c)&0xffffffff;h=(h+(h<<10))&0xffffffff;h^=h>>6
    h=(h+(h<<3))&0xffffffff;h^=h>>11;return (h+(h<<15))&0xffffffff
banks={}
for f in SRC.glob('x64_audio_sfx*/*.awc'):
    category='WEAPONS_PLAYER' if 'WEAPONS_PLAYER' in str(f) else 'RESIDENT'
    banks['hash_%08X'%jenk(category+'/'+f.stem)]=f
    banks['hash_%08X'%jenk(category+'\\'+f.stem)]=f
def leaves(root):
    out=[];seen=set()
    def walk(h):
        if h in seen:return
        seen.add(h);e=nodes.get(h)
        if e is None:return
        if e.get('type')=='SimpleSound':
            bank=banks.get(e.findtext('ContainerName'));filename=e.findtext('FileName')
            if bank and filename and filename.startswith('hash_'):
                wav=bank/('0x%08X.wav'%(int(filename[5:],16)&0x1fffffff))
                if wav.exists():out.append((h,wav))
            return
        # Traverse audio children, never category, curve, synth or mixer refs.
        for c in e.iter():
            h=c.text.strip() if c.text else ''
            if c.tag!='Name' and h in nodes and nodes[h].get('type','').endswith('Sound'):walk(h)
    walk(root);return out
pistol=nodes['hash_0EE4A1FA'].findtext('FireSound')
specs=[('rifle','hash_250178C2'),('pistol',pistol),('launcher','hash_A2A2CF85'),('explosion','hash_72779D03')]
reports=[]
for name,root in specs:
    components=leaves(root)
    if not components:
        if name=='explosion':
            reports.append(dict(name=name,root_sound=root,installed=False,reason='GTA explosion is a ModularSynthSound graph; no WAV exists for this effect. Native ER impact audio retained.'))
            continue
        raise RuntimeError('No original samples resolved for '+name)
    converted=[]
    for h,p in components:
        with wave.open(str(p)) as w:
            assert w.getsampwidth()==2
            samples=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float).reshape(-1,w.getnchannels()).mean(axis=1)
            n=int(len(samples)*44100/w.getframerate())
            samples=np.interp(np.arange(n)*w.getframerate()/44100,np.arange(len(samples)),samples)
        converted.append(samples)
    mix=np.zeros(min(44100*5,max(map(len,converted))))
    for c in converted:mix[:min(len(c),len(mix))]+=c[:len(mix)]
    # Fixed conservative gain leaves headroom for overlapping shots.
    mix=np.clip(mix*.35,-32768,32767).astype('<i2');(OUT/(name+'.pcm')).write_bytes(mix.tobytes())
    reports.append(dict(name=name,root_sound=root,streams=[dict(hash=h,path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for h,p in components],
        format='44100 Hz mono signed PCM16',samples=len(mix),limits='Sample layers only; donor synth/filter/reverb graph is not reimplemented'))
(OUT/'audio-manifest.json').write_text(json.dumps(reports,indent=2)+'\n')
print('Converted original rifle, pistol and launcher sample layers; explosion requires source synth runtime')
