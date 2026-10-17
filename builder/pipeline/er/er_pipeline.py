"""igobygod: build ER-derived packets locally; no proprietary payload is bundled."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, struct, subprocess, sys

RESOURCES = [
 '/chr/c0000.anibnd.dcx', '/chr/c0000.chrbnd.dcx',
 '/chr/c0000_a00_hi.anibnd.dcx', '/chr/c0000_a00_lo.anibnd.dcx',
 '/chr/c0000_a2x.anibnd.dcx', '/chr/c0000_a4x.anibnd.dcx',
 '/parts/wp_a_0223.partsbnd.dcx', '/msg/engus/menu.msgbnd.dcx', '/menu/01_000_fe.gfx',
 '/menu/71_maptile.tpfbhd', '/menu/71_maptile.tpfbdt', '/menu/71_maptile.mtmskbnd.dcx',
]

def winpath(path):
    return str(Path(path).resolve()) if os.name == 'nt' else 'Z:' + str(Path(path).resolve()).replace('/', '\\')

def run(argv, env=None):
    subprocess.run([str(v) for v in argv], check=True, env=env)

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()

def validate_workspace(workspace, game):
    workspace,game=workspace.resolve(),game.resolve()
    if workspace==game or workspace in game.parents or game in workspace.parents:
        raise ValueError('ER workspace must be separate from the selected game directory')
    return workspace,game

def extraction_receipt(args):
    return {'schema':1,'game_headers':{n:digest(args.game/(n+'.bhd')) for n in ('Data0','Data1','Data2','Data3')},
            'resources':{n:digest(args.workspace/'Assets/ER'/n.lstrip('/')) for n in RESOURCES}}

def decode(args, src, dst):
    raw = src.read_bytes()
    dst.parent.mkdir(parents=True, exist_ok=True)
    if raw[:4] != b'DCX\0':
        dst.write_bytes(raw)
    elif raw[40:44] == b'KRAK':
        if dst.exists():
            raise FileExistsError('Decoded output already exists: ' + str(dst))
        expected = struct.unpack_from('>I', raw, 28)[0]
        helper = [winpath(args.oodle_helper), winpath(args.game/'oo2core_6_win64.dll'), winpath(src), winpath(dst), str(expected)]
        if os.name == 'nt':
            helper = [str(args.oodle_helper), str(args.game/'oo2core_6_win64.dll'), str(src), str(dst), str(expected)]
        else:
            if not args.wine or not args.bottle:
                raise ValueError('macOS KRAK decoding requires --wine and --bottle')
            helper = [args.wine, '--bottle', args.bottle, '--', *helper]
        run(helper)
    else:
        from soulstruct.dcx import decompress
        dst.write_bytes(decompress(raw)[0])

def unpack(args):
    args.workspace,args.game=validate_workspace(args.workspace,args.game)
    root = args.workspace
    source = root/'Assets/ER'
    receipt=root/'er-extraction.local.json'
    if not args.skip_archive_extract:
        if source.exists() and any(source.iterdir()):
            raise ValueError('ER extraction output exists; choose a fresh workspace')
        run([args.dotnet, args.extractor, args.game, source, *RESOURCES])
        receipt.write_text(json.dumps(extraction_receipt(args),indent=2)+'\n')
    elif not receipt.is_file() or json.loads(receipt.read_text())!=extraction_receipt(args):
        raise ValueError('Prior extraction does not match the selected game and requested resources')
    targets = {
        'chr/c0000.anibnd.dcx':'Build/c0000.anibnd.decompressed',
        'chr/c0000.chrbnd.dcx':'Build/c0000.chrbnd.decompressed',
        'chr/c0000_a00_hi.anibnd.dcx':'Build/native-animation-source/c0000_a00_hi.anibnd',
        'chr/c0000_a00_lo.anibnd.dcx':'Build/native-animation-source/c0000_a00_lo.anibnd',
        'chr/c0000_a4x.anibnd.dcx':'Build/native-animation-source/c0000_a4x.anibnd',
        'chr/c0000_a2x.anibnd.dcx':'PlayerRenderer/polish-v4.4/melee-assets/c0000_a2x.anibnd',
        'parts/wp_a_0223.partsbnd.dcx':'Build/wp_a_0223.partsbnd',
    }
    for src, dst in targets.items():
        decode(args, source/src, root/dst)
    from soulstruct.containers import Binder
    def member(binder_path, suffix, target):
        binder=Binder.from_path(binder_path)
        candidates=[e for e in binder.entries if e.name.lower().endswith(suffix.lower())]
        if len(candidates)!=1:
            raise ValueError('Expected exactly one '+suffix)
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(candidates[0].get_uncompressed_data())
    member(root/'Build/c0000.chrbnd.decompressed','.flver',root/'Assets/ER/chr/c0000.flver')
    sword=root/'PlayerRenderer/polish-v4.3/sword-prototype'
    member(root/'Build/wp_a_0223.partsbnd','.flver',sword/'WP_A_0223.flver')
    member(root/'Build/wp_a_0223.partsbnd','.tpf',sword/'WP_A_0223.tpf')
    shutil.copy2(source/'parts/wp_a_0223.partsbnd.dcx',sword/'wp_a_0223.partsbnd.dcx')
    print('ER action archives and sword extracted locally.',flush=True)

def validate_packet(raw):
    import numpy as np
    magic,count,nv=struct.unpack_from('<III',raw)
    if magic!=0x524f4c4c or not 0<count<=64 or not 0<nv<=100000:
        raise ValueError('Invalid action packet')
    at=12;records={}
    for _ in range(count):
        start=at;aid,nf,duration=struct.unpack_from('<IIf',raw,at);at+=12
        if not 2<=nf<=240 or not 0<duration<120 or aid in records:
            raise ValueError('Invalid action clip')
        values=np.frombuffer(raw,'<f4',nf*nv*3,at)
        if not np.isfinite(values).all():raise ValueError('Nonfinite animation')
        at+=nf*nv*12;records[aid]=raw[start:at]
    if at!=len(raw):raise ValueError('Unexpected action bytes')
    return nv,records

def actions(args):
    root=args.workspace;env=dict(os.environ,IGOBYGOD_BUILD_ROOT=str(root))
    if not args.sword_absorption or not args.sword_absorption.is_file():
        raise ValueError('actions requires --sword-absorption from the local ERParams run')
    absorption=root/'PlayerRenderer/polish-v4.6/aim/sword-absorption.json'
    absorption.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(args.sword_absorption,absorption)
    templates=Path(__file__).parent/'templates'
    for path in templates.rglob('*'):
        if path.is_file():
            out=root/path.relative_to(templates);out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,out)
    stages=[
      'PlayerRenderer/extract_actual_roll.py','PlayerRenderer/extract_jump.py',
      'PlayerRenderer/build_roll.py','PlayerRenderer/build_jump.py',
      'PlayerRenderer/repair-next/death/extract_candidates.py','PlayerRenderer/repair-next/death/build_death.py',
      'PlayerRenderer/polish-v4.3/sword-prototype/prepare_mesh.py',
      'PlayerRenderer/polish-v4.4/melee-assets/build_melee.py','PlayerRenderer/polish-v4.4/melee-assets/append_gaits.py',
      'PlayerRenderer/polish-v4.4/melee-assets/pack_sword.py',
      'PlayerRenderer/polish-v4.5/sword/build_carry.py','PlayerRenderer/polish-v4.5/sword/emit_helper.py',
      'PlayerRenderer/polish-v4.6/sword/build_attack.py',
    ]
    for name in stages:
        print('Local action build: '+name,flush=True);run([sys.executable,root/name],env)
    p=root/'PlayerRenderer';nv,rolls=validate_packet((p/'roll.bin').read_bytes());jv,jumps=validate_packet((p/'jump.bin').read_bytes())
    if nv!=jv or set(rolls)&set(jumps):raise ValueError('Incompatible roll and jump packets')
    if 202126 not in jumps:
        base=jumps[202125];jumps[202126]=struct.pack('<I',202126)+base[4:]
    if len(rolls)+len(jumps)>64:raise ValueError('Action clip limit exceeded')
    packet=struct.pack('<III',0x524f4c4c,len(rolls)+len(jumps),nv)+b''.join(rolls.values())+b''.join(jumps.values())
    validate_packet(packet);(p/'roll.bin').write_bytes(packet)
    stage=p/'rifle-stage';stage.mkdir(parents=True,exist_ok=True)
    for src,dst in [
      ('repair-next/death/death.bin','death.bin'),
      ('polish-v4.6/sword/melee-joints.bin','melee-joints.bin'),
      ('polish-v4.4/melee-assets/sword-fallback-attachment.bin','sword-fallback-attachment.bin'),
      ('polish-v4.4/melee-assets/sword-mesh.bin','sword-mesh.bin'),
      ('polish-v4.4/melee-assets/sword.rgba','sword.rgba'),
    ]:shutil.copy2(p/src,stage/dst)
    # Sampled palm coordinate is produced locally rather than shipped in a header.
    report=json.loads((p/'polish-v4.5/sword/carry-report.json').read_text())
    # Runtime uses PH_R_Hand coordinates. The report's raw handle center is in
    # SKEL_R_Hand coordinates; attachment already applies the local hand offset.
    attachment=report['attachment']
    palm=','.join(f'{v:.9f}f' for v in (attachment[0][3],0,attachment[2][3]))
    source=p/'polish-v4.6/sword/sword_attack.h.in'
    source.with_suffix('').write_text(source.read_text().replace('@PALM@',palm))
    print('Action packets built.',flush=True)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('stage',choices=['extract','actions'])
    ap.add_argument('--workspace',type=Path,required=True)
    ap.add_argument('--game',type=Path)
    ap.add_argument('--dotnet',default='dotnet')
    ap.add_argument('--extractor',type=Path)
    ap.add_argument('--oodle-helper',type=Path)
    ap.add_argument('--wine');ap.add_argument('--bottle')
    ap.add_argument('--skip-archive-extract',action='store_true')
    ap.add_argument('--sword-absorption',type=Path,help='Local ERParams sword-absorption.json')
    args=ap.parse_args();args.workspace=args.workspace.resolve()
    if args.stage=='extract':
        if not args.game or not args.extractor or not args.oodle_helper:ap.error('extract requires --game, --extractor and --oodle-helper')
        args.game=args.game.resolve();unpack(args)
    else:actions(args)

if __name__=='__main__':main()
