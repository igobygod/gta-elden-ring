"""Reconstruct the original Limgrave atlas from locally owned Elden Ring data."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, struct, subprocess


def run(command):
    subprocess.run([str(x) for x in command], check=True)


def windows_path(path):
    path = Path(path).resolve()
    return str(path) if os.name == 'nt' else 'Z:' + str(path).replace('/', '\\')


def decode_batch(args, pairs, work):
    jobs = []
    for source, target in pairs:
        target.parent.mkdir(parents=True, exist_ok=True)
        data = source.read_bytes()
        if data[:4] != b'DCX\0':
            target.write_bytes(data)
        elif data[40:44] == b'KRAK':
            # These are derived scratch outputs owned by this build, never game files.
            target.unlink(missing_ok=True)
            expected = struct.unpack_from('>I', data, 28)[0]
            jobs.append((windows_path(source), windows_path(target), str(expected)))
        else:
            from soulstruct.dcx import decompress
            target.write_bytes(decompress(data)[0])
    if jobs:
        manifest = work / 'decode-jobs.tsv'
        if any('\t' in value or '\n' in value for job in jobs for value in job):
            raise ValueError('Decoder paths cannot contain tabs or line breaks.')
        manifest.write_text(''.join('\t'.join(job) + '\n' for job in jobs), encoding='utf-8')
        command = [windows_path(args.oodle_helper), '--batch', windows_path(args.game / 'oo2core_6_win64.dll'), windows_path(manifest)]
        if os.name != 'nt':
            if not args.wine or not args.bottle:
                raise ValueError('macOS decoding requires --wine and --bottle.')
            command = [args.wine, '--bottle', args.bottle, '--', *command]
        run(command)


def build(args):
    from PIL import Image
    root = args.workspace.resolve()
    work = root / 'PlayerRenderer/polish-fix/map'
    work.mkdir(parents=True, exist_ok=True)
    helper = root / 'Tools/TerrainAssets'
    helper.mkdir(parents=True, exist_ok=True)
    for source in (Path(__file__).parent / 'terrain').iterdir():
        if source.suffix in ('.cs', '.csproj'):
            shutil.copy2(source, helper / source.name)
    run([args.dotnet, 'build', helper/'TerrainAssets.csproj', '-c', 'Release', '-p:SoulsFormatsDll='+str(args.soulsformats_dll.resolve()), '--nologo', '-v:q'])
    exe = helper/'bin/Release/net8.0/TerrainAssets.dll'
    decode_batch(args, [(root/'Assets/ER/menu/71_maptile.mtmskbnd.dcx', work/'masks.raw')], work)
    run([args.dotnet, exe, 'select', root, work])
    rows = json.loads((work/'tiles.json').read_text())
    decode_batch(args, [(work/'compressed'/r['file'], work/'compressed'/(r['file']+'.raw')) for r in rows], work)
    run([args.dotnet, exe, 'headerize', root, work])
    atlas = Image.new('RGBA', (4096, 4096))
    for row in rows:
        name = row['file'].removesuffix('.tpf.dcx') + '.dds'
        with Image.open(work/'dds'/name) as source:
            tile = source.convert('RGBA')
            if tile.size != (256, 256):
                raise ValueError('Unexpected original map tile dimensions: '+name)
            atlas.paste(tile, ((row['x']-7)*256, (18-row['y'])*256))
    raw = atlas.tobytes()
    target = root/'PlayerRenderer/rifle-stage/terrain-map.rgba'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(raw)
    atlas.resize((1024,1024)).save(work/'preview.png')
    report={'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'tiles':len(rows),'map':'Original Limgrave, exact original fragment masks','gameplay_tested':False}
    (work/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',type=Path,required=True)
    p.add_argument('--game',type=Path,required=True)
    p.add_argument('--dotnet',default='dotnet')
    p.add_argument('--soulsformats-dll',type=Path,required=True)
    p.add_argument('--oodle-helper',type=Path,required=True)
    p.add_argument('--wine');p.add_argument('--bottle')
    build(p.parse_args())

if __name__=='__main__':main()
