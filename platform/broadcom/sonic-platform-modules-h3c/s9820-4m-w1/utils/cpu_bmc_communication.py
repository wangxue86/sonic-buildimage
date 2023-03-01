#!/usr/bin/python
#coding=utf-8
try:
    import os
    import sys, getopt
    import subprocess
    import click
    import imp
    import logging
    import logging.config
    import types
    import time  # this is only being used as part of the example
    import traceback
    import signal
    import re
    
except ImportError as e:
    raise ImportError('%s - required module not found' % str(e))
    
    
OPERATE_INTERVAL_SEC = 10

ABNORMAL_LOG_TIME = 60

PSU_DATA_LIST = []
TEMP_DATA_LIST = []

psu_count = 0

def get_max6696_temp():
    temp_value = 0
    r = os.popen("cat /sys/switch/thermal/temp")
    info = r.readlines()
    pattern = "hotspot1"
    for line in info:
        matchobj = re.search(pattern, line)
        if matchobj:
            temp_value = float(line.replace(" ","").strip('hotspot1').strip(':').replace("Centigrade",""))
            break
    return temp_value

def get_lm75_temp():
    temp_value = 0
    if(os.path.exists('/sys/switch/linecard/1/temp')):
        r = os.popen("cat /sys/switch/linecard/1/temp")
        info = r.readlines()
        pattern = "hotspot-1"
        for line in info:
            matchobj = re.search(pattern, line)
            if matchobj:
                temp_value = float(line.replace(" ","").strip('hotspot-1').strip(':').replace("Centigrade",""))
                break
    return temp_value

def get_psu_data(psu_num):
    global PSU_DATA_LIST
    IIN_value = 0
    VIN_value = 0
    PIN_value = 0
    PSTA_value = 0
    r = os.popen("cat /sys/switch/psu/%d/data" % psu_num)
    info = r.readlines()
    #print "spot_index = %s " % spot_index
    pattern_IIN = "IIN"
    pattern_VIN = "VIN"
    pattern_PIN = "PIN"
    pattern_PSTA = "PSTA"
    PSU_DATA_LIST = []
    for line in info:
        matchobjIIN = re.search(pattern_IIN, line)
        if matchobjIIN:
            IIN_value = float(line.replace(" ","").strip('IIN').strip(':').replace("A",""))
            IIN_value = IIN_value*100
           # print IIN_value
            continue
        matchobjVIN  = re.search(pattern_VIN, line)   
        if matchobjVIN:
            VIN_value = float(line.replace(" ","").strip('VIN').strip(':').replace("V",""))
            continue
        matchobjPIN  = re.search(pattern_PIN, line)   
        if matchobjPIN:
            PIN_value = float(line.replace(" ","").strip('PIN').strip(':').replace("W",""))
            continue
        matchobjPSTA  = re.search(pattern_PSTA, line)   
        if matchobjPSTA:
            PSTA_value = float(line.replace(" ","").strip('PSTA').strip(':').replace("ORG",""))
            continue
    PSU_DATA_LIST.append(IIN_value)
    PSU_DATA_LIST.append(VIN_value)
    PSU_DATA_LIST.append(PIN_value)
    PSU_DATA_LIST.append(PSTA_value)

    
def get_cpu_temp():
    global TEMP_DATA_LIST
    TEMP_DATA_LIST = []
    VrTemp1_value = 0
    VrTemp2_value = 0
    MwmVol_value = 0
    Core_Vol_value = 0
    v05_value = 0
    if(os.path.exists("/sys/class/h3c/cpu_core")):
        r = os.popen("cat /sys/class/h3c/cpu_core")
        info = r.readlines()
        #print "spot_index = %s " % spot_index
        pattern_VrTemp1 = "VrTemperature1"
        pattern_VrTemp2 = "VrTemperature2"
        pattern_MemVol = "MemVol"
        pattern_CoreVol = "CoreVol"
        pattern_1v05 = "1v05"
        for line in info:
            matchobjVrTemp1 = re.search(pattern_VrTemp1, line)
            if matchobjVrTemp1:
                VrTemp1_value = int(line.replace(" ","").strip('VrTemperature1').strip(':').replace("Centigrade",""))
                continue
            
            matchobjVrTemp2  = re.search(pattern_VrTemp2, line)   
            if matchobjVrTemp2:
                VrTemp2_value = int(line.replace(" ","").strip('VrTemperature2').strip(':').replace("Centigrade",""))
                continue
            
            matchobjMemVol  = re.search(pattern_MemVol, line)   
            if matchobjMemVol:
                MwmVol_value = int(line.replace(" ","").strip('MemVol').strip(':').replace("V",""))
                continue
            
            matchobjCore_Vol  = re.search(pattern_CoreVol, line)   
            if matchobjCore_Vol:
                Core_Vol_value = int(line.replace(" ","").strip('CoreVol').strip(':').replace("V",""))
                continue
            
            matchobj1v05  = re.search(pattern_1v05, line)   
            if matchobj1v05:
                v05_value = int(line.replace(" ","").strip('1v05').strip(':').replace("V",""))
                continue
    TEMP_DATA_LIST.append(VrTemp1_value)
    TEMP_DATA_LIST.append(VrTemp2_value)
    TEMP_DATA_LIST.append(MwmVol_value)
    TEMP_DATA_LIST.append(Core_Vol_value)
    TEMP_DATA_LIST.append(v05_value)
    

