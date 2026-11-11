#!/usr/bin/python3
import os,sys,json
from pathlib import Path
r=Path(os.environ['PHONELINK_FAKE_ROOT'])
a=sys.argv[1:]
def read(p,default=''):
    try:return (r/p).read_text().strip()
    except FileNotFoundError:return default
endpoint=read('endpoint','192.168.1.50:34567')
if a[:1]==['devices']:
    print('List of devices attached')
    if read('usb')=='1':print('TESTPHONE\tdevice usb:1-1 model:Example_Phone')
    if read('connected')=='1':print(endpoint+'\tdevice model:Example_Phone')
elif a[:2]==['mdns','services']:
    print('List of discovered mdns services')
    if read('available')=='1':print('adb-TESTPHONE-xyz\t_adb-tls-connect._tcp\t'+endpoint)
elif a[:1]==['connect']:
    if read('available')=='1' and a[1]==endpoint:
        (r/'connected').write_text('1');print('connected to '+endpoint)
    else:print('failed to connect: Connection refused')
elif a[:1]==['-s'] and len(a)>3:
    command=' '.join(a[3:])
    if 'adb_wifi_enabled 0' in command:
        (r/'available').write_text('0');(r/'connected').write_text('0')
    elif 'adb_wifi_enabled 1' in command:
        (r/'available').write_text('1')
    elif 'getprop' in command:
        print(read('identity','TESTPHONE'));print('Example');print('Example Phone');print(read('android','17'))
else:sys.exit(1)
