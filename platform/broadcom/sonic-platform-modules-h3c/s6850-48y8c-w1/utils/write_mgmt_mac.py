#!/usr/bin/python



import sys
import os

def help():
    help_str = '''
    write eth0, eth2, eth3 mac address to eeprom, eth1 mac will not writed
        eth0_mac = base_mac + 0
        ASIC_mac = base_mac + 1 
        eth2_mac = base_mac + 2
        eth3_mac = base_mac + 3
         BMC_mac = base_mac + 4
    Notes:
        BMC mac and ASIC mac will not program to eeprom. The 2 mac addresses should be reserved for customer.

    Usage:
        write_mgmt_mac.py <base_mac>
        base_mac  format: aa:bb:cc:dd:ee:ff
    '''
    print help_str

def mac_int_to_str(mac_int):
    mac_str = ""
    for i in range(0, 6):
        mac_byte = (mac_int >> (i * 8)) & 0xff
        temp_str = "%02x" %(mac_byte)
        mac_str = temp_str + ":" + mac_str
    
    mac_str = mac_str.strip(":")
    return mac_str

def write_mac_to_eth(mac_str, eth_str, magic_code, start_addr):
    mac_str_arr = mac_str.split(":")
    count = 0
    for s in mac_str_arr:
        cmd = "sudo ip netns exec default ethtool -E %s magic %s offset 0x%x value 0x%s" %(eth_str, magic_code, start_addr + count, s)
        count += 1
        print(cmd)
        if os.system(cmd) == 0:
            print "Success"
        else:
            print "Failed"



if len(sys.argv) != 2:
    help()
    exit()

mac_addr = sys.argv[1]
mac_bytes = mac_addr.split(":")

if len(mac_bytes) != 6:
    print("format error! mac should like aa:bb:cc:dd:ee:ff")
    exit()

mac0 = 0
for b in mac_bytes:
    n = int(b, 16)
    if n > 0xff:
        print("'%s' is invalid for mac address!" %(str(b)))
        exit()
    mac0 = (mac0 << 8) + n
base_mac = mac0

eth0 = base_mac + 0
asic = base_mac + 1
eth2 = base_mac + 2
eth3 = base_mac + 3
bmc  = base_mac + 4

mac0_str = mac_int_to_str(eth0)
asic_str = mac_int_to_str(asic)
mac2_str = mac_int_to_str(eth2)
mac3_str = mac_int_to_str(eth3)
bmc_str  = mac_int_to_str(bmc)

write_mac_to_eth(mac0_str, "eth1", "0x15228086", 0x100)
write_mac_to_eth(mac2_str, "eth2", "0x15ab8086", 0x202)
write_mac_to_eth(mac3_str, "eth2", "0x15ab8086", 0x212)

print "eth0:" + mac0_str
print "asic:" + asic_str + "(reserved)"
print "eth2:" + mac2_str
print "eth3:" + mac3_str
print " bmc:" + bmc_str +  "(reserved)"






