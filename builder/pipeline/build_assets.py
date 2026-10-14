#!/usr/bin/env python3
"""Local-only asset builder for igobygod. This source contains no game payloads.

The output is a private workspace. Never redistribute its extracted/generated
files. A complete asset build is separate from runtime and gameplay validation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
CORE_OUTPUTS = [
    'PlayerRenderer/model.bin', 'PlayerRenderer/atlas.rgba',
    'PlayerRenderer/rifle-stage/atlas.rgba',
    'PlayerRenderer/rifle-stage/arsenal-joints.bin',
    'PlayerRenderer/rifle-stage/arsenal-mesh.bin',
    'PlayerRenderer/rifle-stage/projectile-mesh.bin',
    'PlayerRenderer/rifle-stage/rpg-joints.bin',
    'PlayerRenderer/rifle-stage/rpg-mesh.bin',
    'PlayerRenderer/rifle-stage/rpg-projectile-mesh.bin',
    'PlayerRenderer/rifle-stage/armed-directional.bin',
    'PlayerRenderer/rifle-stage/fx-atlas.rgba',
    'PlayerRenderer/repair-next/fx/original_grenade_fx.h',
    'PlayerRenderer/polish-fix/rpg/fx/original_rpg_fx.h',
    'PlayerRenderer/polish-fix/rpg/fx/original_gl_fx.h',
    'PlayerRenderer/polish-fix/rpg/fx/original_grenade_typed.h',
]

def run(command, *, env=None, cwd=None):
    subprocess.run([str(x) for x in command], check=True, env=env, cwd=cwd)

def safe_workspace(path, games):
    path = path.expanduser().resolve()
    for game in games:
        if path == game or game in path.parents or path in game.parents:
            raise ValueError('Output must be separate from both game installations.')
    marker = path / '.igobygod-local-build'
    if path.exists() and any(path.iterdir()) and not marker.exists():
        raise ValueError('Choose an empty output folder; refusing to overwrite an unrelated directory.')
    path.mkdir(parents=True, exist_ok=True)
    marker.write_text('Private local game-derived assets; do not distribute.\n')
    return path

GTA_ARCHIVES = ('x64v.rpf','x64e.rpf','x64c.rpf','x64b.rpf','x64d.rpf',
                'x64/audio/sfx/WEAPONS_PLAYER.rpf','x64/audio/sfx/RESIDENT.rpf',
                'x64/audio/sfx/SCRIPT.rpf','x64/audio/audio_rel.rpf')


def source_stamp(gta):
    files = {}
    for name in ('GTA5.exe', *GTA_ARCHIVES):
        source = gta / name
        if not source.is_file() or not source.stat().st_size:
            raise ValueError('Required GTA V Legacy file is missing: '+name)
        info = source.stat()
        files[name] = {'bytes':info.st_size,'mtime_ns':info.st_mtime_ns}
    files['GTA5.exe']['sha256'] = hashlib.sha256((gta/'GTA5.exe').read_bytes()).hexdigest()
    return {'gta_directory':str(gta),'files':files}


def verify_extraction(output, stamp):
    source_file=output/'gta-source.local.json'
    if not source_file.is_file() or json.loads(source_file.read_text()) != stamp:
        raise ValueError('Cached extraction belongs to different or changed game files. Choose a new output folder.')
    receipt=output/'local-extraction.json'
    if not receipt.is_file():
        raise ValueError('No completed local extraction exists to resume.')
    for row in json.loads(receipt.read_text()):
        path=(output/row['output']).resolve()
        if output.resolve() not in path.parents or not path.is_file():
            raise ValueError('Cached extraction contains a missing or invalid file.')
        if hashlib.sha256(path.read_bytes()).hexdigest()!=row['sha256']:
            raise ValueError('Cached extraction changed. Choose a new output folder.')


def install_recipes(output):
    for source in (HERE / 'recipes').rglob('*.py'):
        target = output / source.relative_to(HERE / 'recipes')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)

def decode_exports(output, python, env):
    decoder = output / 'Source/Conversion/decode_gta_ycd.py'
    aliases = {
        'weapons@rifle@hi@': 'weapons_rifle_hi_',
        'weapons@rifle@hi@assault_rifle_str': 'weapons_rifle_hi_assault_rifle_str',
    }
    for xml in sorted((output / 'Assets/GTA-Movement').rglob('*.ycd.xml')):
        name = xml.name.removesuffix('.ycd.xml')
        target = output / 'Build/movement-animation-source' / (name + '.json')
        run([python, decoder, xml, target], env=env)
        copies = [output / 'PlayerRenderer/arsenal-next/assets-work' / (name + '.json')]
        if name in aliases:
            copies.append(output / 'Build/rifle-animation-source' / (aliases[name] + '.json'))
        if name.startswith('move_strafe@') or name.startswith('move_weapon@pistol'):
            copies.append(output / 'PlayerRenderer/polish-next/movement' / (name + '.ycd.json'))
        if name in ('weapons@heavy@rpg', 'weapons@heavy@rpg_str'):
            filename = 'rpg.json' if name.endswith('@rpg') else 'rpg_str.json'
            copies.append(output / 'PlayerRenderer/polish-fix/rpg' / filename)
        if name in ('veh@std@ds@base', 'veh@std@ds@enter_exit'):
            copies.append(output / 'PlayerRenderer/car-drive' / ('base.json' if name.endswith('@base') else 'enter_exit.json'))
        for copy in copies:
            copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(target, copy)

def convert_core(output, python, env):
    def recipe(relative, *args):
        run([python, output / relative, *args], env=env, cwd=output)
    def copy(source, target):
        dest = output / target
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(output / source, dest)
    stage = 'PlayerRenderer/rifle-stage'
    (output / stage).mkdir(parents=True, exist_ok=True)
    recipe('Build/original-rig-proof/player_movement_pack.py')
    recipe('PlayerRenderer/pack_owned_character.py')
    recipe('PlayerRenderer/tune_gaits.py')
    copy('PlayerRenderer/model-tuned.bin', 'PlayerRenderer/model.bin')
    recipe('PlayerRenderer/prepare_owned_fx.py')
    recipe('PlayerRenderer/build_rifle.py', output / stage)
    recipe('PlayerRenderer/arsenal-next/assets-work/build_assets.py')
    donor = output / 'PlayerRenderer/arsenal-next/assets-work/donor/x64e.rpf'
    donor.mkdir(parents=True, exist_ok=True)
    for name in ('w_lr_40mm.ydr', 'w_lr_40mm.ytd'):
        shutil.copytree(output / 'Assets/GTA-Franklin/x64e.rpf' / name, donor / name, dirs_exist_ok=True)
    recipe('PlayerRenderer/arsenal-next/assets-work/build_projectile.py')
    for name in ('atlas.png', 'atlas.rgba', 'arsenal-mesh.bin', 'arsenal-joints.bin', 'projectile-mesh.bin'):
        copy('PlayerRenderer/arsenal-next/assets-work/' + name, stage + '/' + name)
    recipe('PlayerRenderer/polish-next/movement/build_directional.py')
    copy('PlayerRenderer/polish-next/movement/armed-directional.bin', stage + '/armed-directional.bin')
    recipe('PlayerRenderer/polish-fix/rpg/build_rpg_joints.py')
    recipe('PlayerRenderer/polish-fix/rpg/build_rpg.py')
    for name in ('atlas.png', 'atlas.rgba', 'rpg-mesh.bin', 'rpg-projectile-mesh.bin', 'rpg-joints.bin'):
        copy('PlayerRenderer/polish-fix/rpg/' + name, stage + '/' + name)
    recipe('PlayerRenderer/repair-next/fx/extract_effect.py')
    recipe('PlayerRenderer/repair-next/fx/build_header.py')
    copy('PlayerRenderer/repair-next/fx/fx-atlas.rgba', stage + '/fx-atlas.rgba')
    recipe('PlayerRenderer/polish-fix/rpg/extract_owned_graphs.py')
    recipe('PlayerRenderer/polish-fix/gl-fx/extract_graphs.py')
    for name in ('build_rpg_fx.py', 'build_gl_fx.py', 'build_grenade_fx.py'):
        recipe('PlayerRenderer/polish-fix/rpg/fx/' + name)
    copy('PlayerRenderer/polish-fix/rpg/fx/fx-atlas.rgba', stage + '/fx-atlas.rgba')
    recipe('PlayerRenderer/repair-next/movement/repair_gaits.py')
    copy('PlayerRenderer/repair-next/movement/arsenal-joints.bin', stage + '/arsenal-joints.bin')
    recipe('PlayerRenderer/polish-next/movement/patch_carry.py')
    copy('PlayerRenderer/polish-next/movement/arsenal-joints.bin', stage + '/arsenal-joints.bin')
    recipe('PlayerRenderer/polish-v4/grip/repair_grenade_grip.py')
    copy('PlayerRenderer/polish-v4/grip/arsenal-mesh.bin', stage + '/arsenal-mesh.bin')

def convert_gta_extras(output, python, env, java, ffdec, include_wheel=True):
    def recipe(relative):
        run([python, output / relative], env=env, cwd=output / 'PlayerRenderer')
    def copy(source, target):
        dest = output / target
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(output / source, dest)
    java_command = [java, '-Xmx512m', '-Djava.awt.headless=true', '-jar', ffdec]
    hud = output / 'PlayerRenderer/polish-next/hud'
    death = output / 'PlayerRenderer/polish-next/death-ui'
    wheel = output / 'PlayerRenderer/polish-v4.3/camera-wheel'
    run([*java_command, '-onerror', 'abort', '-selectid', '3,9', '-export', 'font', death / 'exported-fonts', death / 'donor/x64b.rpf/font_lib_efigs_pc.gfx/font_lib_efigs_pc.gfx'])
    run([*java_command, '-onerror', 'abort', '-selectid', '512,508,461,309,254,260,358,360,362,364,366,380,384', '-format', 'shape:png', '-export', 'shape', hud / 'minimap-png', hud / 'donor/x64b.rpf/minimap.gfx/minimap.gfx'])
    run([*java_command, '-swf2xml', hud / 'donor/x64b.rpf/hud.gfx/hud.gfx', wheel / 'hud.xml'])
    recipe('PlayerRenderer/polish-next/hud/build_hud.py')
    recipe('PlayerRenderer/polish-v4/radar/build_radar_icons.py')
    recipe('PlayerRenderer/polish-next/death-ui/render_original_wasted.py')
    recipe('Source/Conversion/export_gta_audio.py')
    recipe('PlayerRenderer/arsenal-next/audio/build_audio.py')
    recipe('PlayerRenderer/polish-next/death-ui/build_wasted_audio.py')
    recipe('PlayerRenderer/car-proof/build_car.py')
    recipe('PlayerRenderer/car-drive/export_pose.py')
    if include_wheel:
        recipe('PlayerRenderer/polish-v4.3/camera-wheel/build_wheel_atlas.py')
        copy('PlayerRenderer/polish-v4.3/camera-wheel/hud-atlas.rgba', 'PlayerRenderer/rifle-stage/hud-atlas.rgba')
    for source, name in [('PlayerRenderer/polish-next/death-ui/wasted-original.rgba', 'wasted-original.rgba'),
                         ('PlayerRenderer/car-proof/stage/buffalo2.carbin', 'buffalo2.carbin'),
                         ('PlayerRenderer/car-proof/stage/buffalo2-atlas.rgba', 'buffalo2-atlas.rgba'),
                         ('PlayerRenderer/car-drive/driver.bin', 'driver.bin')]:
        copy(source, 'PlayerRenderer/rifle-stage/' + name)
    for name in ('rifle', 'pistol', 'launcher'):
        copy('NativePort/combat-source/fx-assets/' + name + '.pcm', 'PlayerRenderer/rifle-stage/audio/' + name + '.pcm')
    for name in ('smg', 'shotgun', 'sniper', 'explosion'):
        copy('PlayerRenderer/arsenal-next/audio/' + name + '.pcm', 'PlayerRenderer/rifle-stage/audio/' + name + '.pcm')
    copy('PlayerRenderer/polish-next/death-ui/wasted.pcm', 'PlayerRenderer/rifle-stage/audio/wasted.pcm')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gta', type=Path, required=True)
    parser.add_argument('--elden-ring', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tool-root', type=Path, required=True)
    parser.add_argument('--dotnet', default=shutil.which('dotnet') or 'dotnet')
    parser.add_argument('--python', default=sys.executable)
    parser.add_argument('--java')
    parser.add_argument('--ffdec', type=Path)
    parser.add_argument('--oodle-helper', type=Path)
    parser.add_argument('--er-extractor', type=Path)
    parser.add_argument('--er-params', type=Path)
    parser.add_argument('--soulsformats-dll', type=Path)
    parser.add_argument('--paramdefs', type=Path)
    parser.add_argument('--wine')
    parser.add_argument('--bottle')
    parser.add_argument('--stage', choices=['all', 'gta-core'], default='all')
    parser.add_argument('--resume-extraction', action='store_true', help='Reuse only this output workspace’s own prior extraction.')
    args = parser.parse_args()
    gta, er = args.gta.expanduser().resolve(), args.elden_ring.expanduser().resolve()
    if not (gta / 'GTA5.exe').is_file():
        raise ValueError('GTA V Legacy folder must contain GTA5.exe.')
    if not (er / 'eldenring.exe').is_file():
        raise ValueError('Elden Ring Game folder must contain eldenring.exe.')
    args.gta, args.elden_ring = gta, er
    args.tool_root = args.tool_root.expanduser().resolve()
    if args.stage == 'all':
        from full_assets import resolve_tools
        resolve_tools(args)
    output = safe_workspace(args.output, (gta, er))
    # A failed rerun must never leave an old completion receipt.
    (output / 'asset-build.json').unlink(missing_ok=True)
    install_recipes(output)
    env = {**os.environ, 'IGOBYGOD_BUILD_ROOT': str(output)}
    exporter = output / 'Tools/OwnedGtaExporter'
    exporter.mkdir(parents=True, exist_ok=True)
    for source in (HERE / 'exporter').iterdir():
        if source.is_file() and source.suffix in ('.cs', '.csproj'):
            shutil.copyfile(source, exporter / source.name)
    codewalker = args.tool_root / 'CodeWalker-src'
    if not (codewalker / 'CodeWalker.Core/CodeWalker.Core.csproj').is_file():
        raise ValueError('Pinned CodeWalker sources are missing under --tool-root/CodeWalker-src.')
    run([args.dotnet, 'build', exporter / 'GtaOwnedAssets.csproj', '-c', 'Release',
         '-p:CodeWalkerRoot=' + str(codewalker.resolve()), '--nologo', '-v:q'])
    stamp = source_stamp(gta)
    stamp_path = output/'gta-source.local.json'
    if stamp_path.exists() and json.loads(stamp_path.read_text()) != stamp:
        raise ValueError('This workspace belongs to different or changed game files. Choose a new output folder.')
    if not args.resume_extraction:
        run([args.dotnet, exporter / 'bin/Release/net8.0/GtaOwnedAssets.dll', '--gta', gta, '--output', output])
        stamp_path.write_text(json.dumps(stamp,indent=2)+'\n')
    else:
        verify_extraction(output, stamp)
    decode_exports(output, args.python, env)
    convert_core(output, args.python, env)
    records = []
    for relative in CORE_OUTPUTS:
        path = output / relative
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError('Required generated file missing: ' + relative)
        records.append({'path': relative, 'bytes': path.stat().st_size,
                        'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    result = {'owner': 'igobygod', 'stage': 'gta-core', 'status': 'complete', 'files': records,
              'full_mod_ready': False, 'gameplay_tested': False}
    (output / 'gta-core-build.json').write_text(json.dumps(result, indent=2))
    if args.stage != 'gta-core':
        from full_assets import er_build, assemble
        er_build(args, output, env)
        convert_gta_extras(output, args.python, env, args.java, args.ffdec)
        assemble(output)
        return
    print('GTA core assets generated locally. This stage is not a complete playable mod.')

if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print('Asset build stopped: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
