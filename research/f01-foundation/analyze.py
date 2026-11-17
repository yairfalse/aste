#!/usr/bin/python3
"""Offline numpy analysis: causal octave-like filters, Schroeder T20, NED proxies."""
import csv, hashlib, html, json, math, struct, sys
from pathlib import Path
import numpy as np
root=Path(sys.argv[1])
def read(p):
 b=p.read_bytes();at=12;data=None;rate=0
 while at+8<=len(b):
  tag=b[at:at+4];n=struct.unpack_from('<I',b,at+4)[0]
  if tag==b'fmt ':kind,ch,rate,_,_,bits=struct.unpack_from('<HHIIHH',b,at+8);assert(kind,ch,bits)==(3,2,32)
  if tag==b'data':data=np.frombuffer(b,dtype='<f4',count=n//4,offset=at+8).reshape(-1,2).astype(float)
  at+=8+n+(n%2)
 assert data is not None
 return rate,data

def fit_rt(energy,fs):
 edc=np.cumsum(energy[::-1])[::-1]
 if edc[0]<1e-30:return dict(rt60=None,r2=None,reason='silent')
 db=10*np.log10(np.maximum(edc/edc[0],1e-300));ix=np.flatnonzero((db<=-5)&(db>=-25))
 if len(ix)<max(20,fs*.02):return dict(rt60=None,r2=None,reason='insufficient range')
 # No measurement if finite recording retains enough late energy to bias T20.
 if ix[-1]>len(db)*.85:return dict(rt60=None,r2=None,reason='truncated tail')
 x=ix/fs;y=db[ix];slope,intercept=np.polyfit(x,y,1);err=np.sum((y-(slope*x+intercept))**2);r2=1-err/max(1e-30,np.sum((y-y.mean())**2))
 return dict(rt60=-60/slope if slope<0 else None,r2=float(r2),reason='ok' if r2>.95 else 'nonlinear decay; estimate weak')

def transfer(fs,n,fc,highpass):
 # RBJ Butterworth biquad; exact causal transfer sampled on a zero-padded FFT grid.
 w=2*np.pi*fc/fs;c=math.cos(w);s=math.sin(w);alpha=s/math.sqrt(2);a=np.array([1+alpha,-2*c,1-alpha])
 b=np.array([(1+c)/2,-(1+c),(1+c)/2]) if highpass else np.array([(1-c)/2,1-c,(1-c)/2])
 z=np.exp(-2j*np.pi*np.arange(n//2+1)/n)
 return (b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z)
rows=[];density=[];curves={}
for p in sorted(root.glob('ir-*.wav')):
 fs,d=read(p);energy=np.mean(d*d,axis=1);n=1<<(2*len(d)-1).bit_length();spectra=np.fft.rfft(d,n=n,axis=0)
 base=fit_rt(energy,fs);active=np.flatnonzero(np.max(np.abs(d),axis=1)>max(1e-8,float(np.max(np.abs(d)))*1e-4))
 def corr(a,b):
  den=np.linalg.norm(a)*np.linalg.norm(b)
  return float(np.dot(a,b)/den) if den>1e-25 else None
 lo,hi=int(.1*fs),int(min(2,len(d)/fs)*fs)
 row=dict(file=p.name,broad_rt60=base['rt60'],broad_r2=base['r2'],first_arrival_ms=float(active[0]/fs*1000) if len(active) else None,lr_correlation=corr(d[lo:hi,0],d[lo:hi,1]),mono_energy_fraction=float(np.sum(np.mean(d,axis=1)**2)/max(1e-30,np.sum(energy))),early_0_80ms_fraction=float(np.sum(energy[:int(.08*fs)])/max(1e-30,np.sum(energy))))
 for center in [125,250,500,1000,2000,4000,8000]:
  h=transfer(fs,n,center/math.sqrt(2),True)*transfer(fs,n,center*math.sqrt(2),False)
  filtered=np.fft.irfft(spectra*h[:,None],n=n,axis=0)[:len(d)]
  fit=fit_rt(np.mean(filtered*filtered,axis=1),fs)
  row[f'rt60_{center}']=fit['rt60'];row[f'r2_{center}']=fit['r2']
 rows.append(row)
 for ms in [20,50,100,200,500,1000]:
  a=d[int(fs*ms/1000):int(fs*(ms/1000+.02)),0];a=a-a.mean();sigma=np.sqrt(np.mean(a*a));ned=float(np.mean(np.abs(a)>sigma)/.3173105) if sigma>1e-12 else None
  density.append(dict(file=p.name,time_ms=ms,ned=ned))
 edc=np.cumsum(energy[::-1])[::-1];db=10*np.log10(np.maximum(edc/max(edc[0],1e-30),1e-12));curves[p.name]=[[round(i/fs,4),round(float(db[i]),3)] for i in range(0,len(d),int(fs*.01))]
 print('analyzed',p.name,flush=True)
for name,data in [('ir-metrics.csv',rows),('echo-density.csv',density)]:
 with(root/name).open('w') as f:w=csv.DictWriter(f,data[0].keys());w.writeheader();w.writerows(data)
(root/'decay-curves.json').write_text(json.dumps(curves))
(root/'SHA256.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(root.glob('*.wav'))},indent=2)+'\n')
# Standalone SVG, no graph-rendering dependency.
colors=['#777','#ad6','#d85','#6bb','#8af','#c8c','#fbc']
chosen=[(k,v) for k,v in curves.items() if k.startswith('ir-2.400000-')]
svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 550"><rect width="1000" height="550" fill="#14191c"/><g fill="#eee" font-family="sans-serif" font-size="13"><text x="55" y="25">F-01: broadband energy decay (2.4 s nominal; current default is not calibrated)</text>']
for db in [0,-20,-40,-60,-80]:
 y=50-db*4;svg.append(f'<path d="M55 {y}H750" stroke="#39434a"/><text x="15" y="{y+5}">{db}</text>')
for idx,(k,v) in enumerate(chosen):
 pts=' '.join(f'{55+t*85:.2f},{50-min(0,max(-90,db))*4:.2f}' for t,db in v if t<=8)
 svg.append(f'<polyline points="{pts}" fill="none" stroke="{colors[idx]}"/><text x="765" y="{65+idx*24}" fill="{colors[idx]}">{html.escape(k[12:-4])}</text>')
svg.append('<text x="55" y="460">0 → 8 seconds; curves show energy, not perceived quality</text></g></svg>');(root/'decay.svg').write_text(''.join(svg))
body=['<!doctype html><meta charset="utf-8"><title>Field foundation comparisons</title><style>body{font:16px system-ui;background:#14191c;color:#eee;max-width:1000px;margin:32px auto}audio{width:100%}a{color:#8af}</style><h1>Field foundation</h1><p>Seven paths, identical dry sources. Wet examples loudness-matched within each material; raw IRs preserve gain. No subjective acceptance claimed. Speech is generated using macOS Samantha, not a recorded singer.</p><img src="decay.svg" width="100%">']
for source in ['impulse','percussion','chords','tone','speech']:
 body.append(f'<h2>{source}</h2>')
 for p in sorted(root.glob(source+'-*.wav')):body.append(f'<p>{html.escape(p.name)}</p><audio controls preload="none" src="{html.escape(p.name)}"></audio>')
body.append('<p>Measurements: <a href="ir-metrics.csv">RT60 / spatial</a>, <a href="echo-density.csv">density proxy</a>, <a href="levels.csv">levels</a>. External licensed reference renders, when available, are in sibling folders.</p>');(root/'index.html').write_text(''.join(body))
