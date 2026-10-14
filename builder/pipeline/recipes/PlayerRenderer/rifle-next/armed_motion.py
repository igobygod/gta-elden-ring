import os
"""Isolated, full-body GTA rifle locomotion export. Never installs or edits game files.

AMOV v1: <6I magic=0x414d4f56,version=1,body_nv,gun_nv,nf,nc=4>;
for idle/walk/run/sprint: <f duration>, float32[nf,body_nv+gun_nv,3],
float32[nf,3] muzzle. Ordering and attachment match rifle-stage/rifle.bin.
"""
from pathlib import Path
import argparse
import contextlib
import importlib.util
import json
import struct
import xml.etree.ElementTree as ET
import numpy as np

ROOT = Path(os.environ['IGOBYGOD_BUILD_ROOT'])
OUT = Path(__file__).parent
NAMES = ('idle', 'walk', 'run', 'sprint')


def normalize(q):
    return q / np.maximum(np.linalg.norm(q, axis=-1, keepdims=True), 1e-20)


def qmix(a, b, weight):
    b = np.where(np.sum(a * b, axis=-1, keepdims=True) < 0, -b, b)
    return normalize(a + (b - a) * weight)


def qmul(a, b):
    return np.r_[a[3] * b[:3] + b[3] * a[:3] + np.cross(a[:3], b[:3]), a[3] * b[3] - np.dot(a[:3], b[:3])]


class Clip:
    def __init__(self, data, name, tagindex):
        self.clip = next(c for c in data['weapon_clips'] if c['name'] == f'pack:/{name}.clip')
        self.duration = self.clip['duration']
        amap = {a['hash']: a for a in self.clip['animations']}
        self.layers = []
        self.coverage = set()
        self.ignored = set()
        for layer in self.clip['layers']:
            anim = amap[layer['hash']]
            tracks = []
            for tr in anim['tracks']:
                if tr['track'] not in (0, 1):
                    self.ignored.add(tr['track'])
                    continue
                if tr['bone'] not in tagindex:
                    continue
                idx = tagindex[tr['bone']]
                self.coverage.add((idx, tr['track']))
                tracks.append((idx, tr['track'], np.asarray(tr['samples'], dtype=np.float64)))
            self.layers.append((layer, (anim['frames'] - 1) / anim['duration'], tracks))

    def apply(self, t, translations, rotations, allowed=None):
        for layer, rate, tracks in self.layers:
            sample = min(layer['end'], layer['start'] + t * layer['rate']) * rate
            for idx, kind, values in tracks:
                if allowed is not None and idx not in allowed:
                    continue
                phase = np.clip(sample, 0, len(values) - 1)
                a = int(phase)
                b = min(a + 1, len(values) - 1)
                if kind == 1:
                    rotations[idx] = qmix(values[a], values[b], phase - a)
                else:
                    translations[idx] = values[a, :3] + (values[b, :3] - values[a, :3]) * (phase - a)


