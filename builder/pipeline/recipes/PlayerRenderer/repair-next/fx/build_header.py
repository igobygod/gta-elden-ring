import os
from pathlib import Path
import json
O=Path(__file__).parent;d=json.loads((O/'source-effects.json').read_text());lines=['#pragma once','#include <cstddef>','namespace gta_original_fx {','struct Key {float t,x,y,z,w;}; struct Curve {const char* name;const Key* keys;unsigned count;};','struct Emitter {const char* name;const char* particle;float start,end;unsigned oneShot,blend,first,last;float uv[4];const Curve* curves;unsigned curveCount;};']
def f(x):return format(float(x),'.9g')+('f' if '.' in format(float(x),'.9g') or 'e' in format(float(x),'.9g') else '.0f')
records=[]
for i,e in enumerate(d['effects'][0]['emitters']):
 curves=dict(e['emitter_curves'])
 for b in e['behaviours'].values():curves.update(b['curves'])
 for domain in ['creation_domain','target_domain']:curves.update({domain+':'+k:v for k,v in e[domain]['curves'].items()})
 table=[]
 for j,(n,ks) in enumerate(curves.items()):
  if not ks:continue
  symbol=f'k{i}_{j}';lines.append('inline constexpr Key '+symbol+'[]={' + ','.join('{'+','.join(f(x) for x in k)+'}' for k in ks)+'};');table.append('{'+json.dumps(n)+','+symbol+','+str(len(ks))+'}')
 lines.append('inline constexpr Curve c'+str(i)+'[]={'+','.join(table)+'};');v=e['render'];uv=d['atlas'][e['texture']]['uv'];records.append('{'+json.dumps(e['emitter'])+','+json.dumps(e['particle'])+','+f(e['start_ratio'])+','+f(e['end_ratio'])+','+str(int(e['oneshot']))+','+v['BlendSet']['value']+','+v['TexFrameIDMin']['value']+','+v['TexFrameIDMax']['value']+',{'+','.join(f(x) for x in uv)+'},c'+str(i)+','+str(len(table))+'}')
lines+=['inline constexpr Emitter groundGrenade[]={'+','.join(records)+'};','inline constexpr float groundEffectDuration=1.0f;','}'];(O/'original_grenade_fx.h').write_text('\n'.join(lines))
