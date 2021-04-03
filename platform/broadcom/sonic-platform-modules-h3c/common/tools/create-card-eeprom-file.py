#!/usr/bin/python

# Since the XLR/GTS cards do not have an EEPROM, we create a file which
# will be use like an EEPROM.

import sys
import struct
from ctypes import *
import os
from datetime import datetime

class TLVINFO_HEADER(Structure):
    _fields_ = [("version1", c_ubyte),
                ("internal_offset", c_ubyte),
                ("chassis_offset", c_ubyte),
                ("board_offset", c_ubyte),
                ("product_offset", c_ubyte),
                ("multirecord_offset", c_ubyte),
                ("pad", c_ubyte),
                ("checksum", c_ubyte),
                ("version", c_ubyte),
                ("totallen",  c_ubyte),
                ("languageCode",c_ubyte),
                ("Mfg_Date_Time1",c_ubyte),
                ("Mfg_Date_Time2",c_ubyte),
                ("Mfg_Date_Time3",c_ubyte),]
    def dump(self):
        return struct.pack('B', self.version1) + \
                struct.pack('B', self.internal_offset) + \
                struct.pack('B', self.chassis_offset) + \
                struct.pack('B', self.board_offset) + \
                struct.pack('B', self.product_offset) + \
                struct.pack('B', self.multirecord_offset) + \
                struct.pack('B', self.pad) + \
                struct.pack('B', self.checksum) + \
                struct.pack('B', self.version) + \
                struct.pack('B', self.totallen)+\
                struct.pack('B',self.languageCode)+\
                struct.pack('B',self.Mfg_Date_Time1)+\
                struct.pack('B',self.Mfg_Date_Time2)+\
                struct.pack('B',self.Mfg_Date_Time3)

class TLVINFO_HEADER1(Structure):
    _fields_ = [("version1", c_ubyte),
                ("internal_offset", c_ubyte),
                ("chassis_offset", c_ubyte),
                ("board_offset", c_ubyte),
                ("product_offset", c_ubyte),
                ("multirecord_offset", c_ubyte),
                ("pad", c_ubyte)]
    def dump(self):
        return struct.pack('B', self.version1) + \
                struct.pack('B', self.internal_offset) + \
                struct.pack('B', self.chassis_offset) + \
                struct.pack('B', self.board_offset) + \
                struct.pack('B', self.product_offset) + \
                struct.pack('B', self.multirecord_offset) + \
                struct.pack('B', self.pad)           

class TLVINFO_HEADER2(Structure):
    _fields_ = [("custom", c_char*8),
                ("nofields", c_ubyte),
                ("unusedspace", c_ubyte)]
    def dump(self):
        return struct.pack('8s', self.custom) + \
                struct.pack('B', self.nofields) + \
                struct.pack('B', self.unusedspace)
class TLVINFO_DATA:
    data = [];
    def add_tlv_str(self, value):
        self.data.append(struct.pack('B', len(value)) + value.encode())
    def add_tlv_mac(self, value):
        self.data.append(struct.pack('B', len(value)))
        for v in value:
            self.data.append(struct.pack('B', int(v, 16)))
    def dump(self):
        r = '';
        for m in self.data:
            r += bytes(m)
        return r 
def bytes_to_int(byte2):
    result = 0
    for b in byte2:
        result = result * 256 + int(b)
    return result
def crc32(crc, p, len):
    for i in range(0,len):
        crc += int(p[i])
    crc &=0xFF
    crc=~crc
    crc &=0xFF
    return  crc

def crc(header, data, header2):
    r = '';
    for m in header:
        r += bytes(m)
    for m in data:
        r += bytes(m)
    for m in header2:
        r += bytes(m)        
    c = crc32(0, bytearray(r), len(r))
    s = struct.pack('B',c)
    return s

def crc1(header, data, header2):
    r = '';
    for m in header:
        r += bytes(m)    
    c = crc32(0, bytearray(r), len(r))
    return c

def help():
    s = '''
    Usage:
        create-eeprom.py <manufacturer> <product_name> <sn> <pn> <out_file_name>
        example:
            create-eeprom.py  H3C TCS83-120-32CQ  0231A05K00000001 NONE slot1.bin

    '''
    print(s)




def main():

     
    if len(sys.argv) != 6:
        help()
        return

    manufacturer = sys.argv[1]
    product_name = sys.argv[2]
    sn = sys.argv[3]
    pn = sys.argv[4]
    out_file = sys.argv[5]

    base = datetime(1996, 1, 1, 0,0,0,0)
    now  = datetime.now()
    manu_minutes = int((now - base).total_seconds() / 60)
    #print "0x%x 0x%x 0x%x" %(manu_minutes & 0xff, (manu_minutes & 0xff00) >> 8 , (manu_minutes & 0xff0000) >> 16)
    manu_0 =  manu_minutes & 0xff
    manu_1 = (manu_minutes >> 8) & 0xff
    manu_2 = (manu_minutes >> 16) & 0xff

    tlvinfo_header = TLVINFO_HEADER(1, 0, 0 ,8, 0 ,0 ,0 , 0, 1, 0 , 0, manu_0, manu_1, manu_2)
    tlvinfo_header1 = TLVINFO_HEADER1( 1, 0, 0 ,8, 0 ,0 ,0)
    tlvinfo_header2 = TLVINFO_HEADER2( 'custom', 0xc1, 00)
    tlvinfo_data = TLVINFO_DATA()
    tlvinfo_data.add_tlv_str(manufacturer)
    tlvinfo_data.add_tlv_str(product_name)
    tlvinfo_data.add_tlv_str(sn)
    tlvinfo_data.add_tlv_str(pn)

    tlvinfo_header.totallen = len(tlvinfo_data.dump())+len(tlvinfo_header2.dump())+6;
    tlvinfo_header.checksum = crc1(tlvinfo_header1.dump(), tlvinfo_data.dump(),tlvinfo_header2.dump());

    f = open(out_file, 'w+')
    f.write(tlvinfo_header.dump())
    f.write(tlvinfo_data.dump())
    f.write(tlvinfo_header2.dump())
    f.write(crc(tlvinfo_header.dump(), tlvinfo_data.dump(),tlvinfo_header2.dump()))
    f.close()
   
if __name__== "__main__":
    main()
