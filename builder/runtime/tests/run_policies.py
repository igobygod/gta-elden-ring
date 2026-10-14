#!/usr/bin/env python3
"""Run platform-independent policy checks without game assets or a running game."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
compiler=os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
if not compiler: raise SystemExit('Install clang++ or g++, or set CXX to its executable.')
tests=json.loads((root/'policy-tests.json').read_text())
with tempfile.TemporaryDirectory(prefix='igobygod-policy-') as temp:
    for i,relative in enumerate(tests):
        output=Path(temp)/('check_'+str(i)+('.exe' if os.name=='nt' else ''))
        subprocess.run([compiler,'-std=c++17','-O2',str(root/'PlayerRenderer'/relative),'-o',str(output)],check=True)
        subprocess.run([str(output)],check=True)
print(f'PASS: {len(tests)} policy test executables. Gameplay testing is separate.')
