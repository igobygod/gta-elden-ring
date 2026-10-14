#!/usr/bin/env python3
"""Fetch pinned build tools locally for igobygod; no game assets are downloaded."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import ssl
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent

def run(command, **kwargs):
    subprocess.run([str(v) for v in command], check=True, **kwargs)

def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def verify_hash(path, expected):
    if digest(path) != expected: raise ValueError('SHA-256 mismatch for '+Path(path).name)

def extract_zip(archive, output):
    output=Path(output)
    if output.exists() or output.is_symlink():raise FileExistsError('Extraction output already exists')
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            name=info.filename;relative=PurePosixPath(name)
            if relative.is_absolute() or '..' in relative.parts or ':' in name or '\\' in name:
                raise ValueError('Unsafe archive member: '+name)
            if (info.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError('Archive symlinks are not supported')
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.igobygod-extract-',dir=output.parent) as temporary:
            candidate=Path(temporary)/'complete';candidate.mkdir()
            z.extractall(candidate)
            candidate.rename(output)

def verify_extraction(archive, output):
    output=Path(output)
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            if info.is_dir():continue
            path=output/info.filename
            if not path.is_file() or path.is_symlink() or path.stat().st_size != info.file_size:
                raise ValueError('Existing tool extraction is incomplete: '+info.filename+'; choose a new tools folder.')
            h=hashlib.sha256()
            with z.open(info) as source:
                for block in iter(lambda:source.read(1024*1024),b''):h.update(block)
            if digest(path) != h.hexdigest():raise ValueError('Extracted tool differs from the verified download: '+info.filename)

def fetch_archive(root, item):
    cache=root/'downloads';cache.mkdir(exist_ok=True)
    archive=cache/item['filename']
    if not archive.is_file():
        request=urllib.request.Request(item['url'],headers={'User-Agent':'igobygod-local-builder/0.1'})
        temporary=archive.with_suffix('.partial')
        try:
            context=ssl.create_default_context()
            # macOS Python distributions may not have populated their bundled CA file.
            if platform.system() == 'Darwin' and Path('/etc/ssl/cert.pem').is_file():
                context.load_verify_locations('/etc/ssl/cert.pem')
            with urllib.request.urlopen(request,timeout=90,context=context) as source, temporary.open('wb') as target:
                shutil.copyfileobj(source,target)
            verify_hash(temporary,item['sha256']);temporary.rename(archive)
        finally:
            if temporary.exists():temporary.unlink()
    verify_hash(archive,item['sha256'])
    target=root/item['directory']
    if not target.exists():extract_zip(archive,target)
    verify_extraction(archive,target)
    return target

def checkout(root, item):
    target=root/item['directory']
    if not target.exists():
        target.mkdir()
        run(['git','init','-q',target])
        run(['git','-C',target,'remote','add','origin',item['url']])
        run(['git','-C',target,'fetch','--depth','1','origin',item['commit']])
        run(['git','-C',target,'checkout','--detach','FETCH_HEAD'])
    head=subprocess.check_output(['git','-C',target,'rev-parse','HEAD'],text=True).strip()
    origin=subprocess.check_output(['git','-C',target,'remote','get-url','origin'],text=True).strip()
    changed=subprocess.check_output(['git','-C',target,'status','--porcelain','--untracked-files=all'],text=True).strip()
    if head != item['commit'] or origin != item['url'] or changed:
        raise ValueError('Existing dependency differs from pinned source: '+item['directory'])
    return target

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True,help='Dedicated local tools folder, outside the games')
    args=parser.parse_args(argv)
    if sys.version_info[:2] != (3,13):raise ValueError('Run this tool with Python 3.13; animation decoding requires it.')
    if platform.system() not in ('Darwin','Windows'):raise ValueError('Use Windows or macOS.')
    for executable in ['git','dotnet','cargo','java']:
        if not shutil.which(executable):raise ValueError('Missing prerequisite: '+executable+' (see README)')
    root=args.output.expanduser().resolve()
    marker=root/'.igobygod-tools'
    if root.exists() and any(root.iterdir()) and not marker.is_file():raise ValueError('Choose a new empty tools directory.')
    root.mkdir(parents=True,exist_ok=True);marker.write_text('igobygod local build dependencies\n')
    lock=json.loads((HERE/'dependencies.lock.json').read_text())
    for item in lock['sources']:
        print('Preparing '+item['directory'],flush=True);checkout(root,item)
    for item in lock['archives']:
        print('Downloading verified '+item['directory'],flush=True);fetch_archive(root,item)
    env_root=root/'python'
    if not (env_root/'pyvenv.cfg').is_file():run([sys.executable,'-m','venv',env_root])
    python=env_root/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
    run([python,'-m','pip','install','--disable-pip-version-check','-r',HERE/'requirements-build.txt'])
    # Some upstream modules import dependencies absent from their package metadata.
    # Exercise the actual readers before asking the viewer to rebuild game assets.
    run([python,'-c','from soulstruct.havok.fromsoft.eldenring import SkeletonHKX, AnimationHKX; from soulstruct.flver import FLVER; from soulstruct.containers import Binder; from soulstruct.containers.tpf import TPF; import numpy, scipy, PIL, zstandard'])
    sf=root/'SoulsFormats/SoulsFormats/SoulsFormats.csproj'
    run(['dotnet','build',sf,'-c','Release','-f','netstandard2.1','--nologo','-v:q'])
    sf_dll=sf.parent/'bin/Release/netstandard2.1/SoulsFormats.dll'
    binaries=root/'bin';binaries.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.igobygod-helpers-',dir=root) as temporary:
        local_source=Path(temporary)/'source'
        shutil.copytree(HERE/'pipeline/er',local_source)
        for project in ['ERAssets/ERAssets.csproj','ERParams/ERParams.csproj']:
            run(['dotnet','publish',local_source/project,'-c','Release','-o',binaries,'--nologo','-v:q',
                 '-p:SoulsFormatsDll='+str(sf_dll),'-p:ArchiveKeysSource='+str(root/'UXM/UXM/ArchiveKeys.cs')])
        cargo=['cargo']+(['xwin'] if os.name!='nt' else [])
        helper=local_source/'oodle-helper'
        env=os.environ.copy();env['CARGO_TARGET_DIR']=str(root/'helper-target')
        run([*cargo,'build','--release','--locked','--target','x86_64-pc-windows-msvc'],cwd=helper,env=env)
        shutil.copy2(root/'helper-target/x86_64-pc-windows-msvc/release/igobygod-oodle-helper.exe',binaries/'igobygod-oodle-helper.exe')
    report={'author':'igobygod','dependencies':lock,'python':str(python),'system':platform.system()}
    (root/'tools.local.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Local dependencies prepared. See README for the game-folder setup command.')
    return 0

if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        print('Dependency setup stopped: '+str(error),file=sys.stderr);sys.exit(1)
