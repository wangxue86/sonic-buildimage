#!/usr/bin/python

import os
import math

HWMON1_DIR = "/sys/class/hwmon/hwmon1/"
HWMON2_DIR = "/sys/class/hwmon/hwmon4/"

#FAN1_DIR = "/sys/switch/fan/fan1/"
#FAN2_DIR = "/sys/switch/fan/fan2/"
#FAN3_DIR = "/sys/switch/fan/fan3/"
#FAN4_DIR = "/sys/switch/fan/fan4/"
#FAN5_DIR = "/sys/switch/fan/fan5/"
#FAN6_DIR = "/sys/switch/fan/fan6/"

FAN_DIR = "/sys/switch/fan/fan%d/"
FAN_NUM = 6
#PSU1_DIR = "/sys/switch/psu/psu1/"
#PSU2_DIR = "/sys/switch/psu/psu2/"
#PSU3_DIR = "/sys/switch/psu/psu3/"
#PSU4_DIR = "/sys/switch/psu/psu4/"

PSU_DIR = "/sys/switch/psu/psu%d/"
PSU_NUM = 4



SLOT_DIR = "/sys/switch/slot/slot%d/"
SLOT_NUM = 4


SENSOR_DIR = "/sys/switch/sensor/"

def find_all_hwmon_paths(name):
    hw_list = os.listdir('/sys/class/hwmon/')
    path_list = []
    for node in hw_list:
        hw_name = ''
        hw_dir = '/sys/class/hwmon/%s/'%(node)
        f = open(hw_dir + 'name', 'r' )
        hw_name = f.read()
        if name in hw_name:
            path_list.append(hw_dir)
        f.close()
    return path_list

def get_file_path(main_dir,sub_dir):
    dir =main_dir+sub_dir
    temp_read = open(dir, "r")
    temp_value = temp_read.read()
    temp_read.close()
    return temp_value

temp_ratio = 0.001
def get_point_data(dir,temp_label,temp_input,temp_max,temp_crit,temp_min=0): 
    temp_label      = get_file_path(dir,temp_label).strip()
    temp_input      =int(get_file_path(dir,temp_input))* temp_ratio
    if temp_input >0:
        temp_input = "%s" % (temp_input)
    elif temp_input <0:
        temp_input = "%s" % (temp_input)
    temp_max      =int(get_file_path(dir,temp_max))* temp_ratio
    if temp_max >0:
        temp_max = "%s" % (temp_max)
    elif temp_max <0:
        temp_max = "%s" % (temp_max)
    temp_crit       = int(get_file_path(dir,temp_crit))* temp_ratio
    if temp_crit >0:
        temp_crit = "%s" % (temp_crit)
    elif temp_crit <0:
        temp_crit = "%s" % (temp_crit)
    if temp_min != 0:
        temp_min   = int(get_file_path(dir,temp_min))* temp_ratio
        if temp_min >0:
            temp_min = "%s" % (temp_min)
        elif temp_min <0:
            temp_min = "%s" % (temp_min)
    else :
        temp_min =0

    return temp_label,temp_input,temp_max,temp_crit,temp_min


def get_coretemp_data(dir):
    temp_label,temp_input,temp_max,temp_crit,temp_min = get_point_data(dir,"temp2_label","temp2_input","temp2_max","temp2_crit")
    msg = "        %s      : %s C  (high = %s C, crit = %s C)" % (temp_label,temp_input,temp_max,temp_crit)
    print(msg)
    temp_label,temp_input,temp_max,temp_crit,temp_min = get_point_data(dir,"temp3_label","temp3_input","temp3_max","temp3_crit")
    msg = "        %s      : %s C  (high = %s C, crit = %s C)" % (temp_label,temp_input,temp_max,temp_crit)
    print(msg)
    temp_label,temp_input,temp_max,temp_crit,temp_min = get_point_data(dir,"temp4_label","temp4_input","temp4_max","temp4_crit")
    msg = "        %s      : %s C  (high = %s C, crit = %s C)" % (temp_label,temp_input,temp_max,temp_crit)
    print(msg)
    temp_label,temp_input,temp_max,temp_crit,temp_min = get_point_data(dir,"temp5_label","temp5_input","temp5_max","temp5_crit")
    msg = "        %s      : %s C  (high = %s C, crit = %s C)" % (temp_label,temp_input,temp_max,temp_crit)
    print(msg)

def print_temp_data(dir, num):
    temp_input = float(get_file_path(dir, "temp_input").strip()) * temp_ratio
    temp_min   = float(get_file_path(dir, "temp_min").strip()) * temp_ratio
    temp_max   = float(get_file_path(dir, "temp_max").strip()) * temp_ratio
    temp_label = get_file_path(dir, "temp_alias").strip()
    msg = "        hotspot%d    : %.1f C  (low = %.1f C, high = %.1f C)" % (num, temp_input, temp_min, temp_max)
    print(msg)

