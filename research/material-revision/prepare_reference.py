#!/usr/bin/env python3
"""Validate and namespace the frozen pre-edit cores; never substitute current sources."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('snapshot',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
manifest=json.loads((a.snapshot/'manifest.json').read_text())
for original,expected in manifest.items():
 product=Path(original).parts[1];name=Path(original).name;source=a.snapshot/product/name
 if hashlib.sha256(source.read_bytes()).hexdigest()!=expected:raise SystemExit('Reference hash mismatch: '+str(source))
 namespace=product.replace('-','')
 text=source.read_text().replace('aste::'+namespace,'aste::before::'+namespace)
 # Non-Machine's namespace has no dash; the filename has an underscore.
 if product=='non-machine':text=text.replace('aste::nonmachine','aste::before::nonmachine')
 dest=a.output/'reference'/product/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(text)
print('Validated frozen pre-edit source files:',len(manifest))
