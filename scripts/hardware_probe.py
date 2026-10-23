"""Upload a development build, launch the non-signing probe and test vendor HID.
Never enters PINs, creates wallets or approves signing. Requires SDK and pyserial/hidapi.
"""
import argparse,sys,time,struct,json,hashlib
from pathlib import Path
import hid,serial
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--sdk',required=True);p.add_argument('--fap',default='dist/flipper_evm_wallet.fap');a=p.parse_args()
sys.path.insert(0,str(Path(a.sdk)/'scripts'))
from flipper.storage import FlipperStorage
remote='/ext/apps/Tools/flipper_evm_wallet.fap'
with FlipperStorage(a.port) as s:
    s.send_file(a.fap,remote)
with serial.Serial(a.port,115200,timeout=.1) as s:
    s.write(b'\r\n');time.sleep(.2);s.read(4096)
    s.write(('loader open "'+remote+'" probe\r\n').encode());time.sleep(.1)
end=time.monotonic()+90;dev=None
while time.monotonic()<end:
    candidates=[d for d in hid.enumerate(0x0483,0x5741) if d['usage_page']==0xff71]
    if candidates:
        dev=hid.device();dev.open_path(candidates[0]['path']);break
    time.sleep(.2)
if dev is None:raise RuntimeError('Wallet HID did not enumerate')
def frame(cmd,session,req,data=b''):
    return b'FW'+bytes([1,cmd])+struct.pack('<IIHHI',session,req,len(data),0,0)+data+bytes(44-len(data))
def exchange(cmd,session,req,data=b''):
    dev.write(b'\0'+frame(cmd,session,req,data));reply=bytes(dev.read(64,3000))
    if len(reply)!=64:raise RuntimeError('No complete HID reply')
    assert reply[:4]==b'FW'+bytes([1,cmd|128]) and struct.unpack_from('<I',reply,8)[0]==req
    return struct.unpack_from('<I',reply,4)[0],reply[20:]
s,hello=exchange(1,0,1)
_,status=exchange(7,s,1,b'\0')
_,reject=exchange(3,s,2,b'\0')
assert hello[0]==0 and status[0]==0 and reject[0]==3
minimum=struct.unpack_from('<I',status,2)[0]
assert status[10]==15, 'On-device crypto selftest failed'
for index in range(3,103):
    _,cycle=exchange(7,s,index,b'\0')
    assert cycle[0]==0
    minimum=min(minimum,struct.unpack_from('<I',cycle,2)[0])
assert minimum>=16384
print(json.dumps({'fap_sha256':hashlib.sha256(Path(a.fap).read_bytes()).hexdigest(),'device_crypto_selftest_flags':status[10],'crypto_selftest_ms':struct.unpack_from('<I',status,6)[0],'minimum_observed_heap':minimum,'locked_status_cycles':100,'transport':'physical Flipper USB vendor HID','protocol':hello[1], 'free_heap':struct.unpack_from('<I',hello,4)[0], 'locked_signing_rejected':True,'session_established':bool(s)}))
dev.close()
