#!/usr/bin/python

#############################################################################
# DELLEMC
#
# Module contains an implementation of SONiC Platform Base API and
# provides the platform information
# Author:qian.chaoyang@h3c.com
#############################################################################

import os ,sys
import re



def get_file_path(main_dir,sub_dir):
    dir =main_dir+sub_dir
    temp_read = None 
    try:
        temp_read = open(dir, "r")
        temp_value = temp_read.read()
    except IOError:
        temp_value = "io error"
    finally:
        if temp_read != None:
            temp_read.close()

    return temp_value

def exec_cmd(cmd_str):
    r = os.popen(cmd_str)
    info = r.read()
    r.close()
    return info


Onie_DIR        = "/sys/switch/syseeprom/"
SLOT_DIR        = "/sys/switch/slot/"
PSU_DIR         = "/sys/switch/psu/"
FAN_DIR         = "/sys/switch/fan/"
Main_board_DIR  = "/sys/switch/cpld/"
Main_board_DIR0 = "/sys/switch/cpld/cpld0/"
Main_board_DIR1 = "/sys/switch/cpld/cpld1/"
Main_board_DIR2 = "/sys/switch/cpld/cpld2/"
DEBUG_CPLD_DIR  = "/sys/switch/debug/cpld/"
SLOT_ITEM_DIR   = "/sys/switch/slot/slot{0}/"

###### get BIOS version #############
info = exec_cmd("sudo dmidecode -t 0 -t 11")
pattern1 = r"Version: *([^\n]+)"
pattern2 = r"H3C BIOS Version *([^\n]+)"
pattern3 = r"Vendor: *([^\n]+)"
pattern4 = r"Release Date: *([^\n]+)"
matchobj1 = re.search(pattern1, info)
matchobj2 = re.search(pattern2, info)
matchobj3 = re.search(pattern3, info)
matchobj4 = re.search(pattern4, info)
ver_str1 = matchobj1.group(1) if matchobj1 != None else "Unknown"
ver_str2 = matchobj2.group(1) if matchobj2 != None else "Unknown"
ver_str3 = matchobj3.group(1) if matchobj3 != None else "Unknown"
ver_str4 = matchobj4.group(1) if matchobj4 != None else "Unknown"
print("")
print("BIOS:")
print("    Vendor: " + ver_str3)
print("    Version: " + ver_str1 + "(AMI)")
print("    Version: " + ver_str2 + "(H3C)")
print("    Release Date: " + ver_str4)

###### BMC version  ###########################
r = os.popen("sudo ipmitool mc info | grep Firmware")
info = r.read()
r.close()
pattern = ": *([^\n]+)"
matchobj = re.search(pattern, info)
ver_str = matchobj.group(1) if matchobj != None else "Unknown"
###### get BMC version #############
print("")
print("BMC:")
print("    Vendor: H3C")
print("    Version: " + ver_str)
print("    Device Model: AST2500")

###### get Onie version #############
cmd1 = "cat /host/machine.conf | grep onie_build_date"
cmd2 = "cat /host/machine.conf | grep onie_version"
#onie_version = exec_cmd(cmd2)
onie_version = get_file_path('/sys/switch/syseeprom/', 'ONIE_version')
onie_date = exec_cmd(cmd1)

pattern = r"=(.+)"
matchobj2 = re.search(pattern, onie_date)
#ver_str1 = matchobj1.group(1) if matchobj1 != None else "Unknown"

ver_str1 = onie_version.strip()
ver_str2 = matchobj2.group(1) if matchobj2 != None else "Unknwon"
print("")
print("ONIE:")
print("    Build Date: " + ver_str2)
print("    Version: " + ver_str1)

#Board version
cpu_cpld_content = get_file_path(DEBUG_CPLD_DIR, "cpu_cpld")
exp = '0x0000: (\\w{2}) (\\w{2}) \\w{2} (\\w{2})'
ver_arr = re.findall(exp, cpu_cpld_content)[0]
###### get Borad version #############i
board_version = int(get_file_path(Main_board_DIR0, "board_version"))
cpu_version =  int(ver_arr[0], 16)

