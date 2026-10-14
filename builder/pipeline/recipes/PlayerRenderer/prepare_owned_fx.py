"""Extract the particle sheet images used by the core arsenal atlas."""
from pathlib import Path
import os
import xml.etree.ElementTree as ET
from PIL import Image

root = Path(os.environ['IGOBYGOD_BUILD_ROOT'])
source = root / 'Build/gta-fx-source/x64d.rpf/core.ypt'
output = root / 'NativePort/combat-source/fx-assets'
output.mkdir(parents=True, exist_ok=True)
document = ET.parse(source / 'core.ypt.xml').getroot()
particles = {item.findtext('Name'): item for item in document.find('ParticleRuleDictionary')}
for name in ('muz_assault_flare', 'bullet_tracer', 'exp_grd_gren_core', 'exp_grd_gren_core_pillar', 'muz_smg_core'):
    particle = particles[name]
    texture = next(item.text for item in particle.iter('TextureName') if item.text)
    with Image.open(source / (texture + '.dds')) as image:
        image.convert('RGBA').save(output / (name + '.png'))
