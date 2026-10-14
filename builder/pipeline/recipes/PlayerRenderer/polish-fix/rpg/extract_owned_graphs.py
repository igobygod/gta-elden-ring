"""Read RPG source graphs from the owner's exported particle archive."""
from pathlib import Path
import os, json
import xml.etree.ElementTree as ET

root = Path(os.environ['IGOBYGOD_BUILD_ROOT'])
source = root / 'Build/gta-fx-source/x64d.rpf/core.ypt'
output = Path(__file__).parent
document = ET.parse(source / 'core.ypt.xml').getroot()
emitters = {item.findtext('Name'): item for item in document.find('EmitterRuleDictionary')}
particles = {item.findtext('Name'): item for item in document.find('ParticleRuleDictionary')}
effects = {item.findtext('Name'): item for item in document.find('EffectRuleDictionary')}
result = {'source': str(source / 'core.ypt.xml'), 'effects': {}, 'textureFiles': {}}
for name in ('exp_grd_rpg', 'exp_air_rpg', 'proj_rpg_trail', 'muz_rpg'):
    effect = effects[name]
    rules = []
    for event in effect.findall('EventEmitters/Item'):
        emitter, particle = event.findtext('EmitterRule'), event.findtext('ParticleRule')
        textures = list(dict.fromkeys(item.text for item in particles[particle].iter('TextureName') if item.text))
        for texture in textures:
            result['textureFiles'][texture] = [str(source / (texture + '.dds'))]
        rules.append({'emitterName': emitter, 'particleName': particle, 'textures': textures,
                      'emitterXML': ET.tostring(emitters[emitter]).decode(),
                      'particleXML': ET.tostring(particles[particle]).decode()})
    result['effects'][name] = {'effectXML': ET.tostring(effect).decode(), 'rules': rules}
(output / 'rpg-effects-source.json').write_text(json.dumps(result))