class Exporter:
    def __init__(self, output):
        self.output = output
        output.mkdir(parents=True, exist_ok=True)
        spec = importlib.util.spec_from_file_location('isolated_donor_probe', ROOT / 'Build/original-rig-proof/probe.py')
        self.p = p = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(p)
        # probe.build's diagnostics and texture writes are redirected into this new folder.
        p.O = output / 'donor-cache'
        p.O.mkdir(exist_ok=True)
        with (output / 'asset.log').open('w') as log, contextlib.redirect_stdout(log):
            p.build()
        nodes = ET.parse(next(p.D.rglob('player_one.yft.xml'))).getroot().findall('.//Skeleton/Bones/Item')
        self.rest_q = np.array([[float(b.find('Rotation').get(k)) for k in 'xyzw'] for b in nodes])
        self.rest_t = p.rest[:, :3, 3].copy()
        self.scales = np.linalg.norm(p.rest[:, :3, :3], axis=1)
        self.inv_rest = np.linalg.inv(p.restworld)
        self.body = [m for m in p.meshes if not m['gun']]
        self.guns = [m for m in p.meshes if m['gun']]
        self.nv = sum(len(m['positions']) for m in self.body)
        self.ng = sum(len(m['positions']) for m in self.guns)
        self.prepared = []
        self.mesh_meta = []
        offset = 0
        self.foot_vertices = []
        for m in self.body:
            lookup = np.array([p.names.index(n) for n in m['bones']])
            self.prepared.append((np.column_stack((m['positions'], np.ones(len(m['positions'])))), lookup[m['joints']], m['weights']))
            self.mesh_meta.append({'name': m['name'], 'offset': offset, 'vertices': len(m['positions'])})
            if m['name'].startswith('feet_'):
                self.foot_vertices.extend(range(offset, offset + len(m['positions'])))
            offset += len(m['positions'])
        for m in self.guns:
            self.mesh_meta.append({'name': m['name'], 'offset': offset, 'vertices': len(m['positions'])})
            offset += len(m['positions'])
        stage = json.loads((ROOT / 'PlayerRenderer/rifle-stage/report.json').read_text())
        assert (self.nv, self.ng) == (stage['body_vertices'], stage['gun_vertices'])
        self.attachment = np.asarray(stage['attachment'])
        self.hand = p.names.index('PH_R_Hand')
        gm = self.guns[0]
        self.muzzle = gm['bind'][gm['bones'].index('Gun_Muzzle')][:3, 3]
        self.grip = gm['bind'][gm['bones'].index('Gun_GripR')][:3, 3]
        self.gun_coords = [np.column_stack((m['positions'], np.ones(len(m['positions'])))) for m in self.guns]
        data = json.loads((ROOT / 'Build/movement-animation-source/move_weapon@rifle@generic.json').read_text())
        self.clips = {n: Clip(data, n, p.tagindex) for n in NAMES}
        ordinary = json.loads((ROOT / 'Build/movement-animation-source/move_p_m_one.json').read_text())
        self.base_idle = Clip(ordinary, 'idle', p.tagindex)
        rifle = json.loads((ROOT / 'Build/rifle-animation-source/weapons_rifle_hi_.json').read_text())
        self.aim = Clip(rifle, 'aim_med_loop', p.tagindex)
        self.aim_t, self.aim_q = self.rest_t.copy(), self.rest_q.copy()
        self.aim.apply(0, self.aim_t, self.aim_q)
        self.spine_root = p.names.index('SKEL_Spine_Root')
        self.upper = set()
        for i in range(len(p.names)):
            j = p.parents[i]
            while j >= 0:
                if j == self.spine_root:
                    self.upper.add(i)
                    break
                j = p.parents[j]

    def pose(self, name, phase, aimed=False):
        clip = self.clips[name]
        t, q = self.rest_t.copy(), self.rest_q.copy()
        if name == 'idle':
            # Rifle idle is an arm-only layer. Retain the ordinary root/pelvis/spine/legs.
            self.base_idle.apply(phase * self.base_idle.duration, t, q)
            clip.apply(phase * clip.duration, t, q)
        else:
            clip.apply(phase * clip.duration, t, q)
        if aimed:
            # Keep gait root translation and legs. The attachment was calibrated in
            # the aim root basis, so stabilize the torso at its sibling anchor.
            self.aim.apply(0, t, q, allowed=self.upper)
            root_inverse = q[0] * np.array([-1., -1., -1., 1.])
            q[self.spine_root] = normalize(qmul(qmul(root_inverse, self.aim_q[0]), self.aim_q[self.spine_root]))
        return t, q

    def world(self, t, q):
        p = self.p
        local = p.rest.copy()
        local[:, :3, 3] = t
        for i, rotation in enumerate(q):
            local[i, :3, :3] = p.qm(rotation) @ np.diag(self.scales[i])
        return p.worlds(local, p.parents)

    def skin(self, world):
        skin = world @ self.inv_rest
        rows = []
        for coords, joints, weights in self.prepared:
            rows.append(sum(np.einsum('nij,nj->ni', skin[joints[:, k]], coords)[:, :3] * weights[:, k, None] for k in range(4)))
        gun = world[self.hand] @ self.attachment
        rows.extend((gun @ coords.T).T[:, :3] for coords in self.gun_coords)
        return np.concatenate(rows), (gun @ np.r_[self.muzzle, 1])[:3]

    def export(self, nf=120, aimed=False):
        p = self.p
        idle, _ = self.skin(self.world(*self.pose('idle', 0)))
        feet = np.asarray(self.foot_vertices)
        phase_feet = feet[idle[feet, 0] > 0]
        assert len(phase_feet)
        reports = []
        stem = 'armed-aim' if aimed else 'armed'
        alignment = None
        if aimed:
            original = json.loads((self.output / 'armed-report.json').read_text())
            alignment = {c['name']: c['aligned_start_frame'] for c in original['clips']}
            assert original['frames'] == nf
        with (self.output / (stem + '.bin')).open('wb') as stream:
            stream.write(struct.pack('<6I', 0x414D4F56, 1, self.nv, self.ng, nf, 4))
            for name in NAMES:
                clip = self.clips[name]
                duration = clip.duration
                frames, tips, worlds = [], [], []
                for phase in np.arange(nf) / nf:
                    world = self.world(*self.pose(name, float(phase), aimed=aimed))
                    vertices, tip = self.skin(world)
                    frames.append(vertices)
                    tips.append(tip)
                    worlds.append(world)
                frames = np.asarray(frames, dtype='<f4')
                tips = np.asarray(tips, dtype='<f4')
                worlds = np.asarray(worlds)
                aligned = 0 if name == 'idle' else int(np.argmax(frames[:nf//2, phase_feet, 1].mean(axis=1)))
                if alignment is not None:
                    aligned = alignment[name]
                frames, tips = np.roll(frames, -aligned, axis=0), np.roll(tips, -aligned, axis=0)
                worlds = np.roll(worlds, -aligned, axis=0)
                assert np.isfinite(frames).all() and np.isfinite(tips).all()
                world0 = self.world(*self.pose(name, 0, aimed=aimed))
                world1 = self.world(*self.pose(name, 1, aimed=aimed))
                closure, _ = self.skin(world1)
                first, _ = self.skin(world0)
                bone_error = max(abs(np.linalg.norm(worlds[:, i, :3, 3] - worlds[:, parent, :3, 3], axis=1) - np.linalg.norm(self.rest_t[i])).max() for i, parent in enumerate(p.parents) if parent >= 0 and not p.names[i].startswith(('PH_', 'IK_')))
                assert bone_error < 1e-5, bone_error
                stream.write(struct.pack('<f', duration))
                stream.write(frames.tobytes())
                stream.write(tips.tobytes())
                report = dict(name=name, source_duration=clip.duration, exported_duration=duration, source_cycles=1 if name == 'idle' else 2, aligned_start_frame=aligned, local_transform_bones=len({i for i, _ in clip.coverage}), rotation_bones=len({i for i, kind in clip.coverage if kind == 1}), translation_bones=len({i for i, kind in clip.coverage if kind == 0}), ignored_track_types=sorted(clip.ignored), root_translation_range=np.ptp(worlds[:, 0, :3, 3], axis=0).tolist(), loop_endpoint_max_vertex_m=float(np.linalg.norm(closure-first, axis=1).max()), adjacent_frame_max_vertex_m=float(np.linalg.norm(frames-np.roll(frames,1,axis=0),axis=2).max()), bone_length_max_error_m=float(bone_error), bounds=[frames.min(axis=(0,1)).tolist(),frames.max(axis=(0,1)).tolist()])
                axis = (worlds[:, self.hand] @ self.attachment)[:, :3, 0]
                report['muzzle_axis_yaw_degrees'] = [float(x) for x in np.rad2deg(np.arctan2(axis[:, 0], axis[:, 1]))[[int(np.argmin(np.arctan2(axis[:, 0], axis[:, 1]))), int(np.argmax(np.arctan2(axis[:, 0], axis[:, 1])))]]]
                elevations = np.rad2deg(np.arctan2(axis[:, 2], np.linalg.norm(axis[:, :2], axis=1)))
                report['muzzle_axis_elevation_degrees'] = [float(elevations.min()), float(elevations.max())]
                reports.append(report)
                print(json.dumps(report), flush=True)
        expected = 24 + 4 * (4 + nf * (self.nv + self.ng) * 3 * 4 + nf * 3 * 4)
        assert (self.output / (stem + '.bin')).stat().st_size == expected
        report = dict(schema='AMOV v1: <6I magic,version,bodyNv,gunNv,120,4>; each <f duration> then f32[nf,totalNv,3] and f32[nf,3] muzzle', body_vertices=self.nv, gun_vertices=self.ng, frames=nf, clips=reports, meshes=self.mesh_meta, attachment=self.attachment.tolist(), file_bytes=expected, limits=['Only transform tracks 0/1 evaluated; GTA root-motion/IK/animation graph channels not reproduced.', 'Armed idle overlays original local arm tracks on one normalized Franklin idle cycle; base body idle is time-stretched to rifle-idle duration.', 'Walk/run/sprint preserve full original two-cycle clips; only cyclic start phase changed.', 'Weapon attachment reused unchanged from rifle-stage/report.json; no additional XY facing flip.', 'Native approved ER roll/jump assets are untouched; runtime must bypass this packet for activeAction.', 'This exports rifle carry locomotion; moving aim/fire/reload need local joint overlays before skinning.'])
        report['aimed'] = aimed
        if aimed:
            report['limits'][-1] = 'Aim uses authored local upper transforms at aim_med_loop time0. Spine_Root compensates gait root rotation to preserve calibrated +Y muzzle direction; torso shares gait root translation/bob. Root and legs are unchanged.'
            report['aim_source_root_quaternion_xyzw'] = self.aim_q[0].tolist()
        (self.output / (stem + '-report.json')).write_text(json.dumps(report, indent=2))
        return report

    def export_actions(self):
        source = json.loads((ROOT / 'Build/rifle-animation-source/weapons_rifle_hi_.json').read_text())
        reload_source = json.loads((ROOT / 'Build/rifle-animation-source/weapons_rifle_hi_assault_rifle_str.json').read_text())
        actions = [('aim', self.aim), ('fire', Clip(source, 'fire_med', self.p.tagindex)), ('reload', Clip(reload_source, 'reload_aim', self.p.tagindex))]
        reference, reference_tip = self.skin(self.world(self.aim_t, self.aim_q))
        reports = []
        with (self.output / 'action-deltas.bin').open('wb') as stream:
            stream.write(struct.pack('<6I', 0x41444C54, 1, self.nv, self.ng, 3, 0))
            for name, action in actions:
                nf = max(2, round(action.duration * 30) + 1)
                deltas, tip_deltas = [], []
                for time in np.linspace(0, action.duration, nf):
                    t, q = self.aim_t.copy(), self.aim_q.copy()
                    action.apply(float(time), t, q, allowed=self.upper)
                    vertices, tip = self.skin(self.world(t, q))
                    deltas.append(vertices - reference)
                    tip_deltas.append(tip - reference_tip)
                deltas = np.asarray(deltas, dtype='<f4')
                tip_deltas = np.asarray(tip_deltas, dtype='<f4')
                if name == 'aim':
                    deltas.fill(0)
                    tip_deltas.fill(0)
                assert np.isfinite(deltas).all() and np.isfinite(tip_deltas).all()
                # Check the additive packet against full skeleton evaluation on
                # independent moving lower-body poses, including weighted waist.
                error = 0.
                for gait, phase in [('idle', .13), ('walk', .41), ('run', .62), ('sprint', .83)]:
                    base_t, base_q = self.pose(gait, phase, aimed=True)
                    base, base_tip = self.skin(self.world(base_t, base_q))
                    for frame in [0, nf // 2, nf - 1]:
                        t, q = base_t.copy(), base_q.copy()
                        if name != 'aim':
                            action.apply(frame * action.duration / (nf - 1), t, q, allowed=self.upper)
                        direct, direct_tip = self.skin(self.world(t, q))
                        error = max(error, float(np.max(np.abs(base + deltas[frame] - direct))), float(np.max(np.abs(base_tip + tip_deltas[frame] - direct_tip))))
                assert error < 1e-6, (name, error)
                stream.write(struct.pack('<If', nf, action.duration))
                stream.write(deltas.tobytes())
                stream.write(tip_deltas.tobytes())
                report = dict(name=name, frames=nf, duration=action.duration, finite=True, maximum_vertex_delta_m=float(np.linalg.norm(deltas, axis=-1).max()), maximum_muzzle_delta_m=float(np.linalg.norm(tip_deltas, axis=-1).max()), additive_vs_direct_skin_max_component_error_m=error)
                reports.append(report)
                print(json.dumps(report), flush=True)
        result = dict(schema='ADLT v1: <6I 0x41444c54,1,bodyNv,gunNv,3,0>; each aim/fire/reload <If frames,duration>, f32[frames,totalNv,3] deltas and f32[frames,3] muzzle deltas', body_vertices=self.nv, gun_vertices=self.ng, clips=reports, file_bytes=(self.output / 'action-deltas.bin').stat().st_size, limits=['Only authored transforms beneath Spine_Root are applied; reference aim root and Spine_Root held fixed.', 'Add these deltas directly to armed-aim.bin sampled at its gait phase; no additional upperMask multiplication because skin weights are already represented.', 'Aim delta is exactly zero.', 'Gun and muzzle use the unchanged calibrated attachment.', 'Native action packets unchanged; no weapon magazine prop animation, IK channels, or full GTA gameplay reproduction.'])
        (self.output / 'action-deltas-report.json').write_text(json.dumps(result, indent=2))
        return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--frames', type=int, default=120)
    parser.add_argument('--aimed', action='store_true')
    parser.add_argument('--actions', action='store_true')
    args = parser.parse_args()
    exporter = Exporter(args.output)
    if args.actions:
        exporter.export_actions()
    else:
        exporter.export(args.frames, aimed=args.aimed)
