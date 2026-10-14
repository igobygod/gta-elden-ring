"""Run the existing builder without opening child console windows on Windows."""
import os
import runpy
import subprocess
import sys
from pathlib import Path

if os.name == 'nt':
    original_popen = subprocess.Popen
    class HiddenProcess(original_popen):
        def __init__(self, *args, **kwargs):
            kwargs['creationflags'] = kwargs.get('creationflags', 0) | subprocess.CREATE_NO_WINDOW
            super().__init__(*args, **kwargs)
    subprocess.Popen = HiddenProcess

if __name__ == '__main__':
    target = Path(sys.argv[1]).resolve()
    sys.argv = sys.argv[1:]
    sys.path.insert(0, str(target.parent))
    runpy.run_path(str(target), run_name='__main__')
