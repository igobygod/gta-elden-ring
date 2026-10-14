"""Read the skeleton from locally extracted Elden Ring archives."""
import os
from pathlib import Path
from soulstruct.containers import Binder
from soulstruct.havok.fromsoft.eldenring import SkeletonHKX, AnimationHKX

def skeleton():
 b=Binder.from_path(Path(os.environ['IGOBYGOD_BUILD_ROOT'])/'Build/c0000.anibnd.decompressed')
 e=next(e for e in b.entries if e.name.lower().endswith('skeleton.hkx'))
 return SkeletonHKX.from_bytes(e.get_uncompressed_data())