def print_fan_data(fan_number):
    fan_dir = FAN_DIR %(fan_number)
    fan_status = get_file_path(fan_dir, "status")
    fan_status_int = int(fan_status)

    msg = ""

    if fan_status_int  == 0:
        msg = "    fan%d : ABSENT" %(fan_number)
    elif fan_status_int ==2:
        msg = "    fan%d : NOT OK" %(fan_number)
    elif fan_status_int == 1:
        fan_product_name = get_file_path(fan_dir, "product_name")
        fan_sn = get_file_path(fan_dir,"sn").strip()
        fan_hw_ver = get_file_path(fan_dir,"hw_version").strip()
        fan_speed0 = get_file_path(fan_dir,"motor0/motor_speed")
        fan_speed1 = get_file_path(fan_dir,"motor1/motor_speed")
        msg += "    fan%d:\n" %(fan_number)
        msg += "        fan_type    : %s\n" % (fan_product_name).strip()
        msg += "        sn          : %s\n" % (fan_sn)
        msg += "        hw_version  : %s\n" % (fan_hw_ver)
        msg += "        Speed       :\n"
        msg += "            speed_front : %s RPM\n" % (fan_speed0)
        msg += "            speed_rear  : %s RPM\n" % (fan_speed1)
        msg += "        status      : OK"
    else:
        msg += '        fan%d : NOT SUPPORT'

    print(msg)

def print_psu_data(psu_number):
    psu_dir = PSU_DIR %(psu_number)
    psu_status = get_file_path(psu_dir, "status")
    psu_status_int = int(psu_status)
    msg = ""

    if psu_status_int  == 0:
        msg = '    psu%d : ABSENT' %(psu_number)
    elif psu_status_int ==2:
        msg = '    psu%d : NOT OK' %(psu_number)
    elif psu_status_int == 1:

        psu_product_name = get_file_path(psu_dir, "product_name")
        psu_sn = get_file_path(psu_dir, "sn")
        psu_in_current = get_file_path(psu_dir, "in_curr")
        psu_in_vol = get_file_path(psu_dir, "in_vol").strip()
        psu_out_current = get_file_path(psu_dir, "out_curr").strip()
        psu_out_vol = get_file_path(psu_dir, "out_vol").strip()
        psu_in_type = get_file_path(psu_dir,"in_vol_type").strip()
        
        psu_temp = get_file_path(psu_dir, "temp_input").strip()
        psu_fan_speed = get_file_path(psu_dir, "fan").strip()
        psu_in_power = get_file_path(psu_dir, "in_power").strip()
        psu_out_power = get_file_path(psu_dir, "out_power").strip()

        msg += "    psu%d:\n" %(psu_number)
        msg += "        type        : %s\n"   %(psu_product_name).strip()
        msg += "        sn          : %s\n"   %(psu_sn).strip()
        msg += "        in_current  : %s A\n" %(psu_in_current).strip()
        msg += "        in_voltage  : %s V\n" %(psu_in_vol)
        msg += "        out_current : %s A\n" %(psu_out_current)
        msg += "        out_voltage : %s V\n" %(psu_out_vol)
        msg += "        input_type  : %s \n" %(psu_in_type) 
        msg += "        temp        : %s C\n" %(psu_temp)
        msg += "        fan_speed   : %d RPM\n" % (math.floor(float(psu_fan_speed)))
        msg += "        in_power    : %s W\n" %(psu_in_power)
        msg += "        out_power   : %s W" % (psu_out_power)
    else:
        msg = '    psu%d : NOT SUPPORT' %(psu_number)
        
    print msg

        
def print_slot_data(slot_num):
    slot_dir = SLOT_DIR %(slot_num)
    slot_status = get_file_path(slot_dir, "status")
    slot_status_int = int(slot_status)

    if (slot_num == 5):
        slot_status_int = 2

    msg = ""
    if slot_status_int  == 0:
        msg = '    slot%d : ABSENT' %(slot_num)
    elif slot_status_int ==2:
        msg = '    slot%d : NOT OK' %(slot_num)
    elif slot_status_int == 1:

        slot_product_name = get_file_path(slot_dir, "product_name")
        slot_sn = get_file_path(slot_dir, "sn")
        slot_hw_ver = get_file_path(slot_dir, "hw_version").strip()

        msg += "    slot%d:\n" %(slot_num)
        msg += "        slot_type   : %s\n" % (slot_product_name).strip()
        msg += "        sn          : %s\n" % (slot_sn).strip()
        msg += "        hw_version  : %s\n" % (chr(int(slot_hw_ver) + 65))
        msg += "        status      : PRESENT\n"
        
        temp_sensor_num = int(get_file_path(slot_dir,"num_temp_sensors"))

        for i in range(1, temp_sensor_num + 1):
            temp_input = get_file_path(slot_dir, "temp%d/temp_input" %(i))
            msg += "        hotspot%d    : %s C%s" % (i, temp_input.strip(), "\n" if i < temp_sensor_num else "")
        
    else:
        msg += '    slot%d : UNKNOWN' %(slot_num)
     
    print msg



def get_temp_spot_num():
    return int(get_file_path(SENSOR_DIR, "num_temp_sensors"))


print("Onboard coretemp Sensors:")
HWMON1_DIR=find_all_hwmon_paths("coretemp")[0]
get_coretemp_data(HWMON1_DIR)
print('\n')
print("Onboard Temperature Sensors:")
temp_spot_num = get_temp_spot_num()
for i in range(1, temp_spot_num + 1):
    print_temp_data(SENSOR_DIR + "temp%d/" %(i), i)


print('\n')
print("Onboard fan Sensors:")

for i in range(0, FAN_NUM):
    print_fan_data(i + 1)

print('\n')
print("Onboard Power Supply Unit Sensors:")

for i in range(0, PSU_NUM):
    print_psu_data(i + 1)

print('\n')
print("Onboard slot Sensors:")

for i in range(0, SLOT_NUM):
    print_slot_data(i + 1)

print('\n')
     


