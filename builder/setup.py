#!/usr/bin/env python3
"""igobygod local mod builder. Game files stay on the viewer's computer."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import platform
import shutil
import subprocess
import sys
import tempfile

GTA_REQUIRED = ('GTA5.exe', 'common.rpf', 'x64c.rpf', 'x64e.rpf', 'x64v.rpf')
ER_REQUIRED = ('eldenring.exe', 'regulation.bin', 'Data0.bhd', 'Data0.bdt',
               'Data1.bhd', 'Data1.bdt', 'Data2.bhd', 'Data2.bdt', 'Data3.bhd', 'Data3.bdt')
PACKAGE = Path(__file__).resolve().parent

class SetupError(Exception):
    pass

def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''): h.update(chunk)
    return h.hexdigest()

def resolve_games(gta, er):
    gta, er = Path(gta).expanduser().resolve(), Path(er).expanduser().resolve()
    if not (er/'eldenring.exe').is_file() and (er/'Game'/'eldenring.exe').is_file(): er /= 'Game'
    for label, root, names in [('GTA V Legacy', gta, GTA_REQUIRED), ('Elden Ring', er, ER_REQUIRED)]:
        missing = [name for name in names if not (root/name).is_file() or (root/name).stat().st_size == 0]
        if missing: raise SetupError(f'{label}: select the installed game directory; missing: {", ".join(missing)}')
    return gta, er

def validate_executable(exe, expected):
    digest = sha256(exe)
    if digest != expected:
        raise SetupError('Elden Ring executable is unsupported by this build. '
                         'Nothing was installed. Do not bypass the version check. '
                         f'Detected SHA-256: {digest}')
    return digest

def validate_output(output, protected):
    output = Path(output).expanduser().resolve()
    for item in protected:
        item = Path(item).resolve()
        if output == item or item in output.parents or output in item.parents:
            raise SetupError('Choose a separate output folder outside both games and the setup package.')
    if output.exists(): raise SetupError('Output folder already exists. Choose a new folder; existing installations are preserved.')
    return output

def local_path(stage, name):
    rel = PurePosixPath(name)
    if '\\' in name or ':' in name or rel.is_absolute() or '..' in rel.parts or not rel.parts:
        raise SetupError(f'Invalid payload path: {name}')
    target = Path(stage).joinpath(*rel.parts)
    if Path(stage).resolve() not in target.resolve().parents:
        raise SetupError(f'Payload escapes output: {name}')
    return target

def validate_payload(stage, requirements):
    result = {}
    mismatches = []
    for item in requirements:
        p = local_path(stage, item['path'])
        if not p.is_file(): raise SetupError(f'Generated payload is missing {item["path"]}')
        size = p.stat().st_size
        if size < item.get('min_bytes', 1): raise SetupError(f'Generated payload is too small: {item["path"]}')
        if 'bytes' in item and size != item['bytes']: raise SetupError(f'Wrong generated size: {item["path"]}')
        digest = sha256(p)
        expected = item.get('platform_sha256', {}).get(platform.system(), item.get('sha256'))
        if expected is not None and expected != digest:
            mismatches.append(item["path"])
            print(f'Fingerprint mismatch: {item["path"]}; expected {expected}; generated {digest}', flush=True)
        result[item['path']] = {'bytes': size, 'sha256': digest}
    if mismatches:
        raise SetupError("Generated payload differs from validated build: " + ", ".join(mismatches))
    return result

def commit_install(staged, destination):
    destination = Path(destination)
    if destination.exists() or destination.is_symlink():
        raise SetupError('Destination appeared during build; existing files were left intact.')
    Path(staged).rename(destination)

def wine_path(path):
    # CrossOver's default Z: drive maps the host root. Checked by the launch preflight.
    return 'Z:' + str(Path(path).resolve()).replace('/', '\\')

def launch_arguments(system, me3, exe, profile, *, wine=None, bottle=None):
    tail = ['launch', '--game', 'eldenring', '--exe', str(exe), '--profile', str(profile),
            '--disable-arxan', 'false', '--show-logos', 'true', '--heap-size', '4096']
    if system == 'Windows': return [str(me3)] + tail
    if system != 'Darwin': raise SetupError('Supported setup hosts: Windows and macOS with CrossOver.')
    if not wine or not bottle: raise SetupError('Select CrossOver and the bottle that already runs Elden Ring.')
    tail[4] = wine_path(exe)
    tail[6] = wine_path(profile)
    return [str(wine), '--bottle', str(bottle), '--dll',
            'vcruntime140,vcruntime140_1,msvcp140,msvcp140_1,msvcp140_2=n,b',
            '--workdir', str(Path(exe).parent), '--', str(me3)] + tail

def choose_directory(title):
    picker_errors = (ImportError, RuntimeError)
    try:
        import tkinter as tk
        picker_errors += (tk.TclError,)
        from tkinter import filedialog
        root = tk.Tk(); root.withdraw()
        try: value = filedialog.askdirectory(title=title, mustexist=True, parent=root)
        finally: root.destroy()
        if not value: raise SetupError('Setup cancelled. No game files were changed.')
        return Path(value)
    except picker_errors as error:
        if not sys.stdin.isatty(): raise SetupError('Folder picker unavailable. Pass --gta and --elden-ring paths.') from error
        value = input(title + '\nPaste folder path: ').strip().strip('"')
        if not value: raise SetupError('Setup cancelled.')
        return Path(value)

def run(command, *, cwd=None, env=None):
    # Argument arrays only: user-provided paths are never evaluated by a shell.
    try: subprocess.run([str(x) for x in command], cwd=cwd, env=env, check=True)
    except FileNotFoundError as error: raise SetupError(f'Required program is missing: {command[0]}') from error
    except subprocess.CalledProcessError as error: raise SetupError(f'Build step failed (exit {error.returncode}): {command[0]}') from error

def read_manifest():
    try: return json.loads((PACKAGE/'release.json').read_text())
    except (OSError, ValueError) as error: raise SetupError('Missing or invalid release.json; extract the complete package.') from error

def profile_text():
    return ('profileVersion = "v1"\nsavefile = "igobygod.sl2"\nstart_online = false\n\n'
            '[[supports]]\ngame = "eldenring"\n\n[[packages]]\nid = "igobygod-assets"\npath = "assets"\n\n'
            '[[natives]]\npath = "runtime/franklin_player_renderer.dll"\n')

def check_prerequisites(system):
    commands = ['git', 'dotnet', 'cargo']
    if system == 'Windows': commands.append('cl')
    elif system == 'Darwin': commands.append('clang-cl')
    missing = [c for c in commands if not shutil.which(c)]
    if missing: raise SetupError('Install build prerequisites listed in README: ' + ', '.join(missing))

def asset_arguments(package, tool_root, scratch, gta, er, *, java='java', wine=None, bottle=None):
    python = Path(tool_root)/('python/Scripts/python.exe' if os.name=='nt' else 'python/bin/python')
    args = [str(python), str(Path(package)/'pipeline/build_assets.py'), '--gta', str(gta),
            '--elden-ring', str(er), '--output', str(scratch), '--tool-root', str(tool_root), '--java', str(java)]
    if wine is not None: args += ['--wine', str(wine)]
    if bottle is not None: args += ['--bottle', str(bottle)]
    return args

def ensure_game_closed():
    if platform.system() == 'Windows':
        result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq eldenring.exe', '/FO', 'CSV'], capture_output=True, text=True, check=True)
    else:
        result = subprocess.run(['ps', '-axo', 'comm='], capture_output=True, text=True, check=True)
    if any(PureWindowsPath(line.strip().strip('"')).name.lower() == 'eldenring.exe' or line.lower().startswith('"eldenring.exe",')
           for line in result.stdout.splitlines()):
        raise SetupError('Save and close Elden Ring before installing or launching another instance.')

def launch_installed(install):
    install = Path(install).expanduser().resolve()
    try: receipt = json.loads((install/'install.local.json').read_text())
    except (ValueError, OSError) as error: raise SetupError('No completed installation receipt in that folder.') from error
    if receipt.get('completed') is not True: raise SetupError('This local build did not finish.')
    current_system = platform.system()
    if current_system != receipt['system']: raise SetupError('This installation belongs to a different platform.')
    ensure_game_closed()
    validate_executable(Path(receipt['elden_ring'])/'eldenring.exe', receipt['exe_sha256'])
    validate_payload(install, [{'path': name, **meta} for name, meta in receipt['files'].items()])
    args = launch_arguments(current_system, receipt['me3'], Path(receipt['elden_ring'])/'eldenring.exe',
                            install/'igobygod.me3', wine=receipt.get('wine'), bottle=receipt.get('bottle'))
    run(args, cwd=receipt['elden_ring'])

def main(argv=None):
    parser=argparse.ArgumentParser(description='igobygod GTA × Elden Ring local setup')
    parser.add_argument('--gta', type=Path, help='Owned GTA V Legacy installation')
    parser.add_argument('--elden-ring', type=Path, help='Owned Elden Ring installation or Game folder')
    parser.add_argument('--output', type=Path, help='New output folder, separate from both games')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true', help='Validate selected games; make no changes')
    mode.add_argument('--build', action='store_true', help='Build a verified, complete release from local games')
    parser.add_argument('--tool-root', type=Path, help='Locally prepared pinned build dependencies')
    parser.add_argument('--me3', type=Path, help='ME3 0.13.0 executable')
    parser.add_argument('--java', default='java', help='Java 21 executable used by the local asset builder')
    parser.add_argument('--bottle', type=Path, help='CrossOver bottle that already runs Elden Ring')
    parser.add_argument('--crossover', type=Path, default=Path('/Applications/CrossOver.app'))
    parser.add_argument('--launch', type=Path, metavar='INSTALL', help='Launch a completed local installation')
    args = parser.parse_args(argv)
    if args.launch: launch_installed(args.launch); return 0
    system=platform.system()
    if system not in ('Windows','Darwin'): raise SetupError('Use Windows or macOS with CrossOver.')
    manifest=read_manifest()
    print('igobygod — local game-folder setup. Original game files remain untouched.', flush=True)
    gta, er = resolve_games(args.gta or choose_directory('Select GTA V Legacy (contains GTA5.exe)'),
                           args.elden_ring or choose_directory('Select Elden Ring (contains Game or eldenring.exe)'))
    digest=validate_executable(er/'eldenring.exe', manifest['elden_ring_sha256'])
    print('Both game folders and the supported Elden Ring executable passed verification.', flush=True)
    if not args.build:
        print('Check finished. Use --build after reviewing README and release status.'); return 0
    platform_status=manifest.get('platforms',{}).get(system,{})
    if not platform_status.get('build_ready'):
        blockers=platform_status.get('blockers',manifest.get('blockers',['This platform has not passed its clean-build acceptance test.']))
        raise SetupError('Installation is not enabled for this platform:\n- ' + '\n- '.join(blockers))
    if not manifest.get('release_ready'):
        print('Installing Windows preview.',flush=True)
    if not args.output: raise SetupError('Specify --output with a new folder for the local mod.')
    output=validate_output(args.output, [gta, er, PACKAGE])
    check_prerequisites(system); ensure_game_closed()
    if not args.tool_root: raise SetupError('Specify --tool-root. See README for dependency setup.')
    args.tool_root = args.tool_root.expanduser().resolve()
    if args.me3 is None: args.me3 = args.tool_root/'me3/bin/me3.exe'
    if not args.me3.is_file(): raise SetupError('Selected ME3 executable is missing.')
    wine=args.crossover/'Contents/SharedSupport/CrossOver/bin/wine'
    if system=='Darwin':
        if not wine.is_file() or not args.bottle or not args.bottle.is_dir(): raise SetupError('Select a valid CrossOver application and existing game bottle.')
        if not (args.bottle/'dosdevices'/'z:').is_symlink() or (args.bottle/'dosdevices'/'z:').resolve()!=Path('/'):
            raise SetupError('The selected bottle must map drive Z: to the macOS root for the generated launcher.')
    output.parent.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(output.parent).free < 16*1024**3: raise SetupError('At least 16 GB free working space is required.')
    with tempfile.TemporaryDirectory(prefix='.igobygod-build-', dir=output.parent) as temp:
        work=Path(temp); stage=work/'install'; assets_build=work/'assets-build'
        asset_env=os.environ.copy();asset_env['PYTHONDONTWRITEBYTECODE']='1'
        run(asset_arguments(PACKAGE, args.tool_root, assets_build, gta, er, java=args.java,
                            wine=wine if system=='Darwin' else None, bottle=args.bottle),env=asset_env)
        if not (assets_build/'Package').is_dir(): raise SetupError('Asset builder did not produce a complete package.')
        shutil.copytree(assets_build/'Package', stage)
        source=work/'source'; shutil.copytree(PACKAGE/'runtime', source)
        env=os.environ.copy(); env['IGOBYGOD_GENERATED_ROOT']=str(assets_build/'generated'); env['CARGO_TARGET_DIR']=str(work/'target')
        command=['cargo'] + (['xwin'] if system=='Darwin' else []) + ['build','--release','--locked','--target','x86_64-pc-windows-msvc','-j1']
        run(command,cwd=source/'PlayerRenderer',env=env)
        (stage/'runtime').mkdir(exist_ok=True)
        shutil.copy2(work/'target/x86_64-pc-windows-msvc/release/franklin_player_renderer.dll',stage/'runtime/franklin_player_renderer.dll')
        files=validate_payload(stage,manifest['payload'])
        (stage/'igobygod.me3').write_text(profile_text(),encoding='utf-8')
        files['igobygod.me3']={'bytes':(stage/'igobygod.me3').stat().st_size,'sha256':sha256(stage/'igobygod.me3')}
        # Absolute paths belong only in the viewer's private local receipt, never the public archive.
        receipt={'completed':True,'system':system,'author':'igobygod','exe_sha256':digest,
                 'elden_ring':str(er),'gta':str(gta),'me3':str(args.me3.resolve()),'files':files,
                 'wine':str(wine) if system=='Darwin' else None,'bottle':str(args.bottle.resolve()) if args.bottle else None}
        (stage/'install.local.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
        shutil.copy2(__file__,stage/'setup.py')
        commit_install(stage,output)
    print(f'Local build installed: {output}\nLaunch with: python setup.py --launch "{output}"')
    return 0

if __name__=='__main__':
    try: sys.exit(main())
    except (SetupError, OSError, subprocess.SubprocessError) as error:
        print(f'Setup stopped: {error}',file=sys.stderr); sys.exit(1)
    except KeyboardInterrupt:
        print('\nSetup cancelled. Original game files remain untouched.',file=sys.stderr); sys.exit(130)