def construct_psu_in_ipmi_data(index):
    
    get_psu_data(index)
    
    if index == 1:
        high_8bit = int(PSU_DATA_LIST[2])/256
        low_8bit =  int(PSU_DATA_LIST[2])%256
        temp = '0x8' + ' ' +  '0xa' + ' ' + '0x21' +  ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
        
        high_8bit = int(PSU_DATA_LIST[1])/256
        low_8bit =  int(PSU_DATA_LIST[1])%256
        temp = temp + ' ' + '0x2' + ' ' +  '0xa' + ' ' + '0x01' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)
        
        high_8bit = int(PSU_DATA_LIST[0])/256
        low_8bit =  int(PSU_DATA_LIST[0])%256
        temp = temp + ' ' + '0x3' + ' ' +  '0xa' + ' ' + '0x01' + ' ' +  '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
        
        high_8bit = int(PSU_DATA_LIST[3])/256
        low_8bit =  int(PSU_DATA_LIST[3])%256
        temp = temp + ' ' + '0x8' + ' ' +  '0xa' + ' ' + '0x01' + ' ' + '0x00' + ' '  + bytes(low_8bit) + ' ' +  bytes(high_8bit) 
        temp = temp + ' '
        
        
    elif index == 2:
        high_8bit = int(PSU_DATA_LIST[2])/256
        low_8bit =  int(PSU_DATA_LIST[2])%256
        temp = '0x8' + ' ' +  '0xa' + ' ' + '0x22' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
        
        high_8bit = int(PSU_DATA_LIST[1])/256
        low_8bit =  int(PSU_DATA_LIST[1])%256
        temp = temp + ' ' + '0x2' + ' ' +  '0xa' + ' ' + '0x02' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' +  bytes(high_8bit)
        
        high_8bit = int(PSU_DATA_LIST[0])/256
        low_8bit =  int(PSU_DATA_LIST[0])%256
        temp = temp + ' ' + '0x3' + ' ' +  '0xa' + ' ' + '0x02' + ' ' + '0x00' + ' ' + bytes(low_8bit) +  ' ' + bytes(high_8bit) 
        
        high_8bit = int(PSU_DATA_LIST[3])/256
        low_8bit =  int(PSU_DATA_LIST[3])%256
        temp = temp + ' ' + '0x8' + ' ' +  '0xa' + ' ' + '0x02' + ' ' + '0x00' + ' ' + bytes(low_8bit) +  ' ' + bytes(high_8bit)
        temp = temp + ' '        
        
    elif index == 3:
        high_8bit = int(PSU_DATA_LIST[2])/256
        low_8bit =  int(PSU_DATA_LIST[2])%256
        temp = '0x8' + ' ' +  '0xa' + ' ' + '0x23' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)
        
        high_8bit = int(PSU_DATA_LIST[1])/256
        low_8bit =  int(PSU_DATA_LIST[1])%256
        temp = temp + ' ' + '0x2' + ' ' +  '0xa' + ' ' + '0x03' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)
        
        high_8bit = int(PSU_DATA_LIST[0])/256
        low_8bit =  int(PSU_DATA_LIST[0])%256
        temp = temp + ' ' + '0x3' + ' ' +  '0xa' + ' ' + '0x03' +  ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
        
        high_8bit = int(PSU_DATA_LIST[3])/256
        low_8bit =  int(PSU_DATA_LIST[3])%256
        temp = temp + ' ' + '0x8' + ' ' +  '0xa' + ' ' + '0x03' + ' ' +  '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
        temp = temp + ' '
        
    elif index == 4:
        high_8bit = int(PSU_DATA_LIST[2])/256
        low_8bit =  int(PSU_DATA_LIST[2])%256
        temp = '0x8' + ' ' +  '0xa' + ' ' + '0x24' + ' ' +  '0x00' + ' ' +  bytes(low_8bit) + ' ' + bytes(high_8bit)
        
        high_8bit = int(PSU_DATA_LIST[1])/256
        low_8bit =  int(PSU_DATA_LIST[1])%256
        temp = temp + ' ' + '0x2' + ' ' +  '0xa' + ' ' + '0x04' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)
        
        high_8bit = int(PSU_DATA_LIST[0])/256
        low_8bit =  int(PSU_DATA_LIST[0])%256
        temp = temp + ' ' + '0x3' + ' ' +  '0xa' + ' ' + '0x04' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
        
        high_8bit = int(PSU_DATA_LIST[3])/256
        low_8bit =  int(PSU_DATA_LIST[3])%256
        temp = temp + ' ' + '0x8' + ' ' +  '0xa' + ' ' + '0x04' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)  
        temp = temp + ' '        

   #temp = '0'
    return temp
        
