#!/usr/bin/env python3
"""Reversibly quarantine the C-01 bundle after a reported Cubase hang."""
from pathlib import Path
import datetime,json,os,pwd,shutil
source=Path('/Library/Audio/Plug-Ins/VST3/Contamination C-01.vst3')
owner=pwd.getpwuid(Path(__file__).resolve().parents[1].stat().st_uid)
folder=Path(owner.pw_dir)/'Library/Application Support/Aste/Plugin Quarantine'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S-c01')
if source.is_symlink():raise SystemExit('Refusing symlinked bundle')
if not source.exists():print('C-01 is already absent from the scan folder');raise SystemExit(0)
folder.mkdir(parents=True,exist_ok=False)
shutil.move(str(source),str(folder/source.name))
receipt=folder/'QUARANTINE.json';receipt.write_text(json.dumps(dict(source=str(source),quarantined=str(folder/source.name),reason='Reported Cubase hang on load; do not reinstall before host/editor investigation'),indent=2)+'\n')
for path in [folder,receipt]:os.chown(path,owner.pw_uid,owner.pw_gid)
print('Quarantined C-01:',folder/source.name)
