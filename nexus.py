import serial
import sys
import time
import struct

BLOCKSIZE = 16

RES_GD = 0x47
RES_NG = 0x4e
RES_RP = 0x52

CMD_PING = 0x50
CMD_CFG = 0x43
CMD_JR_RW = 0x57
CMD_RESET = 0x54

CAPTURE_IR_FLAG = 0x80
WAIT_READY_FLAG = 0x40
TO_IDLE_FLAG = 0x20
JTAG_REG_MAX_LEN = 96

def ser_debugread(ser, nbytes):
    data = b''
    for i in range(nbytes):
        lonebyte = ser.read(1)
        print(lonebyte)
        data += lonebyte
    return data

def itob(i):
    return i.to_bytes(1, 'little')

def msgchksum(msg):
    return -sum(msg) & 0xff

def msgxchg(ser,code,msg):
    if len(msg) >= BLOCKSIZE:
        return None
    msg = code.to_bytes(1,'little') + msg[:BLOCKSIZE-1]
    msg += bytes(BLOCKSIZE - len(msg))
    msg += itob(msgchksum(msg))
    print(msg)
    for rpcnt in range(16):
        req = ser.write(msg)
        resp = ser.read(BLOCKSIZE)
        chksum = ser.read(1)[0]
        print(resp, chksum)
        if chksum != msgchksum(resp): 
            rpmsg = bytes([RES_RP]) + bytes(BLOCKSIZE - 1)
            ser.write(rpmsg)
            ser.write(itob(msgchksum(rpmsg)))
        elif resp[0] != RES_RP:
            return resp[0],resp[1:]
    return None

def jtag_reg_rw(ser, reg, rlen, flags):
    if rlen > JTAG_REG_MAX_LEN:
        return None
    regdata = reg.to_bytes((rlen + 7) // 8, 'little')
    msgxchg(ser, CMD_JR_RW, itob(rlen) + itob(flags) + regdata)

def main(ser):
    # Read IDCODE
    print(jtag_reg_rw(ser, 0, 32, TO_IDLE_FLAG))
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
