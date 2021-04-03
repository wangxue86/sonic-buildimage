#!/usr/bin/python

import sys
import os

PARAM_SYSFS_PATH = '/sys/switch/debug/i2c/param'
READ_SYSFS_PATH = '/sys/switch/debug/i2c/mem_dump'

def help():
    help_str = ''' 
    Usage:
        i2c_read.py <dev_id> <i2c_addr> <mode> <start> <read_len>
        dev_id   : i2c path id
        i2c_addr : i2c device address
        mode     : 1 (address 8bit data 8bit), 2 (address 16bit, data 8bit)
        start    : start from device inner address
        read_len : how many bytes read from i2c device.
    example:
        i2c_read.py 245 0x50 1 0 256

        'cat /sys/switch/debug/i2c/param' for furture help
    '''
    extra_help = ''
    f = os.popen('cat ' + PARAM_SYSFS_PATH)
    extra_help = f.read()
    f.close()

    print(help_str)
    print("----------extra help from %s------------" %(PARAM_SYSFS_PATH))
    print(extra_help)


def main():

    if len(sys.argv) != 6:
        help()
        return;

    param = ["","","","","",""]
    success_count = 0;
    failed_count = 0;

    for i in range(1, len(sys.argv)):
        try:
            param[i]   = int(sys.argv[i])
        except ValueError as e:
            param[i]   = int(sys.argv[i], 16)
    
    dev_id = param[1]
    i2c_addr = param[2]
    mode = param[3]
    start_addr = param[4]
    read_len = param[5]

    cmd = 'echo "path 0x%x addr 0x%x read from 0x%x len 0x%x mode 0x%x" > %s' %(dev_id, i2c_addr, start_addr, read_len, mode,PARAM_SYSFS_PATH) 
    if (os.system(cmd) != 0):
        print("cmd '%s' failed!" %cmd)
    else:
        cmd = 'cat %s'%(READ_SYSFS_PATH)
        f = os.popen(cmd)
        print((f.read()))
        f.close()

if __name__== "__main__":
    main()




