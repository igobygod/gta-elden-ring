"""Decode the GTA YCD XML export into retargeting-ready local tracks.

This deliberately emits source data only. It does not pretend GTA's RAGE
clips can be dropped into Elden Ring's Havok graph without skeleton and
compression conversion.
"""
from __future__ import annotations
import os
import argparse, json, math
from pathlib import Path
import xml.etree.ElementTree as ET

def attr(node, name, default=None):
    a=node.find(name)
    return default if a is None else a.attrib.get("value", a.text)

def value(ch, frame):
    typ=attr(ch, "Type", "")
    v=ch.find("Value")
    if typ=="StaticFloat": return [float(v.attrib.get("value", v.text))]
    if typ=="StaticVector3": return [float(v.attrib[k]) for k in "xyz"]
    if typ=="StaticQuaternion": return [float(v.attrib[k]) for k in "xyzw"]
    vals=ch.findtext("Values", "").split()
    if typ in ("QuantizeFloat", "Float"):
        return [float(vals[frame % len(vals)])]
    if typ=="IndirectQuantizeFloat":
        frames=[int(x) for x in ch.findtext("Frames", "").split()]
        return [float(vals[frames[frame % len(frames)]])]
    if typ in ("QuantizeVector3", "Vector3"):
        n=len(vals)//3; i=(frame%n)*3; return [float(x) for x in vals[i:i+3]]
    raise ValueError(f"unsupported YCD channel {typ}")

def animation_tracks(anim):
    frames=int(attr(anim,"FrameCount",0)); limit=int(attr(anim,"SequenceFrameLimit",frames))
    bones=[]
    bone_ids = anim.find("BoneIds")
    for b in ([] if bone_ids is None else bone_ids):
        bones.append((int(attr(b,"BoneId")),int(attr(b,"Track"))))
    sequences = anim.find("Sequences")
    seqs=[] if sequences is None else list(sequences)
    first_data = None if not seqs else seqs[0].find("SequenceData")
    if not seqs or len(bones)!=len([] if first_data is None else list(first_data)):
        raise ValueError("YCD bone/sequence mismatch")
    tracks=[]
    for bi,(bone,track) in enumerate(bones):
        samples=[]
        for frame in range(frames):
            seq=seqs[min(frame//limit,len(seqs)-1)]
            sequence_data = seq.find("SequenceData")
            item=list([] if sequence_data is None else sequence_data)[bi]
            channel_nodes = item.find("Channels")
            channels=list([] if channel_nodes is None else channel_nodes)
            vals=[]
            local_frame=frame % limit
            for ch in channels:
                typ=attr(ch,"Type","")
                if typ.startswith("CachedQuaternion"):
                    # RAGE stores x/y/z followed by a cached missing quaternion
                    # component. The component index is explicit in QuatIndex.
                    idx=int(attr(ch,"QuatIndex",3)); xyz=[]
                    for prior in channels[:3]: xyz.append(value(prior,local_frame)[0])
                    missing=math.sqrt(max(0.0,1.0-sum(x*x for x in xyz)))
                    q=xyz[:]; q.insert(idx,missing); vals=q; break
                vals.extend(value(ch,local_frame))
            if not vals: raise ValueError("empty YCD channel")
            vals=[float(x) for x in vals[:4]]
            if any(not math.isfinite(x) for x in vals): raise ValueError("nonfinite YCD sample")
            if track==1:
                n=math.sqrt(sum(x*x for x in vals))
                if not .85<n<1.15: raise ValueError(f"bad quaternion norm {n}")
            samples.append(vals)
        tracks.append({"bone":bone,"track":track,"samples":samples})
    return frames, tracks

def main():
    ap=argparse.ArgumentParser();ap.add_argument("ycd_xml",type=Path);ap.add_argument("output",type=Path);a=ap.parse_args()
    root=ET.parse(a.ycd_xml).getroot(); by_hash={x.findtext("Hash"):x for x in root.findall("./Clips/Item")}
    animations=[]; wanted=[]
    for anim in root.findall("./Animations/Item"):
        frames,tracks=animation_tracks(anim)
        animations.append({"hash":anim.findtext("Hash"),"frames":frames,"duration":float(attr(anim,"Duration",0)),"tracks":tracks})
    amap={x["hash"]:x for x in animations}
    for clip in root.findall("./Clips/Item"):
        name=clip.findtext("Name","")
        layer_nodes=clip.findall("./Animations/Item")
        if not layer_nodes and clip.find("AnimationHash") is not None:
            layer_nodes=[clip]
        refs=[x.findtext("AnimationHash") for x in layer_nodes]
        if True:  # Retain locomotion, throw, idle and weapon clips for the native port.
            tags=[]
            tag_nodes=clip.find("Tags")
            for tag in ([] if tag_nodes is None else tag_nodes):
                if tag.findtext("NameHash") != "audio":
                    continue
                attrs=tag.find("Attributes")
                ident=None if attrs is None else attrs.find("Item/Value")
                tags.append({"start_phase":float(attr(tag,"StartPhase",0)),
                             "end_phase":float(attr(tag,"EndPhase",0)),
                             "event":None if ident is None else (ident.attrib.get("value") or ident.text)})
            wanted.append({"name":name,"type":attr(clip,"Type",""),
                           "start_time":float(attr(clip,"StartTime",0)),
                           "end_time":float(attr(clip,"EndTime",0)),
                           "rate":float(attr(clip,"Rate",1)),
                           "duration":float(attr(clip,"Duration",0)),
                           "layers":[dict(hash=x.findtext("AnimationHash"),start=float(attr(x,"StartTime",0)),end=float(attr(x,"EndTime",0)),rate=float(attr(x,"Rate",1))) for x in layer_nodes],
                           "audio_tags":tags,
                           "animations":[amap[h] for h in refs if h in amap]})
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps({"source":str(a.ycd_xml),"animations":animations,"weapon_clips":wanted},separators=(",",":")))
    print(f"decoded {len(animations)} animations, selected {len(wanted)} weapon clips -> {a.output}")

if __name__=="__main__": main()
