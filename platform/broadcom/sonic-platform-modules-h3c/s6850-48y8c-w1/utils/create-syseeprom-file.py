#!/usr/bin/python


import sys
import struct
from ctypes import *
import os
import re

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
        netns = os.popen("ip netns")
        if netns.read() != "" :
            #mac1 = os.popen('ip netns exec mgmt cat /sys/class/net/'+interface+'/address')
            mac1 = os.popen('ip netns exec mgmt ethtool -e ' + interface + '  | grep 0x0100')
        else:
            mac1 = os.popen('ethtool -e ' + interface + '  | grep 0x0100')
            
        mac = mac1.read().split()[1:7]
        mac = ":".join(mac)
        mac1.close()
        netns.close()
        #mac = "00:22:33:44:55:66"
    except:
        mac = "00:00:00:00:00:00"
    return mac
  
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
def usage():
    usage_str = '''
    write system eeprom to eeprom

    Usage:
        create-syseeprom.py <Vendor> <Product_name> <SN> <product_time>
        Vendor : H3C
        Product_name : S6850-48Y8C-W1/S9850-32C-W1/S9820-4M-W1
        SN: Serial number of the device
        product time: format is "MM/DD/YYYY HH:MM:SS" 
    '''
    print(usage_str)
def main():
    if len(sys.argv) != 5:
        usage()
        exit()

    time_format = "^\d{2}/\d{2}/\d{4} \d{2}:\d{2}:\d{2}$"
    Vendor = sys.argv[1]
    product_name = sys.argv[2]
    sn = sys.argv[3]
    pn = sn[2:10]
    product_time = sys.argv[4]

    arg_err = False
    if product_name not in ['S6850-48Y8C-W1', 'S9850-32C-W1', 'S9820-4M-W1']:
        print("product name %s is invalid, S6850-48Y8c-W1/S9850-32C-W1/S9820-4M-W1" %product_name)
        arg_err = True
    if Vendor not in ['H3C']:
        print("Vendor %s is invalid, H3C" %Vendor)
        arg_err = True
    if re.match(time_format, product_time) == None:
        print("product time %s is invalid. " %product_time)
        arg_err = True

    if arg_err: 
        exit()

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
    tlvinfo_data.add_tlv_str(TLV_CODE_SERIAL_NUMBER, sn)     
    tlvinfo_data.add_tlv_str(TLV_CODE_PRODUCT_NAME,  product_name)
    tlvinfo_data.add_tlv_str(TLV_CODE_PART_NUMBER,  '120F%s'%pn)
    tlvinfo_data.add_tlv_str(TLV_CODE_MANUF_NAME,    'H3C')
    tlvinfo_data.add_tlv_str(TLV_CODE_MANUF_DATE,  product_time)
    tlvinfo_data.add_tlv_str(TLV_CODE_MANUF_COUNTRY,  'CHN')
    tlvinfo_data.add_tlv_str(TLV_CODE_DEVICE_VERSION, "d")
    tlvinfo_data.add_tlv_str(TLV_CODE_VENDOR_NAME, Vendor)
    tlvinfo_data.add_tlv_str(TLV_CODE_DIAG_VERSION, "0.1.0.16")
    tlvinfo_data.add_tlv_str(TLV_CODE_SERVICE_TAG, "www.h3c.com")
    tlvinfo_data.add_tlv_str(TLV_CODE_LABEL_REVISION, "R01") 
    tlvinfo_data.add_tlv_mac(TLV_CODE_MAC_SIZE, ["00","05"]);
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
