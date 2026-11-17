#!/usr/bin/env python3
"""Dependency-free analysis of actual float WAV renders; no listening claims."""
import array,cmath,csv,hashlib,json,math,struct,sys
from pathlib import Path
root=Path(sys.argv[1])
def read(name):
 d=(root/name).read_bytes();offset=12;data=None;rate=None
 while offset+8<=len(d):
  tag=d[offset:offset+4];length=struct.unpack_from('<I',d,offset+4)[0];body=d[offset+8:offset+8+length]
  if tag==b'fmt ':fmt,channels,rate,_,_,bits=struct.unpack('<HHIIHH',body[:16]);assert (fmt,channels,bits)==(3,2,32)
  if tag==b'data':data=array.array('f');data.frombytes(body)
  offset+=8+length+(length%2)
 assert data is not None and sys.byteorder=='little'
 return rate,data

def fft(a):
 n=len(a);j=0
 for i in range(1,n):
  bit=n>>1
  while j&bit:j^=bit;bit>>=1
  j^=bit
  if i<j:a[i],a[j]=a[j],a[i]
 size=2
 while size<=n:
  w=cmath.exp(-2j*math.pi/size)
  for start in range(0,n,size):
   tw=1
   for k in range(size//2):
    u=a[start+k];v=a[start+k+size//2]*tw;a[start+k]=u+v;a[start+k+size//2]=u-v;tw*=w
  size*=2
 return a

def spectrum(name):
 rate,d=read(name);n=16384;start=int(rate*1.15);x=[complex(d[(start+i)*2]* (.5-.5*math.cos(2*math.pi*i/(n-1)))) for i in range(n)];s=fft(x);return rate,n,[abs(v) for v in s[:n//2]]
result={}
for variant in ['independent','varispeed']:
 fs,n,s=spectrum('G5-'+variant+'.wav');index=max(range(int(50*n/fs),int(1000*n/fs)),key=s.__getitem__);result['G5_'+variant+'_peak_hz']=index*fs/n
for variant in ['linear','sinc16']:
 fs,n,s=spectrum('G1-alias-'+variant+'.wav');center=round(14000*n/fs);energy=sum(v*v for v in s[center-3:center+4]);result['G1_'+variant+'_alias_band_power']=energy
result['G1_alias_sinc_vs_linear_db']=10*math.log10(result['G1_sinc16_alias_band_power']/result['G1_linear_alias_band_power'])
for variant in ['centred','scattered']:
 fs,d=read('G6-'+variant+'.wav');l=d[::2];r=d[1::2];m=sum(((x+y)*.5)**2 for x,y in zip(l,r));side=sum(((x-y)*.5)**2 for x,y in zip(l,r));result['G6_'+variant+'_side_to_mid_db']=10*math.log10(max(side,1e-30)/max(m,1e-30));result['G6_'+variant+'_mono_rms']=math.sqrt(m/len(l))
fs,d=read('G8-three-minutes-two-second-source.wav')
for second in [30,90,179]:
 x=d[second*fs*2:(second+1)*fs*2];result['G8_rms_'+str(second)]=math.sqrt(sum(v*v for v in x)/len(x))
result['wav_count']=len(list(root.glob('*.wav')))
(root/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
(root/'SHA256.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(root.glob('*.wav'))},indent=2)+'\n')
print(json.dumps(result,indent=2))
import html
items=[]
for p in sorted(root.glob('*.wav')):
 name=html.escape(p.name)
 items.append(f'<section><h2>{name}</h2><audio controls preload="none" src="{name}"></audio> <a href="{name}">WAV</a></section>')
(root/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>G-01 listening cycle</title><style>body{background:#101617;color:#e5e9e5;font:16px system-ui;max-width:1000px;margin:40px auto;padding:20px}section{padding:16px;border-top:1px solid #52615b}h2{font-size:15px}a{color:#c3ca8f}</style><h1>G-01 / rendered evidence</h1><p>34 WAVs. Start with G2 direct/cascade, G5 independent/varispeed, G7 inherited/eroded, then the three-minute G8 performance. G2/G5/G7 and Grain-versus-Loop/Field pairs are loudness matched. Other files retain raw levels. Source 5 is synthetic field-like audio, not a recording. No subjective success or Cubase validation is claimed.</p>'+''.join(items)+'</html>')
