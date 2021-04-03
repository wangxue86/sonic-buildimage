#!/usr/bin/python


import re
import sys
import os

TOTAL_LEN = 256

FAN_SN_START = 0x28 
FAN_SN_LEN = 64
FAN_PDT_NAME_START = 0x8
FAN_PDT_NAME_LEN = 32
FAN_START_I2C_PATH_ID = 438

arr = []

def help():
    usage = '''
    tobin.py <FAN_ID> <product_name> <SN>
    output a binary called 
    '''
    print(usage)

if len(sys.argv) != 4:
    help()
    exit()

product_name = sys.argv[2]
sn = sys.argv[3]
fan_id = sys.argv[1]
binary = "__temp.fan.bin"

#print product_name
#print sn
#print binary


arr = [0] * TOTAL_LEN

for i in range(0, FAN_SN_LEN):
    arr[i + FAN_SN_START] = ord(sn[i]) if i < len(sn) else 0
    #print arr[i + FAN_SN_START]

for i in range(0, FAN_PDT_NAME_LEN):
    arr[i + FAN_PDT_NAME_START] = ord(product_name[i]) if i < len(product_name) else 0
    #print arr[i + FAN_PDT_NAME_START]

bytes_arr = arr

count = 0
buffer = ""

for b in bytes_arr:
    buffer += chr(b)
    count = count + 1

f = open(binary, "wb")
f.write(buffer)
f.close()

f = os.popen("sudo cat /sys/switch/debug/i2c/param")
info = f.readlines()
f.close()

cmd = "sudo i2c_write.py %d 0x50 2 %d %s" %(FAN_START_I2C_PATH_ID + int(fan_id) - 1, count, binary)
#print cmd
os.system(cmd)
os.system("sudo rm -f %s" %binary)
print(("fan %d : %d bytes writed" %(int(fan_id), count)))








