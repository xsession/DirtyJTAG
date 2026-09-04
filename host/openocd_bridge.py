#!/usr/bin/env python3
"""OpenOCD remote_bitbang <-> DirtyJTAG Universal Pico DJP2 bridge.

This makes the Pico a real OpenOCD/GDB debug probe for ARM SWD and generic
JTAG without requiring a second USB class in the firmware.  OpenOCD connects
to this process over TCP; this process batches the documented remote_bitbang
ASCII operations into DJP2 DEBUG_BITBANG USB commands.
"""
from __future__ import annotations
import argparse, socket, struct

try:
    from djp2 import MAX_PAYLOAD, Link
except ImportError:  # Allows module execution from outside host/.
    from host.djp2 import MAX_PAYLOAD, Link

CMD_CONFIG=3; CMD_ENTER=0x10; CMD_LEAVE=0x11; CMD_DEBUG_BITBANG=0x50; CMD_DEBUG_ATTACH=0x52; CMD_DEBUG_DETACH=0x53; CMD_SAFE=0x7e
PROTO={'swd':60,'jtag':61}; POWER={'off':0,'external':1,'3v3':2,'5v':3}

def configure(link,transport,power,clock):
    # proto:u16 power:u8 flags:u8 clock:u32 vtarget:u32 flash:u32 page:u16 namelen:u8
    payload=struct.pack('<HBBIIIHB',PROTO[transport],POWER[power],0,clock,3300,0,0,0)
    link.x(CMD_CONFIG,payload)
    link.x(CMD_ENTER)
    link.x(CMD_DEBUG_ATTACH)

def serve(link,host,port,verbose=False):
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        srv.bind((host,port)); srv.listen(1)
        print(f'OpenOCD remote_bitbang bridge listening on {host}:{port}')
        print('Start OpenOCD now; Ctrl+C stops the bridge.')
        while True:
            conn,addr=srv.accept()
            print(f'OpenOCD connected from {addr[0]}:{addr[1]}')
            with conn:
                conn.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
                while True:
                    data=conn.recv(MAX_PAYLOAD)
                    if not data: break
                    if verbose: print(f'RX {len(data)} ops')
                    # Q tells remote_bitbang server that OpenOCD is done. Execute any
                    # operations before it, then close after returning read samples.
                    quit_after=b'Q' in data
                    reply=link.x(CMD_DEBUG_BITBANG,data)
                    if reply: conn.sendall(reply)
                    if quit_after: break
            print('OpenOCD disconnected')

def main():
    ap=argparse.ArgumentParser(description='DirtyJTAG Pico OpenOCD remote_bitbang bridge')
    ap.add_argument('--port',required=True,help='Pico USB CDC serial port, e.g. COM8 or /dev/ttyACM0')
    ap.add_argument('--transport',choices=PROTO,default='swd')
    ap.add_argument('--power',choices=POWER,default='external')
    ap.add_argument('--clock',type=int,default=1000000,help='stored target clock hint (remote_bitbang itself is host paced)')
    ap.add_argument('--listen',default='127.0.0.1')
    ap.add_argument('--tcp-port',type=int,default=3335)
    ap.add_argument('-v','--verbose',action='store_true')
    a=ap.parse_args()
    l=Link(a.port, timeout=5, connect_delay=.15)
    try:
        configure(l,a.transport,a.power,a.clock)
        serve(l,a.listen,a.tcp_port,a.verbose)
    except KeyboardInterrupt:
        pass
    finally:
        try: l.x(CMD_DEBUG_DETACH)
        except Exception: pass
        try: l.x(CMD_LEAVE)
        except Exception: pass
        try: l.x(CMD_SAFE)
        except Exception: pass
        l.close()

if __name__=='__main__': main()
