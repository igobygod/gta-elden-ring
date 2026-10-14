import os
from pathlib import Path
import json,xml.etree.ElementTree as ET
from PIL import Image
O=Path(__file__).parent;root=O.parents[2];d=json.loads((O.parent/'rpg-effects-source.json').read_text());old=json.loads((root/'repair-next/fx/source-effects.json').read_text());atlas=dict(old['atlas']);image=Image.frombytes('RGBA',(4096,4096),(root/'rifle-stage/fx-atlas.rgba').read_bytes())
new={'_explosion_fireball_rgba':(0,3072,1024,512),'ptfx_smoke_wispy_anim':(2048,2048,1024,1024),'ptfx_multi_sheet_002_debris':(1024,3072,256,256),'ptfx_glow':(1280,3072,128,128),'ptfx_muzzle_02':(1408,3072,512,512)}
for name,rect in new.items():
 im=Image.open(d['textureFiles'][name][0]).convert('RGBA');assert im.size==rect[2:];image.paste(im,rect[:2]);atlas[name]={'rect_px':rect,'uv':[x/4096 for x in rect]}
(O/'fx-atlas.rgba').write_bytes(image.tobytes());image.save(O/'fx-atlas.png')
grid={'_explosion_fireball_rgba':(8,4),'ptfx_smoke_wispy_anim':(7,7),'ptfx_multi_sheet_002_debris':(4,4),'ptfx_embers_rgb':(2,2),'ptfx_muzzle_core_rgb':(4,4),'ptfx_muzzle_02':(1,2),'ptfx_glow_rgb':(2,2)}
def val(e,n,default=0):
 x=None if e is None else e.find(n);return default if x is None else float(x.get('value',default))
def curves(e):
 out={}
 for x in e.iter():
  n=x.findtext('Name');ks=x.find('Keyframes')
  if n and ks is not None:
   rows=[]
   for k in ks.findall('Item'):
    t,v=k.find('KeyframeTime'),k.find('KeyframeValue')
    if t is not None and v is not None:rows.append([float(t.get('x',0))]+[float(v.get(a,0)) for a in 'xyzw'])
   if rows:out[n]=rows
 return out
def f(x):
 s=format(float(x),'.9g');return s+('f' if '.' in s or 'e' in s else '.0f')
