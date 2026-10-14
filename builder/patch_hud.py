"""Locally hide six original HUD placements; preserve every other game tag."""
import argparse
import hashlib
import struct
from pathlib import Path

TARGETS={452:{11:b'SP',13:b'MP',15:b'HP',17:b'HPRegain'},470:{1:b'ItemPanel',13:b'ItemPanel2'}}

def tags(data, offset):
    while offset+2<=len(data):
        start=offset; header=struct.unpack_from('<H',data,offset)[0]; offset+=2
        code=header>>6; length=header&63
        if length==63:
            if offset+4>len(data): raise ValueError('Truncated tag header')
            length=struct.unpack_from('<I',data,offset)[0]; offset+=4
        end=offset+length
        if end>len(data): raise ValueError('Truncated tag payload')
        yield code,data[offset:end],data[start:end],end,(header&63)==63
        offset=end
        if code==0:return

def tag(code, payload, long_header=False):
    if len(payload)>=63 or long_header:
        return struct.pack('<HI',(code<<6)|63,len(payload))+payload
    return struct.pack('<H',(code<<6)|len(payload))+payload

def zero_scale(data):
    bits=''.join(f'{byte:08b}' for byte in data); offset=0
    def read(count):
        nonlocal offset
        result=bits[offset:offset+count]
        if len(result)!=count:raise ValueError('Truncated matrix')
        offset+=count
        return int(result,2) if count else 0
    if read(1):
        n=read(5);read(n);read(n)
    rest=offset
    if read(1):
        n=read(5);read(n);read(n)
    n=read(5);read(n);read(n)
    # Zero scale needs no payload bits. Keep rotation and translation verbatim.
    result='100000'+bits[rest:offset]
    result+='0'*((-len(result))%8)
    return bytes(int(result[i:i+8],2) for i in range(0,len(result),8)),(offset+7)//8

def patch(source):
    data=bytes(source)
    if data[:3]!=b'GFX' or len(data)<16:raise ValueError('Expected uncompressed GFX file')
    offset=8+(5+4*(data[8]>>3)+7)//8+4
    chunks=[data[:offset]];changed=[];end=offset
    for code,payload,raw,end,long_header in tags(data,offset):
        if code==39 and len(payload)>=4:
            sid=struct.unpack_from('<H',payload)[0]
            if sid in TARGETS:
                inner=[payload[:4]];inner_end=4
                for c,p,r,inner_end,wide in tags(payload,4):
                    if c==26 and len(p)>=5:
                        flags=p[0];depth=struct.unpack_from('<H',p,1)[0]
                        name=TARGETS[sid].get(depth)
                        if name and flags&0x26==0x26 and p.endswith(name+b'\0'):
                            matrix,used=zero_scale(p[5:])
                            r=tag(c,p[:5]+matrix+p[5+used:],wide)
                            changed.append((sid,depth))
                    inner.append(r)
                inner.append(payload[inner_end:]);raw=tag(code,b''.join(inner),long_header)
        chunks.append(raw)
    chunks.append(data[end:]);output=bytearray(b''.join(chunks))
    struct.pack_into('<I',output,4,len(output))
    if sorted(changed)!=sorted((s,d) for s,depths in TARGETS.items() for d in depths):
        raise ValueError('Game HUD structure differs; no output was written')
    return bytes(output)

def main():
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if a.input.resolve()==a.output.resolve():raise SystemExit('Output must differ from original input')
    output=patch(a.input.read_bytes());a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(output)
    print('HUD patch written; six placements updated. SHA256 '+hashlib.sha256(output).hexdigest())
if __name__=='__main__':main()
