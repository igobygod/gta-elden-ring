"""Rebuild the one English death-message override from the selected local game."""
from pathlib import Path
import argparse, hashlib, json
from terrain_assets import decode_batch


def build(args):
    from soulstruct.containers import Binder
    from soulstruct.base.text.fmg import FMG
    from soulstruct.dcx import DCXType
    root=args.workspace.resolve();work=root/'Build/MessagePatch';work.mkdir(parents=True,exist_ok=True)
    source=root/'Assets/ER/msg/engus/menu.msgbnd.dcx';raw=work/'menu.decompressed'
    decode_batch(args,[(source,raw)],work)
    binder=Binder.from_path(raw);changes=[]
    for entry in binder.entries:
        if entry.entry_id!=200:continue
        before=entry.get_uncompressed_data();fmg=FMG.from_bytes(before)
        if fmg.entries.get(331305)!='YOU DIED':raise ValueError('Unsupported original English death message.')
        needle='YOU DIED'.encode('utf-16le')+b'\0\0'
        if before.count(needle)!=1:raise ValueError('Original death message is not unique.')
        offset=before.index(needle);after=before[:offset]+b'\0\0'+before[offset+2:]
        entry.set_uncompressed_data(after);changes.append(entry.entry_id)
    if changes!=[200]:raise ValueError('Original death-message binder layout differs.')
    binder.dcx_type=DCXType.DCX_DFLT_11000_44_9_15
    dest=root/'Package/assets/msg/engus/menu.msgbnd.dcx';dest.parent.mkdir(parents=True,exist_ok=True);binder.write(dest)
    check=Binder.from_path(dest);original=Binder.from_path(raw)
    if len(check.entries)!=len(original.entries):raise ValueError('Message binder count changed.')
    for before,after in zip(original.entries,check.entries):
        if before.entry_id!=after.entry_id or before.name!=after.name:raise ValueError('Message binder identity changed.')
        a,z=before.get_uncompressed_data(),after.get_uncompressed_data()
        if before.entry_id!=200:
            if a!=z:raise ValueError('Unrelated message binder entry changed.')
        else:
            af,zf=FMG.from_bytes(a),FMG.from_bytes(z)
            if set(af.entries)!=set(zf.entries) or [k for k in af.entries if af.entries[k]!=zf.entries[k]]!=[331305]:
                raise ValueError('Unexpected message text change.')
    report={'changed_entry':331305,'binder_entry':200,'unrelated_entries_preserved':True,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'bytes':dest.stat().st_size}
    (work/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',type=Path,required=True);p.add_argument('--game',type=Path,required=True)
    p.add_argument('--oodle-helper',type=Path,required=True);p.add_argument('--wine');p.add_argument('--bottle')
    build(p.parse_args())

if __name__=='__main__':main()
