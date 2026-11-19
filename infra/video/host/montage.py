# montage.py out.png in1.png in2.png ...: stack PNGs vertically (pure python, 8-bit RGB/RGBA, non-interlaced)
import sys, zlib, struct
def rd(p):
    d=open(p,'rb').read(); i=8; w=h=0; idat=b''; ct=2
    while i<len(d):
        n=struct.unpack('>I',d[i:i+4])[0]; t=d[i+4:i+8]; c=d[i+8:i+8+n]; i+=12+n
        if t==b'IHDR': w,h,bd,ct=struct.unpack('>IIBB',c[:10])
        elif t==b'IDAT': idat+=c
    bpp=4 if ct==6 else 3; raw=zlib.decompress(idat); st=w*bpp; out=bytearray(); prev=bytearray(st); p=0
    for y in range(h):
        f=raw[p]; line=bytearray(raw[p+1:p+1+st]); p+=1+st
        for x in range(st):
            a=line[x-bpp] if x>=bpp else 0; b=prev[x]; c=prev[x-bpp] if x>=bpp else 0
            if f==1: line[x]=(line[x]+a)&255
            elif f==2: line[x]=(line[x]+b)&255
            elif f==3: line[x]=(line[x]+(a+b)//2)&255
            elif f==4:
                pa=abs(b-c); pb=abs(a-c); pc=abs(a+b-2*c); pr=a if pa<=pb and pa<=pc else (b if pb<=pc else c); line[x]=(line[x]+pr)&255
        prev=line
        out+= line if bpp==3 else bytes(v for k,v in enumerate(line) if k%4!=3)
    return w,h,bytes(out)
imgs=[rd(p) for p in sys.argv[2:]]; w=max(i[0] for i in imgs); rows=[]
for iw,ih,px in imgs:
    for y in range(ih): rows.append(px[y*iw*3:(y+1)*iw*3]+b'\x00'*3*(w-iw))
    rows += [b'\xff\x00\xff'*w]*4
raw=b''.join(b'\x00'+r for r in rows)
ch=lambda t,b: struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,len(rows),8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw,6))+ch(b'IEND',b''))
