import os
"""Resolve original GTA rifle materials for the renderer's opaque atlas pass.

The donor palette shaders use diffuse alpha as a palette coordinate, not opacity.
Keep all seven meshes/vertex ranges so existing animation packets remain compatible;
omit only overlay indices until a separate alpha/decal pass is implemented.
This module does not write or modify assets.
"""

from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image


PROJECT_ROOT = Path(os.environ['IGOBYGOD_BUILD_ROOT'])
MODEL_NAME = 'w_ar_assaultrifle_hi'


def bake_weapon_palette(diffuse, palette):
    """Bake the default weapon palette, matching CodeWalker BasicPS.hlsl.

    Reference: Tools/CodeWalker-src/CodeWalker.Shaders/BasicPS.hlsl:24-31.
    Its x coordinate is (round(alpha * 255.009995) - 32) * 0.007813;
    its y coordinate 0.015625 is the center of row zero in the 32-row palette.
    Match the bilinear/wrap sampling used by the existing native asset converter.
    """
    rgba = np.array(diffuse.convert('RGBA'), dtype=np.uint8)
    pal = np.asarray(palette.convert('RGBA'), dtype=np.uint8)
    if pal.shape[0] != 32:
        raise ValueError(f'Expected 32-row GTA weapon palette; got {pal.shape[0]}')
    x = (rgba[:, :, 3].astype(np.float64) - 32) * 0.007813 * pal.shape[1] - 0.5
    lo = np.floor(x).astype(np.int64)
    fraction = (x - lo)[:, :, None]
    tint = (pal[0, lo % pal.shape[1], :3] * (1 - fraction)
            + pal[0, (lo + 1) % pal.shape[1], :3] * fraction)
    rgba[:, :, :3] = np.rint(rgba[:, :, :3] * tint / 255).astype(np.uint8)
    rgba[:, :, 3] = 255
    return Image.fromarray(rgba)


def prepare_rifle_materials(guns, project_root=PROJECT_ROOT):
    """Return (RGBA images, emit_indices flags, JSON-serializable report).

    Inputs and outputs retain the original mesh order and vertex counts. A caller
    must still append every mesh's UVs, positions, and vertex offset, even when
    emit_indices[i] is False. Images are returned at their native resolution;
    the existing atlas builder owns tile resizing and placement.
    """
    donor = Path(project_root) / 'Assets/GTA-Franklin/x64e.rpf'
    xml_path = donor / f'{MODEL_NAME}.ydr' / f'{MODEL_NAME}.ydr.xml'
    texture_dir = donor / 'w_ar_assaultrifle.ytd'
    root = ET.parse(xml_path).getroot()
    geometries = root.findall('DrawableModelsHigh/Item/Geometries/Item')
    shaders = root.findall('ShaderGroup/Shaders/Item')
    expected_names = [f'{MODEL_NAME}-{i}' for i in range(len(geometries))]
    if [m['name'] for m in guns] != expected_names:
        raise ValueError('Rifle mesh order differs from the original donor; refusing material remap')

    images, emit_indices, records, cache = [], [], [], {}
    for i, (mesh, geometry) in enumerate(zip(guns, geometries)):
        shader_index = int(geometry.find('ShaderIndex').get('value'))
        shader = shaders[shader_index]
        diffuse_name = shader.findtext("Parameters/Item[@name='DiffuseSampler']/Name")
        palette_name = shader.findtext("Parameters/Item[@name='TextureSamplerDiffPal']/Name")
        render_bucket = int(shader.find('RenderBucket').get('value'))
        hard_alpha = shader.find("Parameters/Item[@name='HardAlphaBlend']")
        cutout = hard_alpha is not None and float(hard_alpha.get('x', '0')) > 0
        solid = render_bucket == 0 and not cutout
        key = (diffuse_name, palette_name, solid)
        if key not in cache:
            diffuse_path = texture_dir / f'{diffuse_name.lower()}.dds'
            with Image.open(diffuse_path) as source:
                image = source.convert('RGBA')
            if palette_name:
                with Image.open(texture_dir / f'{palette_name.lower()}.dds') as palette:
                    image = bake_weapon_palette(image, palette)
            elif solid:
                image.putalpha(255)
            cache[key] = image
        images.append(cache[key])
        emit_indices.append(solid)
        records.append({
            'mesh': mesh['name'],
            'geometry': i,
            'shader_index': shader_index,
            'diffuse': diffuse_name,
            'palette': palette_name,
            'palette_row': 0 if palette_name else None,
            'render_bucket': render_bucket,
            'hard_alpha_blend': cutout,
            'emit_indices': solid,
            'vertices_preserved': len(mesh['positions']),
            'indices_before': int(np.asarray(mesh['faces']).size),
            'indices_after': int(np.asarray(mesh['faces']).size) if solid else 0,
            'reason': ('Default donor palette baked into RGB; opaque alpha'
                       if solid else 'Overlay requires alpha/decal pass; retain vertices, omit indices'),
        })
    report = {
        'source': str(xml_path),
        'palette_reference': 'Tools/CodeWalker-src/CodeWalker.Shaders/BasicPS.hlsl:24-31',
        'mesh_count': len(guns),
        'vertices_preserved': sum(len(m['positions']) for m in guns),
        'indices_before': sum(r['indices_before'] for r in records),
        'indices_after': sum(r['indices_after'] for r in records),
        'meshes': records,
    }
    return images, emit_indices, report