lines=['#pragma once','#include "../../../repair-next/fx/original_grenade_fx.h"','namespace gta_rpg_fx_source {','using gta_original_fx::Key;using gta_original_fx::Curve;using gta_original_fx::Emitter;','struct Spec{Emitter emitter;unsigned columns,rows,animLast;bool animate,scaled,hold,pointRelative;unsigned creationShape,targetShape;float zoomMin,zoomMax,tintAlpha;bool randomised=false;unsigned alignment=0;bool soft=false,rgbAlpha=false,lit=true,rgbCutoff=false,accumulateAngle=false;};','struct Effect{const Spec*specs;unsigned count;float duration,zoomMin,zoomMax;};'];report={}
for en,ef in d['effects'].items():
 e=ET.fromstring(ef['effectXML']);ev={x.findtext('EmitterRule'):x for x in e.findall('EventEmitters/Item')};entries=[];effectcurves=curves(e);z=effectcurves.get('ptxEffectRule:m_zoomScalarKFP',[[0,100,100,0,0]])[0];duration=val(e,'DurationMax',1)
 for i,r in enumerate(ef['rules']):
  a,b=ET.fromstring(r['emitterXML']),ET.fromstring(r['particleXML']);v=ev[r['emitterName']];cs=curves(a);cs.update(curves(b));table=[]
  # ShaderVar keyframes store time/value in Unknown0/Unknown10, not Keyframes.
  for material in b.findall('ShaderVars/Item'):
   if material.find('Type').get('value')!='Keyframe':continue
   rows=[[val(k,'Unknown0'),val(k,'Unknown10'),0,0,0] for k in material.findall('Items/Item')]
   if rows:cs['material:'+material.findtext('Name')]=rows
  # LOD evolutions preserved separately; runtime uses authored base curves.
  for item in v.findall('EvolutionList/EvolvedKeyframeProps/Item'):
   n=item.findtext('Name');ks=item.find('Items/Item/Keyframes')
   if n and ks is not None:
    cs['LOD:'+n]=[[float(k.find('KeyframeTime').get('x',0))]+[float(k.find('KeyframeValue').get(axis,0)) for axis in 'xyzw'] for k in ks.findall('Item')]
  for j,(n,ks) in enumerate(cs.items()):
   if not ks:continue
   symbol=f'{en}_k{i}_{j}';lines.append('inline constexpr Key '+symbol+'[]={'+','.join('{'+','.join(f(x) for x in row)+'}' for row in ks)+'};');table.append('{'+json.dumps(n)+','+symbol+','+str(len(ks))+'}')
  symbol=f'{en}_c{i}';lines.append('inline constexpr Curve '+symbol+'[]={'+','.join(table)+'};');texture=r['textures'][0];uv=atlas[texture]['uv'];cols,rows=grid.get(texture,(1,1));ani=next((x for x in b.findall('AllBehaviours/Item') if x.find('Type').get('value')=='AnimateTexture'),None)
  first,last=int(val(b,'TexFrameIDMin')),int(val(b,'TexFrameIDMax'));lastanim=int(val(ani,'LastFrameID',last)) if ani is not None else last;assert max(first,last,lastanim)<cols*rows,(texture,first,last,lastanim,cols,rows)
  sh=lambda n: {'Cylinder':0,'Sphere':1,'Box':2}[a.find(n+'/DomainType').get('value')]
  em='{'+','.join([json.dumps(r['emitterName']),json.dumps(r['particleName']),f(val(v,'StartRatio')*duration),f(val(v,'EndRatio',1)*duration),str(int(val(a,'IsOneShot'))),str(int(val(b,'BlendSet'))),str(first),str(last),'{'+','.join(f(x) for x in uv)+'}',symbol,str(len(table))])+'}'
  tint=int(v.find('ColourTintMin').get('value'),0);entries.append('{'+em+','+','.join([str(cols),str(rows),str(lastanim),str(ani is not None).lower(),str(bool(val(ani,'IsScaledOverParticleLife')) if ani is not None else False).lower(),str(bool(val(ani,'IsHeldOnLastFrame')) if ani is not None else True).lower(),str(bool(val(a.find('TargetDomainObj'),'IsPointRelative'))).lower(),str(sh('CreationDomainObj')),str(sh('TargetDomainObj')),f(val(v,'ZoomScalarMin',1)),f(val(v,'ZoomScalarMax',1)),f(((tint>>24)&255)/255),str(bool(val(ani,'IsRandomised')) if ani is not None else False).lower(),str(int(val(next((x for x in b.findall('AllBehaviours/Item') if x.find('Type').get('value')=='Sprite'),ET.Element('x')),'AlignmentMode',0))),str(bool(val(b,'IsSoft'))).lower(),str(int(val(b,'DiffuseMode'))==4).lower(),str(bool(val(b,'IsLit'))).lower(),str(bool(val(b.find("ShaderVars/Item[Name='alphacutoffminmax']"),'VectorZ'))).lower(),str(bool(val(next((x for x in b.findall('AllBehaviours/Item') if x.find('Type').get('value')=='Rotation'),None),'AccumulateAngle'))).lower()])+'}')
 lines.append('inline constexpr Spec '+en+'_specs[]={'+','.join(entries)+'};');lines.append('inline constexpr Effect '+en+'={'+en+'_specs,'+str(len(entries))+','+f(duration)+','+f(z[1]*.01)+','+f(z[2]*.01)+'};');report[en]={'emitters':len(entries),'effect_zoom':[z[1]*.01,z[2]*.01]}
lines.append('}');(O/'original_rpg_fx.h').write_text('\n'.join(lines));(O/'report.json').write_text(json.dumps({'source':d['source'],'effects':report,'atlas':atlas,'grid':grid,'limits':['LOD evolution curves retained but not automatically distance-selected','light-only glow has no size curves and is omitted as a sprite','full RAGE refraction/shadow/lights and engine collision callbacks not reproduced']},indent=2));print('Generated original RPG ground/air/trail/muzzle metadata and atlas')
