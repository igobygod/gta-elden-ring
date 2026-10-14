import os
from pathlib import Path
import json
O=Path(__file__).parent;root=O.parents[2];d=json.loads((root/'repair-next/fx/source-effects.json').read_text());effect=d['effects'][0]
def f(x):
 s=format(float(x),'.9g');return s+('f' if '.' in s or 'e' in s else '.0f')
def field(e,n,default=0):return float(e.get(n,{}).get('value',default))
lines=['#pragma once','#include "original_rpg_fx.h"','namespace gta_original_grenade_source {','using gta_rpg_fx_source::Spec;using gta_rpg_fx_source::Effect;using gta_original_fx::Key;using gta_original_fx::Curve;'];records=[]
grid={'ptfx_smoke_sheet_6':(6,6),'_explosion_fireball':(6,6),'ptfx_rubble':(2,2),'ptfx_flat_glow_rgb':(1,1),'ptfx_smoke_spikes':(1,1)}
for i,e in enumerate(effect['emitters']):
 cs=dict(e['emitter_curves'])
 for b in e['behaviours'].values():cs.update(b['curves'])
 for domain in ['creation_domain','target_domain']:cs.update(e[domain]['curves'])
 table=[]
 for j,(n,ks) in enumerate(cs.items()):
  if not ks:continue
  symbol=f'gk{i}_{j}';lines.append('inline constexpr Key '+symbol+'[]={'+','.join('{'+','.join(f(x) for x in k)+'}' for k in ks)+'};');table.append('{'+json.dumps(n)+','+symbol+','+str(len(ks))+'}')
 curves='gc'+str(i);lines.append('inline constexpr Curve '+curves+'[]={'+','.join(table)+'};');v=e['render'];uv=d['atlas'][e['texture']]['uv'];first,last=int(field(v,'TexFrameIDMin')),int(field(v,'TexFrameIDMax'));animation=e['behaviours'].get('AnimateTexture');fields=animation['fields'] if animation else {};cols,rows=grid[e['texture']];end=int(field(fields,'LastFrameID',last));assert max(first,last,end)<cols*rows
 base='{'+','.join([json.dumps(e['emitter']),json.dumps(e['particle']),f(e['start_ratio']),f(e['end_ratio']),str(int(e['oneshot'])),str(int(field(v,'BlendSet'))),str(first),str(last),'{'+','.join(f(x) for x in uv)+'}',curves,str(len(table))])+'}'
 shape=lambda n:{'Cylinder':0,'Sphere':1,'Box':2}[e[n]['type']]
 records.append('{'+base+','+','.join([str(cols),str(rows),str(end),str(bool(animation)).lower(),str(bool(field(fields,'IsScaledOverParticleLife'))).lower(),str(bool(field(fields,'IsHeldOnLastFrame',1))).lower(),str(bool(field(e['target_domain']['fields'],'IsPointRelative'))).lower(),str(shape('creation_domain')),str(shape('target_domain')),'1.0f','1.0f','1.0f',str(bool(field(fields,'IsRandomised'))).lower(),str(int(field(e['behaviours']['Sprite']['fields'],'AlignmentMode'))),str(bool(field(v,'IsSoft'))).lower(),str(int(field(v,'DiffuseMode'))==4).lower(),str(bool(field(v,'IsLit',1))).lower(),'false',str(bool(field(e['behaviours'].get('Rotation',{}).get('fields',{}),'AccumulateAngle'))).lower()])+'}')
lines+=['inline constexpr Spec ground_specs[]={'+','.join(records)+'};'];zoom=effect['curves']['ptxEffectRule:m_zoomScalarKFP'][0];lines+=['inline constexpr Effect ground={ground_specs,'+str(len(records))+','+f(effect['duration_max'])+','+f(zoom[1]*.01)+','+f(zoom[2]*.01)+'};','}'];(O/'original_grenade_typed.h').write_text('\n'.join(lines));print('Generated6 source-typed original grenade rules; no emitter-index decisions')