board_version = board_version - (0xa if board_version >= 0xa else 0x0)
cpu_version = cpu_version - (0xa if board_version >= 0xa else 0x0)

print("")
print("BOARD")
print("    BOARD1:")
print("        Description: Main Board")
print(("        Hard Version: " + '%s' %((chr(board_version + 65)))))
print("    BOARD2:")
print("        Description: CPU Board")
print(("        Hard Version: " + '%s' %(chr(65 + cpu_version))))

####### CPU ########################
cmd1 = "sudo dmidecode --type processor"
info = exec_cmd(cmd1)
pattern1 = "Manufacturer: *([^\n]+)"
pattern2 = "Version: *([^\n]+)"
pattern3 = "Core Count: *([^\n]+)"
pattern4 = "Thread Count: *([^\n]+)"
matchobj1 = re.search(pattern1, info)
matchobj2 = re.search(pattern2, info)
matchobj3 = re.search(pattern3, info)
matchobj4 = re.search(pattern4, info)
ver_str1 = matchobj1.group(1) if matchobj1 != None else "Unknown"
ver_str2 = matchobj2.group(1) if matchobj2 != None else "Unknown"
ver_str3 = matchobj3.group(1) if matchobj3 != None else "Unknown"
ver_str4 = matchobj4.group(1) if matchobj4 != None else "Unknown"

print("CPU:")
print(("    Vendor: " + ver_str1))
print(("    Device Model: " + ver_str2))
print(("    Core Count: " + ver_str3))
print(("    Thread Count: " + ver_str4))



########### memory #####################
cmd1 = "sudo lshw -class memory"
info = exec_cmd(cmd1)
pattern1 = "description: *([^\n]+)"
pattern2 = "product: *([^\n]+)"
pattern3 = "vendor: *([^\n]+)"
pattern4 = "clock: *([^\n]+)"
trim_pat1 = "-bank:\d[^*]+"
pattern5 = "-bank:(\d+)"

print("")
print("Memory:")
matchobj = re.findall(trim_pat1, info)
for bank in matchobj:
    if "NO DIMM" in bank:
        continue

    matchobj1 = re.search(pattern1, bank)
    matchobj2 = re.search(pattern2, bank)
    matchobj3 = re.search(pattern3, bank)
    matchobj4 = re.search(pattern4, bank)
    matchobj5 = re.search(pattern5, bank)
    ver_str1 = matchobj1.group(1) if matchobj1 != None else "Unknown"
    ver_str2 = matchobj2.group(1) if matchobj2 != None else "Unknown"
    ver_str3 = matchobj3.group(1) if matchobj3 != None else "Unknown"
    ver_str4 = matchobj4.group(1) if matchobj4 != None else "Unknown"
    ver_str5 = matchobj5.group(1) if matchobj5 != None else "Unknown"

    print(("    Bank%s:" %(ver_str5) ))
    print(("        Description: " + ver_str1))
    print(("        Device Model: " + ver_str2))
    print(("        Vendor: " + ver_str3))
    print(("        Clock: " + ver_str4))
    print("        Firmware: NA")


########### memory #####################
cmd1 = "sudo smartctl -i /dev/sda"
info = exec_cmd(cmd1)
pattern1 = "Device Model: *([^\n]+)"
pattern2 = "Firmware Version: *([^\n]+)"
pattern3 = "User Capacity: *([^\n]+)"

print("")
print("SSD:")
matchobj1 = re.search(pattern1, info)
matchobj2 = re.search(pattern2, info)
matchobj3 = re.search(pattern3, info)

ver_str1 = matchobj1.group(1) if matchobj1 != None else "Unknown"
ver_str2 = matchobj2.group(1) if matchobj2 != None else "Unknown"
ver_str3 = matchobj3.group(1) if matchobj3 != None else "Unknown"

