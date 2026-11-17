#!/usr/bin/env python3
import array,cmath,csv,hashlib,html,json,math,struct,sys
from pathlib import Path
root=Path(sys.argv[1])
def read(path):
 d=path.read_bytes();at=12;rate=48000;data=None
 while at+8<=len(d):
  tag=d[at:at+4];size=struct.unpack_from('<I',d,at+4)[0];body=d[at+8:at+8+size]
  if tag==b'fmt ':kind,channels,rate,_,_,bits=struct.unpack('<HHIIHH',body[:16]);assert(kind,channels,bits)==(3,2,32)
  if tag==b'data':data=array.array('f');data.frombytes(body)
  at+=8+size+(size%2)
 assert data is not None and sys.byteorder=='little';return rate,data

def fft(a):
 n=len(a);j=0
 for i in range(1,n):
  bit=n>>1
  while j&bit:j^=bit;bit>>=1
  j^=bit
  if i<j:a[i],a[j]=a[j],a[i]
 length=2
 while length<=n:
  step=cmath.exp(-2j*math.pi/length)
  for start in range(0,n,length):
   p=1
   for i in range(length//2):u=a[start+i];v=p*a[start+i+length//2];a[start+i]=u+v;a[start+i+length//2]=u-v;p*=step
  length*=2
 return a

def spectrum(data,start,n=8192):
 x=[complex((data[(start+i)*2]+data[(start+i)*2+1])*.5*(.5-.5*math.cos(2*math.pi*i/n))) for i in range(n)];return [abs(v)**2 for v in fft(x)[:n//2]]
rows=[]
for p in sorted(root.glob('*.wav')):
 fs,d=read(p);n=8192;s=[0.]*(n//2)
 for t in [.52,.62,2.77,5.27,8.02]:
  if int(t*fs)+n<len(d)//2:
   power=spectrum(d,int(t*fs),n);s=[a+b for a,b in zip(s,power)]
 total=sum(s);low=sum(v for i,v in enumerate(s) if 100<=i*fs/n<800);high=sum(v for i,v in enumerate(s) if i*fs/n>6000)
 centroid=sum(i*fs/n*v for i,v in enumerate(s))/max(total,1e-30)
 def rms(begin,end):
  x=d[int(begin*fs)*2:int(end*fs)*2];return math.sqrt(sum(v*v for v in x)/len(x)) if x else None
 rows.append(dict(file=p.name,low_mid_fraction=low/max(total,1e-30),above_6k_fraction=high/max(total,1e-30),mid_centroid_hz=centroid,rms_1_to_2=rms(1,2),rms_10_to_12=rms(10,12)))
with(root/'spectral.csv').open('w')as f:
 w=csv.DictWriter(f,rows[0].keys());w.writeheader();w.writerows(rows)
alias=[]
for note in [93,105]:
 for factor in [1,2,4,8]:
  fs,d=read(root/f'alias-{note}-{factor}x.wav');n=32768;s=spectrum(d,fs,n);fund=440*2**((note-69)/12)/2;total=sum(s);bad=0
  for i,v in enumerate(s):
   hz=i*fs/n;k=round(hz/fund)
   if abs(hz-k*fund)>5*fs/n:bad+=v
  alias.append(dict(note=note,factor=factor,off_harmonic_proxy_db=10*math.log10(max(bad,1e-30)/max(total,1e-30))))
(root/'alias-analysis.json').write_text(json.dumps(alias,indent=2)+'\n')
(root/'SHA256.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(root.glob('*.wav'))},indent=2)+'\n')
parts=[]
for p in sorted(root.glob('*.wav')):
 name=html.escape(p.name);parts.append(f'<section><h2>{name}</h2><audio controls preload="none" src="{name}"></audio> <a href="{name}">WAV</a></section>')
(root/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>C-01 sound redesign</title><style>body{background:#101417;color:#e7ece8;font:16px system-ui;max-width:1000px;margin:40px auto;padding:20px}section{border-top:1px solid #435851;padding:12px}h2{font-size:15px}a{color:#83a99a}</style><h1>C-01 / same notes, different circuit</h1><p>Start with compare-original, then dense-source, nonlinear-filter, dirty-return and interacting. Central MIDI is identical. Main files are integrated-LUFS matched within each group; the foreground-* set separately matches the first chord window (.5–.9 seconds) so long tails cannot make the attack artificially louder. Dry is the source tap while the same architecture runs; Dense/Resonant/Original have no spatial path, so their dry/processed duplicates are intentional. Alias probes retain raw levels. Five sound targets are hypotheses, not musical approval. No protected recordings are included.</p>'+''.join(parts)+'</html>')
print('Analyzed',len(rows),'WAVs; alias probes:',alias)