def temp_convert(temp):
    temp_value = 0
    real_value = 0
    return_value = 0
    if(temp >> 15):
        temp_value = (temp & 0x7800) >> 11
        temp_value = temp_value - 1
        temp_value = ~temp_value
        temp_value = temp_value & 0x0f
        temp_value = 0 - temp_value
        real_value = temp & 0x7ff
        return_value = real_value*(2**temp_value)
        #print 'zhc'
        #print return_value
    return return_value 

def construct_cpu_ipmi_data():
    get_cpu_temp();    
    for val in TEMP_DATA_LIST:
        if int(val) == 0:
            return None  
            
    temp_val = TEMP_DATA_LIST[0]
         
    #print temp_val
    temp_val =  temp_convert(temp_val)
    #print temp_val
    high_8bit = int(temp_val)/256
    low_8bit =  int(temp_val)%256
    temp = '0x1' + ' ' +  '0x3' + ' ' + '0x31' + ' ' +  '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
        
    temp_val = TEMP_DATA_LIST[1]
    temp_val = temp_convert(temp_val)
    high_8bit = int(temp_val)/256
    low_8bit =  int(temp_val)%256
    temp = temp + ' ' + '0x1' + ' ' +  '0x3' + ' ' + '0x32' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) 
    
    temp_val = TEMP_DATA_LIST[2]
    high_8bit = int(temp_val)/256
    low_8bit =  int(temp_val)%256
    temp = temp + ' ' + '0x2' + ' ' +  '0x8' + ' ' + '0x00' +  ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)

    temp_val = TEMP_DATA_LIST[3]
    high_8bit = int(temp_val)/256
    low_8bit =  int(temp_val)%256
    temp = temp + ' ' + '0x2' + ' ' +  '0x3' + ' ' + '0x41' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)    
 
    temp_val = TEMP_DATA_LIST[4]
    high_8bit = int(temp_val)/256
    low_8bit =  int(temp_val)%256
    temp = temp + ' ' + '0x2' + ' ' +  '0x03' + ' ' + '0x51' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit)
    
    temp = temp + ' '
        
    return temp

temp_th = 1
def contruct_temp_ipmi_data():
    global temp_th
    temp_th = 1
    temp_val = get_max6696_temp()
    high_8bit = int(temp_val)/256
    low_8bit =  int(temp_val)%256
    temp = '0x1' + ' ' +  '0x07' + ' ' + '0x3c' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) + ' '
    if(os.path.exists('/sys/switch/linecard/1/temp')):
        temp_th = temp_th + 1
        temp_val = get_lm75_temp()
        high_8bit = int(temp_val)/256
        low_8bit =  int(temp_val)%256
        temp =  temp + '0x1' + ' ' +  '0x07' + ' ' + '0x32' + ' ' + '0x00' + ' ' + bytes(low_8bit) + ' ' + bytes(high_8bit) + ' '
    
    return temp
    
    
def construct_all_psu_data():
    global psu_count
    main_dir = '/sys/switch/psu/'
    psu_count = 0
    all_psu = ''
    temp = '0'
    for pwr_index in range(1,5):
        if(os.path.exists(main_dir + bytes(pwr_index) + '/')):
            temp = construct_psu_in_ipmi_data(pwr_index);
            all_psu = all_psu + temp
            psu_count = psu_count + 1
    return all_psu
    
def cpu_info_monitor():
    log_level = logging.INFO
    temp = '0'
    common_cmd = 'ipmitool' + ' ' + 'raw' + ' ' +  '0x36' + ' ' + '0x05' + ' ' + '0xA2' + ' ' + '0x63' + ' ' + '0x00' + ' '  + '0x0D'
    common_cmd_send = '0'
    try:
        if ( os.path.exists("/dev/ipmi0")):
            temp = construct_cpu_ipmi_data()
            if temp != None:##对于抖动的0值不上报
                common_cmd_send = common_cmd + ' ' +  bytes(5) + ' ' + temp
            os.popen(common_cmd_send)
    except BaseException, err:
        import traceback
        traceback.print_stack()
        print ("ERROR:" + str(err))
        logging.info(str(err))



        
            
        
    
    
    
