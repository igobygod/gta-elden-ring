import os
from pathlib import Path
import xml.etree.ElementTree as E,json
S=Path(os.environ['IGOBYGOD_BUILD_ROOT'])/'Build/gta-fx-source/x64d.rpf/core.ypt';O=Path(__file__).parent;r=E.parse(S/'core.ypt.xml').getroot();em={x.findtext('Name'):x for x in r.find('EmitterRuleDictionary')};pa={x.findtext('Name'):x for x in r.find('ParticleRuleDictionary')};out={'source':str(S/'core.ypt.xml'),'effects':{},'requiredTextures':[],'textureFiles':{}};textures=set()
for name in ['exp_grd_grenade','exp_air_grenade','exp_grd_gren_sp','proj_grenade_trail']:
 e=next(x for x in r.find('EffectRuleDictionary')if x.findtext('Name')==name);rules=[]
 for ev in e.findall('EventEmitters/Item'):
  en,pn=ev.findtext('EmitterRule'),ev.findtext('ParticleRule');a,b=em[en],pa[pn];ts=list(dict.fromkeys(x.text for x in b.iter('TextureName')if x.text));textures.update(ts);rules.append({'emitterName':en,'particleName':pn,'textures':ts,'emitterXML':E.tostring(a).decode(),'particleXML':E.tostring(b).decode()})
 out['effects'][name]={'effectXML':E.tostring(e).decode(),'rules':rules}
 (O/(name+'.source.xml')).write_text(E.tostring(e).decode())
out['requiredTextures']=sorted(textures);out['textureFiles']={t:[str(S/(t+'.dds'))]for t in textures};(O/'grenade-effects-source.json').write_text(json.dumps(out,indent=2));print({k:len(v['rules'])for k,v in out['effects'].items()});print(sorted(textures))
