"""Check the source snapshot, or refresh it after intentional source edits."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BLOCKED = {'.exe', '.dll', '.bin', '.rgba', '.pcm', '.dcx', '.rpf', '.bdt', '.bhd', '.zip', '.pdb', '.pyc', '.sl2', '.dds', '.ytd', '.yft', '.ydd', '.ydr', '.ycd', '.awc'}
CACHES = {'bin', 'obj', 'target', '__pycache__', '.git', '.venv'}


def source_files(root):
    files = []
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError('Source symlink: ' + str(path.relative_to(root)))
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if path.suffix.lower() in BLOCKED or CACHES.intersection(relative.parts) or path.name.startswith('.env') or path.name.endswith('.local.json'):
            raise ValueError('Non-source file: ' + str(relative))
        files.append(path)
    return files


def manifest(root, paths):
    return {'author': 'igobygod', 'files': [
        {'path': p.relative_to(root).as_posix(), 'bytes': p.stat().st_size,
         'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
        for p in paths if p.name != 'SOURCE_MANIFEST.json'
    ]}


def check_manifest(root):
    spec = json.loads((root / 'SOURCE_MANIFEST.json').read_text())
    for item in spec['files']:
        path = root / item['path']
        data = path.read_bytes()
        if len(data) != item['bytes'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError('Source manifest mismatch: ' + item['path'])
    expected = {item['path'] for item in spec['files']} | {'SOURCE_MANIFEST.json'}
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file() and not CACHES.intersection(p.relative_to(root).parts)}
    if actual != expected:
        raise ValueError('Source manifest inventory differs: ' + str(sorted(actual ^ expected)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh', action='store_true', help='Regenerate source manifests and embedded payload after intentional edits. Does not replace the preview EXE.')
    args = parser.parse_args()
    builder, installer = ROOT / 'builder', ROOT / 'installer'
    files = source_files(builder)
    if args.refresh:
        (builder / 'SOURCE_MANIFEST.json').write_text(json.dumps(manifest(builder, files), indent=2) + '\n', encoding='utf-8')
        files = source_files(builder)
        with zipfile.ZipFile(installer / 'payload.zip', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for path in files:
                info = zipfile.ZipInfo('builder/' + path.relative_to(builder).as_posix(), (2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, path.read_bytes())
        for name in ('release.json', 'windows_worker.py'):
            (installer / name).write_bytes((builder / name).read_bytes())
        tracked = sorted(p for p in installer.rglob('*') if p.is_file() and not CACHES.intersection(p.relative_to(installer).parts))
        (installer / 'SOURCE_MANIFEST.json').write_text(json.dumps(manifest(installer, tracked), indent=2) + '\n', encoding='utf-8')
    check_manifest(builder)
    check_manifest(installer)
    expected = {'builder/' + p.relative_to(builder).as_posix(): p.read_bytes() for p in files}
    with zipfile.ZipFile(installer / 'payload.zip') as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or set(names) != set(expected):
            raise ValueError('Payload inventory mismatch')
        for name, data in expected.items():
            if archive.read(name) != data:
                raise ValueError('Payload source mismatch: ' + name)
    for name in ('release.json', 'windows_worker.py'):
        if (installer / name).read_bytes() != (builder / name).read_bytes():
            raise ValueError('Embedded resource mismatch: ' + name)
    print('Source manifests and embedded payload match: ' + str(len(files)) + ' builder files.')


if __name__ == '__main__':
    main()
