import serial
import sys
import time
import struct

BLOCKSIZE = 16

RES_GD = 0x47
RES_NG = 0x4e
RES_RP = 0x52

REQ_PING = 0x50

def ser_debugread(ser, nbytes):
    data = b''
    for i in range(nbytes):
        lonebyte = ser.read(1)
        print(lonebyte)
        data += lonebyte
    return data

def msgchksum(msg):
    return -sum(msg) & 0xff

def msgxchg(code,msg):
    msg = code.to_bytes(1,'little') + msg[:BLOCKSIZE-1]
    msg += bytes(BLOCKSIZE - len(msg))
    msg += msgchksum(msg).to_bytes(1, 'little')
    print(msg)
    for rpcnt in range(16):
        req = ser.write(msg)
        resp = ser.read(BLOCKSIZE)
        chksum = ser.read(1)[0]
        print(resp, chksum)
        if chksum != msgchksum(resp): 
            rpmsg = bytes([RES_RP]) + bytes(BLOCKSIZE - 1)
            ser.write(rpmsg)
            ser.write(msgchksum(rpmsg).to_bytes(1, 'little'))
        elif resp[0] != RES_RP:
            return resp[0],resp[1:]
    return None

def main(ser):
    print(msgxchg(REQ_PING, B''))
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <fname>')
        sys.exit(1)
    fname = sys.argv[1]
    ser = serial.Serial(fname, baudrate=115200)
    print("Waiting for Arduino to reset...")
    time.sleep(2)
    sys.exit(main(ser))
