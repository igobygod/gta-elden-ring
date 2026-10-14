import os
from pathlib import Path
import xml.etree.ElementTree as E,json,hashlib
import numpy as np
from PIL import Image
R=Path(os.environ['IGOBYGOD_BUILD_ROOT']);O=Path(__file__).parent;S=R/'Build/gta-fx-source/x64d.rpf/core.ypt';r=E.parse(S/'core.ypt.xml').getroot();emit={x.findtext('Name'):x for x in r.find('EmitterRuleDictionary')};part={x.findtext('Name'):x for x in r.find('ParticleRuleDictionary')}
def scalar(e,k,default=0):
 x=e.find(k);return default if x is None else float(x.get('value',default))
def curve(e):
 if e is None:return []
 return [[float(k.find('KeyframeTime').get('x'))]+[float(k.find('KeyframeValue').get(c)) for c in 'xyzw'] for k in e.findall('Keyframes/Item')]
def allcurves(e):return {x.findtext('Name') or x.tag:curve(x) for x in e.iter() if x.find('Keyframes') is not None}
def obj(e):
 if e is None:return None
 return dict(type=e.find('DomainType').get('value'),fields={x.tag:x.attrib for x in e if x.attrib},curves=allcurves(e))
out=[];textures={}
for effectname in ['exp_grd_grenade','exp_air_grenade']:
 effect=next(x for x in r.find('EffectRuleDictionary') if x.findtext('Name')==effectname);entry=dict(name=effectname,duration_min=scalar(effect,'DurationMin'),duration_max=scalar(effect,'DurationMax'),curves=allcurves(effect.find('KeyframeProps')),emitters=[])
 (O/f'{effectname}.source.xml').write_text(E.tostring(effect).decode())
 for event in effect.findall('EventEmitters/Item'):
  en,pn=event.findtext('EmitterRule'),event.findtext('ParticleRule');em,p=emit[en],part[pn];beh={x.find('Type').get('value'):x for x in p.find('AllBehaviours')};tn=next(x.text for x in p.iter('TextureName') if x.text);im=Image.open(S/f'{tn}.dds').convert('RGBA');textures[tn]=im
  (O/f'{en}.emitter.source.xml').write_text(E.tostring(em).decode());(O/f'{pn}.particle.source.xml').write_text(E.tostring(p).decode())
  shader={x.findtext('Name'):{c.tag:c.attrib or c.text for c in x if c.tag!='Name'} for x in p.findall('ShaderVars/Item')};dims=next((x for x in p.findall('ShaderVars/Item') if x.findtext('Name')=='texturesize'),None)
  d=dict(emitter=en,particle=pn,texture=tn,texture_size=list(im.size),start_ratio=scalar(event,'StartRatio'),end_ratio=scalar(event,'EndRatio'),oneshot=scalar(em,'IsOneShot'),emitter_curves=allcurves(em.find('KeyframeProps')),creation_domain=obj(em.find('CreationDomainObj')),target_domain=obj(em.find('TargetDomainObj')),render={x.tag:x.attrib or x.text for x in p if x.tag in ['ShaderFile','ShaderTechnique','BlendSet','DepthWrite','DepthTest','DiffuseMode','TexFrameIDMin','TexFrameIDMax','ProjectionMode','DrawType','IsLit','IsSoft']},behaviours={k:dict(curves=allcurves(v),fields={x.tag:x.attrib for x in v if x.attrib}) for k,v in beh.items()},shader_vars=shader,event_evolution_xml=E.tostring(event.find('EvolutionList')).decode(),source_dds_sha256=hashlib.sha256((S/f'{tn}.dds').read_bytes()).hexdigest())
  entry['emitters'].append(d)
 out.append(entry)
# Dedicated source-resolution atlas, preserving active arsenal atlas without edits.
atlas=Image.new('RGBA',(4096,4096));x=y=rowheight=0;placements={}
for name,im in sorted(textures.items(),key=lambda p:-p[1].height):
 if x+im.width>4096:x=0;y+=rowheight;rowheight=0
 assert y+im.height<=4096,(name,y,im.size)
 atlas.paste(im,(x,y));placements[name]=dict(rect_px=[x,y,im.width,im.height],uv=[x/4096,y/4096,im.width/4096,im.height/4096]);im.save(O/f'{name}.png');x+=im.width;rowheight=max(rowheight,im.height)
atlas.save(O/'fx-atlas.png');(O/'fx-atlas.rgba').write_bytes(atlas.tobytes());(O/'source-effects.json').write_text(json.dumps(dict(source=str(S/'core.ypt.xml'),source_sha256=hashlib.sha256((S/'core.ypt.xml').read_bytes()).hexdigest(),atlas=placements,effects=out),indent=2));print(json.dumps(dict(textures=placements,effects=[(d['name'],len(d['emitters'])) for d in out])))
