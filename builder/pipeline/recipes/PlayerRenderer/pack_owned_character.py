"""Build the current vertex-animation packet directly from local GTA exports."""
from pathlib import Path
import json, os, struct
import numpy as np
from PIL import Image

root = Path(os.environ['IGOBYGOD_BUILD_ROOT'])
source = root / 'Build/original-rig-proof'
output = root / 'PlayerRenderer'
meshes = json.loads((source / 'movement-meshes.json').read_text())
frames = np.load(source / 'player-movement-frames.npz')
atlas = Image.new('RGBA', (2048, 2048))
uvs, indices, vertex_count = [], [], 0
for i, mesh in enumerate(meshes):
    if i >= 16:
        raise ValueError('The owned character has more geometry than the supported atlas layout.')
    image = Image.open(mesh['texture']).convert('RGBA').resize((512, 512))
    col, row = i % 4, i // 4
    atlas.paste(image, (col * 512, row * 512))
    uv = np.asarray(mesh['uv'], dtype=np.float32)
    uv[:, 0] = (col + np.mod(uv[:, 0], 1)) / 4
    uv[:, 1] = (row + np.mod(uv[:, 1], 1)) / 4
    uvs.extend(uv)
    indices.extend((np.asarray(mesh['faces']) + vertex_count).ravel())
    vertex_count += mesh['count']
names = ['idle', 'walk', 'run', 'sprint']
for name in names:
    if frames[name].shape != (120, vertex_count, 3) or not np.isfinite(frames[name]).all():
        raise ValueError('Malformed local movement clip: ' + name)
with (output / 'model.bin').open('wb') as stream:
    stream.write(struct.pack('<5I', 0x47544151, vertex_count, len(indices), 120, 4))
    stream.write(np.asarray(indices, dtype='<u4').tobytes())
    stream.write(np.asarray(uvs, dtype='<f4').tobytes())
    stream.write(struct.pack('<4f', *[float(frames[n + '_duration']) for n in names]))
    for name in names:
        stream.write(np.asarray(frames[name], dtype='<f4').tobytes())
(output / 'atlas.rgba').write_bytes(atlas.tobytes())
print('Packed owned character:', vertex_count, 'vertices,', len(indices), 'indices')
