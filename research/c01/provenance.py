#!/usr/bin/env python3
"""Record exact source/artifact hashes; optionally compare two deterministic packs."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
pack = Path(sys.argv[1]).resolve()
if not pack.is_dir():
    raise SystemExit('usage: provenance.py PACK [SECOND_PACK]')
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
paths = []
for directory in ('research/c01', 'apps/contamination', 'libs/parameters', 'libs/dsp',
                  'apps/sequence', 'apps/field', 'apps/resonator'):
    paths.extend(p for p in (root / directory).rglob('*') if p.is_file())
paths.append(root / 'tests/realtime_audit_mac.cpp')
data = {
    'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'workspace_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=root)),
    'sources': {str(p.relative_to(root)): digest(p) for p in sorted(paths)},
    'artifacts': {str(p.relative_to(pack)): digest(p) for p in sorted(pack.rglob('*'))
                  if p.is_file() and p.name != 'provenance.json'},
}
(pack / 'provenance.json').write_text(json.dumps(data, indent=2) + '\n')
if len(sys.argv) > 2:
    other = Path(sys.argv[2])
    audio = [p for p in pack.rglob('*.wav')]
    differences = [str(p.relative_to(pack)) for p in audio
                   if not (other / p.relative_to(pack)).exists()
                   or digest(p) != digest(other / p.relative_to(pack))]
    if differences:
        raise SystemExit('WAV differences: ' + ', '.join(differences))
    print(f'{len(audio)} WAV files match byte for byte')
print(pack / 'provenance.json')
