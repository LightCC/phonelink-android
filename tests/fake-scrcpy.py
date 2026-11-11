#!/usr/bin/python3
import os,sys,time
from pathlib import Path
r=Path(os.environ['PHONELINK_FAKE_ROOT'])
p=r/'launches'
try:n=int(p.read_text())+1
except FileNotFoundError:n=1
p.write_text(str(n))
(r/'arguments').write_text(' '.join(sys.argv[1:]))
mode=(r/'mode').read_text().strip()
if mode=='close':time.sleep(.1);sys.exit(0)
if mode=='crash' or (mode=='once' and n==1):time.sleep(.1);sys.exit(1)
time.sleep(60)
