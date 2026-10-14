"""Assemble only freshly reconstructed local assets for the igobygod runtime."""
from pathlib import Path, PurePosixPath
import hashlib, importlib.util, json, os, shutil, subprocess

HERE = Path(__file__).resolve().parent
AUDIO = ('rifle','pistol','launcher','explosion','smg','shotgun','sniper','wasted')


def run(command, env=None):
    subprocess.run([str(x) for x in command], check=True, env=env)


def metadata(path):
    path = Path(path)
    if not path.is_file() or not path.stat().st_size:
        raise ValueError('Required generated asset is missing: '+str(path))
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            digest.update(chunk)
    return {'bytes':path.stat().st_size,'sha256':digest.hexdigest()}


def inside(root, relative):
    rel = PurePosixPath(relative)
    if rel.is_absolute() or not rel.parts or '..' in rel.parts or '\\' in relative or ':' in relative:
        raise ValueError('Invalid package resource path: '+relative)
    result = root.joinpath(*rel.parts)
    if root.resolve() not in result.resolve().parents:
        raise ValueError('Resource path escapes private output: '+relative)
    return result


def copy(root, source, target):
    src, dst = inside(root, source), inside(root, target)
    metadata(src)
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dst)


def resolve_tools(args):
    defaults = {
      'java': shutil.which('java') or 'java',
      'ffdec': args.tool_root/'ffdec/ffdec.jar',
      'oodle_helper': args.tool_root/'bin/igobygod-oodle-helper.exe',
      'er_extractor': args.tool_root/'bin/ERAssets.dll',
      'er_params': args.tool_root/'bin/ERParams.dll',
      'soulsformats_dll': args.tool_root/'SoulsFormats/SoulsFormats/bin/Release/netstandard2.1/SoulsFormats.dll',
      'paramdefs': args.tool_root/'Paramdex/ER/Defs',
    }
    for key, default in defaults.items():
        if getattr(args,key,None) is None:
            setattr(args,key,default)
    for key in ('ffdec','oodle_helper','er_extractor','er_params','soulsformats_dll'):
        value = Path(getattr(args,key)).expanduser().resolve()
        if not value.is_file():
            raise ValueError('Missing prepared tool --'+key.replace('_','-')+': '+str(value))
        setattr(args,key,value)
    args.paramdefs = Path(args.paramdefs).expanduser().resolve()
    if not args.paramdefs.is_dir():
        raise ValueError('Missing original parameter definitions under --paramdefs.')
    if os.name != 'nt' and (not args.wine or not args.bottle):
        raise ValueError('macOS asset construction needs --wine and --bottle for the selected game decoder.')


def er_build(args, output, env):
    helper = HERE/'er/er_pipeline.py'
    if not helper.is_file():helper = HERE.parent/'audit/ERPipeline/er_pipeline.py'
    if not helper.is_file():
        raise ValueError('Missing ERPipeline source; extract the complete setup package.')
    package = output/'Package'
    regulation = output/'Build/ERParams/regulation.bin'
    fingerprints = output/'Build/ERParams/expected_params.h'
    regulation.parent.mkdir(parents=True, exist_ok=True)
    # This directory belongs to this marked local workspace; reruns may replace
    # only these derived outputs. Source regulation is opened read-only.
    regulation.unlink(missing_ok=True); fingerprints.unlink(missing_ok=True)
    run([args.dotnet,args.er_params,'--input',args.elden_ring/'regulation.bin',
         '--output',regulation,'--paramdefs',args.paramdefs,'--fingerprints',fingerprints],env)
    copy(output,'Build/ERParams/expected_params.h','PlayerRenderer/polish-v4.6/Params/expected_params.h')
    copy(output,'Build/ERParams/sword-absorption.json','PlayerRenderer/polish-v4.6/aim/sword-absorption.json')
    extract=[args.python,helper,'extract','--workspace',output,'--game',args.elden_ring,
             '--dotnet',args.dotnet,'--extractor',args.er_extractor,'--oodle-helper',args.oodle_helper]
    if args.wine:extract += ['--wine',args.wine]
    if args.bottle:extract += ['--bottle',args.bottle]
    run(extract,env)
    run([args.python,helper,'actions','--workspace',output,'--sword-absorption',output/'Build/ERParams/sword-absorption.json'],env)
    terrain=[args.python,HERE/'terrain_assets.py','--workspace',output,'--game',args.elden_ring,
             '--dotnet',args.dotnet,'--soulsformats-dll',args.soulsformats_dll,'--oodle-helper',args.oodle_helper]
    if args.wine:terrain += ['--wine',args.wine]
    if args.bottle:terrain += ['--bottle',args.bottle]
    run(terrain,env)
    messages=[args.python,HERE/'patch_messages.py','--workspace',output,'--game',args.elden_ring,'--oodle-helper',args.oodle_helper]
    if args.wine:messages += ['--wine',args.wine]
    if args.bottle:messages += ['--bottle',args.bottle]
    run(messages,env)
    patcher=HERE.parent/'patch_hud.py'
    if not patcher.is_file():patcher=HERE.parent/'setup/patch_hud.py'
    run([args.python,patcher,'--input',output/'Assets/ER/menu/01_000_fe.gfx',
         '--output',package/'assets/menu/01_000_fe.gfx'],env)
    copy(output,'Build/ERParams/regulation.bin','Package/assets/regulation.bin')


def assemble(output):
    mapping=json.loads((HERE.parent/'runtime/runtime-data-map.json').read_text())
    required=[]
    for source,target in mapping.items():
        if target.startswith('logs/') or '%' in source:
            continue
        required.append(('runtime/'+target,source))
    for name in AUDIO:
        item=('runtime/data/audio/'+name+'.pcm','PlayerRenderer/rifle-stage/audio/'+name+'.pcm')
        if item not in required:required.append(item)
    for target,source in required:
        copy(output,source,'Package/'+target)
    headers=json.loads((HERE.parent/'runtime/generated-headers.json').read_text())
    for relative in headers:
        copy(output,relative,'generated/'+relative)
    files={name:metadata(output/'Package'/name) for name,_ in required}
    for relative in ('assets/regulation.bin','assets/menu/01_000_fe.gfx','assets/msg/engus/menu.msgbnd.dcx'):
        files[relative]=metadata(output/'Package'/relative)
    report={'author':'igobygod','status':'complete','asset_build_complete':True,
            'full_mod_ready':False,'gameplay_tested':False,
            'files':files,'generated_headers':{rel:metadata(output/'generated'/rel) for rel in headers},
            'note':'Private local assets reconstructed from the selected owned games. Runtime compilation and installer validation are separate.'}
    (output/'asset-build.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('All required local asset packets and generated headers are ready for runtime compilation.',flush=True)
    return report
