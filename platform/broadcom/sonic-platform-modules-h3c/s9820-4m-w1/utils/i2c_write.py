#!/usr/bin/python

import sys
import os

PARAM_SYSFS_PATH = '/sys/switch/debug/i2c/param'
WRITE_SYSFS_PATH = '/sys/switch/debug/i2c/do_write'

def help():
    help_str = ''' 
    Usage:
        i2c_write.py <dev_id> <i2c_addr> <mode> <write_len> <bin_file>
        dev_id   : i2c path id
        i2c_addr : i2c device address
        mode     : 1 (address 8bit data 8bit), 2 (address 16bit, data 8bit)
        write_len: how many bytes write to i2c device. if write_leng > bin_file size, '0' will filled.
        bin_file : binary file path
    example:
        i2c_write.py 245 0x50 1 256  /home/a.bin

        'cat /sys/switch/debug/i2c/param' for furture help
    '''
    print help_str


def main():

    if len(sys.argv) != 6:
        help()
        return;

    param = ["","","","","",""]
    success_count = 0;
    failed_count = 0;

    for i in range(1, len(sys.argv) - 1):
        try:
            param[i]   = int(sys.argv[i])
        except ValueError as e:
            param[i]   = int(sys.argv[i], 16)
    
    param[i + 1] = sys.argv[i + 1]

    dev_id = param[1]
    i2c_addr = param[2]
    mode = param[3]
    write_len = param[4]
    bin_file = param[5]


    fi = open(bin_file, 'rb')
    content = fi.read()
    fi.close()

    content_len = len(content)
    print "Input file %s size: %d" %(bin_file, content_len)

    for i in range(0, write_len):
        byte = ord(content[i]) if i < content_len else 0
        cmd = 'echo "path 0x%x addr 0x%x write inner 0x%x value 0x%x mode 0x%x" > %s' %(dev_id, i2c_addr, i, byte, mode, PARAM_SYSFS_PATH) 
        if 0 != os.system(cmd):
            print "exit for failed cmd '%s'" %(cmd)
            break
        else:
            fi = open(WRITE_SYSFS_PATH, "r")
            result = fi.read()
            fi.close()
            if "success" in result:
                sys.stdout.write(".")
                success_count += 1
            else:
                sys.stdout.write("x")
                failed_count += 1
            sys.stdout.flush()
        
    print "\n"
    print "%d bytes writen. %d success, %d failed" %(i + 1, success_count, failed_count)


if __name__== "__main__":
    main()




