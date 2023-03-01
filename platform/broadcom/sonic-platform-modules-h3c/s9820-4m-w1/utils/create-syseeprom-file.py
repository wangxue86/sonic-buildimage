#!/usr/bin/python


import sys
import struct
from ctypes import *
import os


TLV_CODE_PRODUCT_NAME  = 0x21
TLV_CODE_PART_NUMBER   = 0x22
TLV_CODE_SERIAL_NUMBER = 0x23
TLV_CODE_MAC_BASE      = 0x24
TLV_CODE_MANUF_DATE    = 0x25
TLV_CODE_DEVICE_VERSION= 0x26
TLV_CODE_LABEL_REVISION= 0x27
TLV_CODE_PLATFORM_NAME = 0x28
TLV_CODE_ONIE_VERSION  = 0x29
TLV_CODE_MAC_SIZE      = 0x2A
TLV_CODE_MANUF_NAME    = 0x2B
TLV_CODE_MANUF_COUNTRY = 0x2C
TLV_CODE_VENDOR_NAME   = 0x2D
TLV_CODE_DIAG_VERSION  = 0x2E
TLV_CODE_SERVICE_TAG   = 0x2F
TLV_CODE_VENDOR_EXT    = 0xFD
TLV_CODE_CRC_32        = 0xFE

def getmac(interface):
    try:
        mac = open('/sys/class/net/'+interface+'/address').readline()
        #mac = "00:22:33:44:55:66"
    except:
        mac = "00:00:00:00:00:00"
    return mac[0:17]
  
class TLVINFO_HEADER(Structure):
    _fields_ = [("signature", c_char*8),
                ("version",   c_ubyte),
                ("totallen",  c_ushort)]
    def dump(self):
        return struct.pack('8s', self.signature) + \
               struct.pack('B', self.version) + \
               struct.pack('>H', self.totallen)

class TLVINFO_DATA:
    data = [];
    def add_tlv_str(self, type, value):
        self.data.append(struct.pack('B', type) + struct.pack('B', len(value)) + value.encode())
    def add_tlv_mac(self, type, value):
        self.data.append(struct.pack('B', type) + struct.pack('B', len(value)))
        for v in value:
            self.data.append(struct.pack('B', int(v, 16)))
    def dump(self):
        r = '';
        for m in self.data:
            r += bytes(m)
        return r + struct.pack('B', TLV_CODE_CRC_32) + struct.pack('B', 4)

def crc32(crc, p, len):
    crc = 0xffffffff & ~crc
    for i in range(len):
        crc = crc ^ p[i]
        for j in range(8):
            crc = (crc >> 1) ^ (0xedb88320 & -(crc & 1))
    return 0xffffffff & ~crc
  
def crc(header, data):
    r = '';
    for m in header:
        r += bytes(m)
    for m in data:
        r += bytes(m)
    c = crc32(0, bytearray(r), len(r))
    s = struct.pack('>I', c)
    return s
   
def main():

    tlvinfo_header = TLVINFO_HEADER('TlvInfo', 1, 0)

    tlvinfo_data = TLVINFO_DATA()
    #onie_machine = os.popen("cat /host/machine.conf | grep 'onie_machine=' | sed 's/onie_machine=//'").read().strip()
    #tlvinfo_data.add_tlv_str(TLV_CODE_ONIE_MACHINE, onie_machine)

    eth0_mac_str = getmac('eth0')
    eth0_mac = eth0_mac_str.split(':')
    tlvinfo_data.add_tlv_mac(TLV_CODE_MAC_BASE, eth0_mac)

    onie_version = os.popen("cat /host/machine.conf | grep 'onie_version' | sed 's/onie_version=//'").read().strip()
    tlvinfo_data.add_tlv_str(TLV_CODE_ONIE_VERSION, onie_version)

    onie_platform = os.popen("cat /host/machine.conf | grep 'onie_platform=' | sed 's/onie_platform=//'").read().strip()
    tlvinfo_data.add_tlv_str(TLV_CODE_PLATFORM_NAME, onie_platform)  


    #02A5GC020100007 02A5GC020100008 02A5GC02010000H
    tlvinfo_data.add_tlv_str(TLV_CODE_SERIAL_NUMBER, '02A5GC02010000N')     
    tlvinfo_data.add_tlv_str(TLV_CODE_PRODUCT_NAME,  'S9820-4M-W1')
    tlvinfo_data.add_tlv_str(TLV_CODE_PART_NUMBER,  '120F0302A5GC')
    tlvinfo_data.add_tlv_str(TLV_CODE_MANUF_NAME,    'H3C')
    tlvinfo_data.add_tlv_str(TLV_CODE_MANUF_DATE,  '03/03/2020 17:00:00')
    tlvinfo_data.add_tlv_str(TLV_CODE_MANUF_COUNTRY,  'CHN')
    tlvinfo_data.add_tlv_str(TLV_CODE_DEVICE_VERSION, "d")
    tlvinfo_data.add_tlv_str(TLV_CODE_VENDOR_NAME, "H3C")
    tlvinfo_data.add_tlv_str(TLV_CODE_DIAG_VERSION, "0.1.0.16")
    tlvinfo_data.add_tlv_str(TLV_CODE_SERVICE_TAG, "www.h3c.com")
    tlvinfo_data.add_tlv_str(TLV_CODE_LABEL_REVISION, "R01") 
    tlvinfo_data.add_tlv_mac(TLV_CODE_MAC_SIZE, ["00","03"]);
    tlvinfo_data.add_tlv_str(TLV_CODE_VENDOR_EXT, "DEMO")


    tlvinfo_header.totallen = len(tlvinfo_data.dump())+4;



    try:
        f = open('sys_eeprom.bin', 'w+')
        f.write(tlvinfo_header.dump())
        f.write(tlvinfo_data.dump())
        f.write(crc(tlvinfo_header.dump(), tlvinfo_data.dump()))
        f.close()
        os.system("sudo cat sys_eeprom.bin > /sys/switch/debug/eeprom")
        os.system("sudo rm -f /var/cache/sonic/decode-syseeprom/syseeprom_cache")
    except:
        print('Unable to write file ./sys_eeprom.bin')

if __name__== "__main__":
    main()