print(("    Device Model: " + ver_str1))
print(("    Firmware Version: " + ver_str2))
print(("    User Capacity:" + ver_str3))


###### get CPLD version #############
print("")
print('CPLD:')
#mainboard0_version = get_file_path(Main_board_DIR0,"hw_version")
#mainboard1_version = get_file_path(Main_board_DIR1,"hw_version")
#mainboard2_version = get_file_path(Main_board_DIR2,"hw_version")

#fw1 = str(int(mainboard0_version, 16)) + '.' + str(int(mainboard1_version,16)) + '.' + str(int(mainboard2_version,16))
fw2 = "%d" %(int(ver_arr[2], 16) & 0xf )

cpld_num = get_file_path(Main_board_DIR, "num_cplds")

for i in range(0, int(cpld_num)):
    dev_model = get_file_path(Main_board_DIR + "/cpld" + str(i) + "/" , "type")
    vendor    = "Lattice"
    fw_version = get_file_path(Main_board_DIR + "/cpld" + str(i) + "/", "hw_version")
    print(("    CPLD%d:" %(i)))
    print(("        Device Model: " + dev_model))
    print("        Vendor: Lattice")
    print("        Description: Main board")
    print(("        Firmware Version: " + fw_version))

print(("    CPLD%d:" %(i + 1)))
print("        Device Model: XO3 6900")
print("        Vendor: Lattice")
print("        Description: CPU board" )
print(("        Firmware Version: " + fw2))


###### get PSU version #############
print("")
print('PSU:')
psu_num = get_file_path(PSU_DIR, "num_psus")
for n in range(1, int(psu_num) + 1):
    temp_psu_dir = PSU_DIR + "psu%d/" %n
    status = get_file_path(temp_psu_dir, "status").strip()
    if status == "1":
        hw_ver = get_file_path(temp_psu_dir, "hw_version").strip()
        fw_ver = get_file_path(temp_psu_dir, "fw_version").strip()
    else:
        hw_ver = "NA"
        fw_ver = "NA"
    print(("    PSU%d:" %n))
    print(("        Hardware Version: %s" %hw_ver))
    print(("        Firmware Version: %s" %fw_ver))

###### get FAN version #############
print("")
print('FAN:')
fan_num = get_file_path(FAN_DIR, "num_fans")
for n in range(1, int(fan_num) + 1):
    temp_fan_dir = FAN_DIR + "fan%d/" %n
    fan_status = get_file_path(temp_fan_dir, "status").strip()
    if fan_status == "1":
        fan_hw_ver = get_file_path(temp_fan_dir, "hw_version").strip()
    else:
        fan_hw_ver = "NA"
    print(("    FAN%d:" %(n)))
    print(("        Hardware Version: %s" %(fan_hw_ver)))
    print("        Firmware Version: NA")


###### get SLOT version #############
if os.path.exists(SLOT_DIR):
    print("")
    print('SLOT:')
    slot_num = int(get_file_path(SLOT_DIR,"num_slot"))
    if slot_num:
        for i in range(1, slot_num + 1):
            slot_status = int(get_file_path(SLOT_ITEM_DIR.format(i), "status"))
            if slot_status == 1:
                slot_version = chr(int(get_file_path(SLOT_ITEM_DIR.format(i), "hw_version")) + 65)
            else:
                slot_version = "NA"
            print(("    SLOT%d:" %(i)))
            print(('        Hard Version: %s' %(slot_version)))

###### get I210 firmware version #############
info = exec_cmd("sudo ethtool -i eth0")
pattern = r"firmware-version: *([^\n]+)"
matchobj = re.search(pattern, info)
tem=matchobj.group(1).split(",",1)
#ver_str = matchobj.group(1) if matchobj != None else "Unknown"
ver_str = tem[0] if matchobj != None else "Unknown"
print("")
print("NIC:")
print("    Device Model: I350 ")
print("    Vendor: Intel")
print("    Firmware Version: " + ver_str)


