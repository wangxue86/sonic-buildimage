#!/usr/bin/python

try:
    import os
    import sys, getopt
    import subprocess
    import imp
    import logging
    import logging.config
    import types
    import time  # this is only being used as part of the example
    import traceback
    import signal
    import re
    import datetime
    import syslog
except ImportError as e:
    raise ImportError('%s - required module not found' % str(e))

MONITOR_INTERVAL_SEC = 1
FUNCTION_NAME ="/var/log/syslog"
FAN1_DIR = "/sys/switch/fan/fan1/"
FAN2_DIR = "/sys/switch/fan/fan2/"
FAN3_DIR = "/sys/switch/fan/fan3/"
FAN4_DIR = "/sys/switch/fan/fan4/"
FAN5_DIR = "/sys/switch/fan/fan5/"
MOTOR_DIR = "/sys/switch/%s/motor%d/"

FAN_LED_FRONT_DIR = "/sys/switch/sysled/fan_led_status_front"
FAN_LED_REAR_DIR = "/sys/switch/sysled/fan_led_status_rear"

PSU_DIR = "/sys/switch/psu/"
PSU1_DIR = "/sys/switch/psu/psu1/"
PSU2_DIR = "/sys/switch/psu/psu2/"
PSU_LED_FRONT_DIR = "/sys/switch/sysled/psu_led_status_front"
PSU_LED_REAR_DIR = "/sys/switch/sysled/psu_led_status_rear"
PSU_AC_IN_VOL_LOW = 200
PSU_AC_IN_VOL_HI  = 240
PSU_DC_IN_VOL_LOW_650 = 220
PSU_DC_IN_VOL_Hi_650  = 288
PSU_DC_IN_VOL_LOW_1600 = 250
PSU_DC_IN_VOL_HI_1600 = 320

SYS_LED_FRONT_DIR = "/sys/switch/sysled/sys_led_status_front"
SYS_LED_REAR_DIR = "/sys/switch/sysled/sys_led_status_rear"

LED_COLOR_RED =    3
LED_COLOR_YELLOW = 2
LED_COLOR_GREEN =  1
LED_COLOR_DARK =   0
MAX6696_SPOT_DIR = "/sys/switch/sensor/temp%d/"
CPLD_DIR = "/sys/switch/debug/cpld/buffer"

ComE_DIR = []

ComE_DIR_NUM = 1


FAN_ADJ_DIR = "/sys/switch/debug/fan/fan_speed_pwm"

I2C_WDT_REG_DIR     = "/sys/switch/cpld/cpld0/reg_i2c_wdt_ctrl"
CPU_INIT_OK_REG_DIR = "/sys/switch/cpld/cpld0/reg_cpu_init_ok"
I2C_WDT_FEED_DIR    = "/sys/switch/cpld/cpld0/reg_i2c_wdt_feed"
I2C_WDT_ENABLE_CODE = 0x5
CPU_INIT_OK_CODE    = 0x80
ABNORMAL_LOG_TIME = 60
DMESG_NEW_LOG_INTERVAL = 3600

STATUS_ABSENT = 0
STATUS_NORMAL =1
STATUS_FAULT = 2
STATUS_UNKNOWN = 3

TEMPER_NORMAL = 0
TEMPER_ABNORMAL =1

CURVE_PWM_MAX=194

HIS_REBOOT_CAUSE_FILE = "/host/misc/history-reboot-cause.txt"
HW_REBOOT_REASON_FILE = "/sys/switch/cpld/cpld0/hw_reboot"
PRE_REBOOT_CAUSE_FILE = "/var/cache/sonic/previous-reboot-cause.txt"
PLT_REBOOT_CAUSE_FILE = "/var/cache/sonic/reboot-cause.txt"

fan_adj_list = [
    {'type': 'max6696' ,'index': 0, 'spot': 0 , 'Th': 55, 'Tl': 29, 'Nl': 0x27, 'Nh': 0xc2, 'k': 5.96,  'Twarn': 68, 'TLwarn':0, 'Tshut': None},
    {'type': 'max6696' ,'index': 0, 'spot': 1 , 'Th': 68, 'Tl': 55, 'Nl': 0x27, 'Nh': 0xc2, 'k': 11.92,  'Twarn': 75, 'TLwarn':0, 'Tshut': None} ,
    {'type': 'max6696' ,'index': 0, 'spot': 2 , 'Th': 85, 'Tl': 58, 'Nl': 0x27, 'Nh': 0xc2, 'k': 5.74,  'Twarn': 88, 'TLwarn':0, 'Tshut': 107},
    {'type': 'ASIC_inner' ,'index': 0, 'spot': 0 , 'Th': 85, 'Tl': 58, 'Nl': 0x27, 'Nh': 0xc2, 'k': 5.74,  'Twarn': 88, 'TLwarn':0, 'Tshut': 107},
    {'type': 'ssd'     ,'index': 0, 'spot': 0 , 'Th': 67, 'Tl': 52, 'Nl': 0x27, 'Nh': 0xc2, 'k': 10.33, 'Twarn': 70, 'TLwarn':0, 'Tshut': None},
    {'type': 'coretemp','index': 0, 'spot': 0 , 'Th': 90, 'Tl': 65, 'Nl': 0x27, 'Nh': 0xc2, 'k': 6.20,  'Twarn': 95, 'TLwarn':0, 'Tshut': None}
]

DEFAULT_TEMPERATUR_FOR_SENSOR_FAULT = 80
FAN_NUM = 5
PSU_NUM = 2


SLOT_LED_STATUS_MAP = { 
STATUS_UNKNOWN : LED_COLOR_DARK,
STATUS_ABSENT  : LED_COLOR_DARK,
STATUS_NORMAL  : LED_COLOR_GREEN,
STATUS_FAULT   : LED_COLOR_RED,
}

PORT_LED_ENABLE_DIR = "/sys/switch/cpld/cpld0/reg_mac_init_ok"
MAC_INIT_OK_FLAG_DIR = "/var/run/docker-syncd/system_run"

KERNEL_LOG_FILE = "/var/log/h3c_log.txt"
LOG_FILE_SIZE = 10 * 1024 * 1024
def process_reboot_cause():
    hw_reboot_cause = read_file(HW_REBOOT_REASON_FILE)

    reg = "RESET_TYPE_\w+=1\s"
    all_reason = re.findall(reg, hw_reboot_cause)
    reboot_reason_string = ""
    if all_reason == []:
    #previous is not hardware reboot
   
        log_notice("Not hardware reboot")
    else:
        for match in all_reason:
            if "RESET_TYPE_COLD" in match:
                reboot_reason_string += "Cold reboot, %s\n" % (str(datetime.datetime.now()))
            if "RESET_TYPE_WDT" in match:
                reboot_reason_string += "CPLD watchdog reboot, %s\n" % (str(datetime.datetime.now()))
            if "RESET_TYPE_BOOT_SW" in match:
                reboot_reason_string += "BIOS switchover reboot, %s\n" % (str(datetime.datetime.now()))
        if reboot_reason_string != "":
            f = open(HIS_REBOOT_CAUSE_FILE, "a")
            f.write(reboot_reason_string)
            f.close()
            f = open(PRE_REBOOT_CAUSE_FILE, "w")
            f.write(reboot_reason_string)
            f.close()
            log_notice("Hardware reboot reason %s" %(reboot_reason_string))
        else:
            log_notice("Not known hardware reboot %s" %(str(all_reason)))
            pass

def get_board_cpld_reg_value(offset_addr):

    cpld_value = ""
    cpld_file = open(CPLD_DIR, "w")
    cpld_file.write("brd_read_0x%x" %(offset_addr))
    cpld_file.close()
    cpld_file = open(CPLD_DIR, "r")
    cpld_value = cpld_file.read()
    cpld_file.close()             
    return ord(cpld_value)

def get_mac_temp():
    global last_temp

    try:
        t1 = get_board_cpld_reg_value(0x64)
        t2 = get_board_cpld_reg_value(0x65)
    except BaseException as err:
        return last_temp
                                        
    temp_code = t1 | (t2 << 8)
    temp_val = 434.1 - ((12500000 / float(temp_code) - 1) * 0.5350) 

    if temp_val > 130:
        temp_val = last_temp
    else:
        last_temp = temp_val

    return temp_val


def get_file_path_temp(main_dir,sub_dir):
    dir = os.path.join(main_dir, sub_dir)
    temp_read = open(dir, "r")
    temp_value = temp_read.read()
    temp_read.close()
    return temp_value

def get_file_path_status(main_dir,sub_dir):
    dir = os.path.join(main_dir, sub_dir)
    status_read = open(dir, "r")
    fan_status = status_read.read()
    status_read.close()
    return fan_status

def read_file(path):
    f = open(path, "r")
    value = f.read()
    f.close()
    return value

global log_file
global log_level
g_fan1_status = [STATUS_UNKNOWN, STATUS_UNKNOWN, STATUS_UNKNOWN]
g_fan2_status = [STATUS_UNKNOWN, STATUS_UNKNOWN, STATUS_UNKNOWN]
g_fan3_status = [STATUS_UNKNOWN, STATUS_UNKNOWN, STATUS_UNKNOWN]
g_fan4_status = [STATUS_UNKNOWN, STATUS_UNKNOWN, STATUS_UNKNOWN]
g_fan5_status = [STATUS_UNKNOWN, STATUS_UNKNOWN, STATUS_UNKNOWN]
g_psu1_status = STATUS_UNKNOWN
g_psu2_status = STATUS_UNKNOWN

time_cnt = 0
temp_ratio = 0.001
run_time_seconds = 0


def log_notice(log_str):
    syslog.syslog(syslog.LOG_NOTICE, log_str)



# Make a class we can use to capture stdout and sterr in the log
class TD3_hw_monitor(object):

    def __init__(self, log_file, log_level):
        global MAX6696_DIR
        global ComE_DIR

        logging.basicConfig(
            filename=log_file,
            filemode='a',
            level=log_level,
            format='%(asctime)s -%(levelname)s:%(message)s',
            datefmt='%b   %d  %H:%M:%S %Y'
        )
        for i in range(0, 5):
            #MAX6696_DIR = self.find_all_hwmon_paths("Max6696")
            ComE_DIR = self.find_all_hwmon_paths("coretemp")
            drv_installed = os.path.exists("/sys/switch/xcvr")
            if len(ComE_DIR) != ComE_DIR_NUM or not drv_installed:
                #log_notice(str(MAX6696_DIR))
                log_notice(str(ComE_DIR))
                time.sleep(2)
            else:
                break
        
        self.over_temp_reboot = 0
        self.temper_status = TEMPER_NORMAL
        self.mac_init_ok_set = 0
        #record last reboot reason in file
        process_reboot_cause()

    def record_reboot_cause(self, cause_str):
        f = open(HIS_REBOOT_CAUSE_FILE, "a")
        f.write(cause_str)
        f.close()
        
        f = open(PRE_REBOOT_CAUSE_FILE, "w")
        f.write(cause_str)
        f.close()

        f = open(PLT_REBOOT_CAUSE_FILE, "w")
        f.write(cause_str)
        f.close()


    def find_all_hwmon_paths(self, name):
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


    def manage_psu_monitorin(self,dir,g_status,class_device,device):
        status = get_file_path_status(dir,"status")
        status_int =int(status)
 
        if status_int ==STATUS_ABSENT:
            if g_status == STATUS_UNKNOWN:
                log_notice('%%PMON-5-%s_ABSENT: %s is ABSENT.' %(str(class_device).upper(),device))
            elif g_status == STATUS_NORMAL:
                log_notice('%%PMON-5-%s_PLUG_OUT: %s is ABSENT.' %(str(class_device).upper(),device))
            elif g_status == STATUS_FAULT:
                log_notice('%%PMON-5-%s_PLUG_OUT: %s is ABSENT.' %(str(class_device).upper(),device))
            g_status =STATUS_ABSENT
            if time_cnt == ABNORMAL_LOG_TIME:
                log_notice('%%PMON-5-%s_ABSENT: %s is ABSENT.' %(str(class_device).upper(),device))
        elif status_int ==STATUS_NORMAL:
            if g_status == STATUS_UNKNOWN:
                log_notice('%%PMON-5-%s_OK: %s is OK.' %(str(class_device).upper(),device))
            elif g_status == STATUS_ABSENT:
                log_notice('%%PMON-5-%s_PLUG_IN: %s is PRESENT.' %(str(class_device).upper(),device))
                log_notice('%%PMON-5-%s_OK: %s is OK.' %(str(class_device).upper(),device))
            elif g_status == STATUS_FAULT:
                log_notice('%%PMON-5-%s_OK: %s is OK. Recover from %s FAILED.' %(str(class_device).upper(),device,str(class_device).upper()))
            g_status =STATUS_NORMAL
        elif status_int ==STATUS_FAULT:
            if g_status == STATUS_UNKNOWN:
                log_notice('%%PMON-5-%s_FAILED: %s is NOT OK.' %(str(class_device).upper(),device))
            elif g_status == STATUS_ABSENT:
                log_notice('%%PMON-5-%s_PLUG_IN: %s is PRESENT.' %(str(class_device).upper(),device))
                log_notice('%%PMON-5-%s_FAILED: %s is NOT OK.' %(str(class_device).upper(),device))
            elif g_status == STATUS_NORMAL:
                log_notice('%%PMON-5-%s_FAILED: %s is NOT OK.' %(str(class_device).upper(),device))
            g_status =STATUS_FAULT
            if time_cnt ==ABNORMAL_LOG_TIME:
                log_notice('%%PMON-5-%s_FAILED: %s is NOT OK.' %(str(class_device).upper(),device))
        return g_status


    def get_motor_status(self, fan_dir, motor_index):
        path = os.path.join(fan_dir, "motor%d" %motor_index)
        status = int(get_file_path_status(path, "motor_status"))
        return status

    def manage_fans_monitorin(self,dir,g_status_group,class_device,device):
        
        status = get_file_path_status(dir,"status")
        status_int = int(status)
        g_status = g_status_group[0]
        
        motor0_status_old = g_status_group[1]
        motor1_status_old = g_status_group[2]

        motor0_status = status if status in [STATUS_NORMAL, STATUS_ABSENT] else self.get_motor_status(dir, 0)
        motor1_status = status if status in [STATUS_NORMAL, STATUS_ABSENT] else self.get_motor_status(dir, 1) 

        if status_int ==STATUS_ABSENT:
            if g_status == STATUS_UNKNOWN:

                log_notice('%%PMON-5-%s_ABSENT: %s is ABSENT.' %(str(class_device).upper(),device))
            elif g_status == STATUS_NORMAL:
                log_notice('%%PMON-5-%s_PLUG_OUT: %s is ABSENT.' %(str(class_device).upper(),device))
            elif g_status == STATUS_FAULT:
                log_notice('%%PMON-5-%s_PLUG_OUT: %s is ABSENT.' %(str(class_device).upper(),device))
            g_status =STATUS_ABSENT
            
            if time_cnt == ABNORMAL_LOG_TIME:
                log_notice('%%PMON-5-%s_ABSENT: %s is ABSENT.' %(str(class_device).upper(),device))
        elif status_int ==STATUS_NORMAL:
            if g_status == STATUS_UNKNOWN:
                pass
            elif g_status == STATUS_ABSENT:
                log_notice('%%PMON-5-%s_PLUG_IN: %s is PRESENT.' %(str(class_device).upper(),device))
            
            elif g_status == STATUS_FAULT:
                pass
            g_status =STATUS_NORMAL
        elif status_int == STATUS_FAULT:
            if g_status == STATUS_UNKNOWN:
                pass
            elif g_status == STATUS_ABSENT:
                log_notice('%%PMON-5-%s_PLUG_IN: %s is PRESENT.' %(str(class_device).upper(),device))

            elif g_status == STATUS_NORMAL:
                pass
            g_status = STATUS_FAULT

                      
        motor_iter = [
                         {'motor_status': motor0_status, 'old_status': motor0_status_old, 'position': 'front'}, 
                         {'motor_status': motor1_status, 'old_status': motor1_status_old, 'position': 'rear'}
                     ]
        
        for m in motor_iter:

            if m['motor_status'] == STATUS_NORMAL:
                if m['old_status'] in [STATUS_UNKNOWN, STATUS_ABSENT] :
                    log_notice('%%PMON-5-%s_OK: %s module %s fan is OK.' %(str(class_device).upper(),device, m['position']))

                elif m['old_status'] == STATUS_FAULT:
                    log_notice('%%PMON-5-%s_OK: %s module %s fan is OK. Recover from %s FAILED.' %(str(class_device).upper(),device, m['position'], str(class_device).upper()))
        
            elif m['motor_status'] == STATUS_FAULT:
                if m['old_status'] in [STATUS_ABSENT, STATUS_UNKNOWN, STATUS_NORMAL]:
                    log_notice('%%PMON-5-%s_FAILED: %s module %s fan is NOT OK.' %(str(class_device).upper(),device, m['position']))

                if time_cnt ==ABNORMAL_LOG_TIME:
                    log_notice('%%PMON-5-%s_FAILED: %s module %s fan is NOT OK.' %(str(class_device).upper(),device, m['position']))


 
        g_status_group = [g_status, motor0_status, motor1_status]
        return g_status_group



    def manage_temperture_monitor(self):

        temp_status = []
        for spot_info in fan_adj_list:

            class_device = "%s.%d.%d" %(spot_info['type'], spot_info['index'], spot_info['spot'])
            temp_input   = get_spot_temp(spot_info['type'], spot_info['index'], spot_info['spot'])
            temp_max     = spot_info['Twarn']
            temp_min     = spot_info['TLwarn']
            temp_shut    = spot_info['Tshut']

            #print "%s:%s:temp_input = %d,temp_max =%d,temp_min=%d" % (class_device,temp_label,temp_input,temp_max,temp_min)
            if temp_input > temp_max:
                if time_cnt == ABNORMAL_LOG_TIME:
                    temp_status.append(TEMPER_ABNORMAL)
                    log_notice('%%PMON-5-TEMP_HIGH: %s temperature %sC is larger than max threshold %sC.' %(class_device,temp_input, temp_max))
            elif temp_input < temp_min:
                if time_cnt == ABNORMAL_LOG_TIME:
                    temp_status.append(TEMPER_ABNORMAL)
                    log_notice('%%PMON-5-TEMP_LOW: %s temperature %sC is lower than min threshold %sC.' %(class_device, temp_input, temp_min))
            else:
                temp_status.append(TEMPER_NORMAL)

            if temp_shut != None and temp_input >= temp_shut:
                import os
                log_notice('%%PMON-5-TEMP_SHUT: %s temperature %sC is too hight > %sC, will reboot device.' %(class_device, temp_input, temp_shut))
                if (self.over_temp_reboot == 0):
                    self.record_reboot_cause("OTP reboot, %s\n" %(str(datetime.datetime.now())))
                elif (self.over_temp_reboot == 1):
                    os.system("/sbin/reboot")
                self.over_temp_reboot = 1
            
            self.temp_status = TEMPER_NORMAL if TEMPER_ABNORMAL not in temp_status else TEMPER_ABNORMAL

    def manage_voltage_monitorin(self):
         
        psu_num = int(get_file_path_temp(PSU_DIR, "num_psus"))

        for i in range(0, psu_num):
            psu_path = PSU_DIR +  "psu%d/"%(i + 1)
            status = int(get_file_path_temp(psu_path, "status"))
            if status == STATUS_NORMAL:
                voltage_in = get_file_path_temp(psu_path, "in_vol")
                try:
                    voltage_in = float(voltage_in)
                    if voltage_in <= PSU_AC_IN_VOL_LOW:
                        log_notice('%%PMON-5-VOLTAGE_HIGH: %s voltage %sV is lower than min threshold %sV', "psu%d" %(i + 1, voltage_in, PSU_AC_IN_VOL_LOW))
                    elif voltage_in >= PSU_AC_IN_VOL_HI:
                        log_notice('%%PMON-5-VOLTAGE_HIGH: %s voltage %sV is larger than max threshold %sV', "psu%d"%(i + 1, voltage_in, PSU_AC_IN_VOL_HI))
                except ValueError as err:
                    pass
    
    def check_and_enable_port_led(self):

        if (self.mac_init_ok_set == 0):
            os.system("echo 1 > " + PORT_LED_ENABLE_DIR)
            self.mac_init_ok_set = 1
            log_notice("Portled enable is set")
        else:
            pass
                
def data_write(dir,data):
    temp_write = open(dir, "w")
    temp_write.write(str(data))
    temp_write.close()

def get_ssd_temp():
    cmd = 'smartctl -A /dev/sda | grep "Temperature_Celsius"' 
    p = os.popen(cmd, "r")
    s = p.read()
    p.close()
    exp = "(\d+)$"
    match = re.search(exp, s)
    temp =  int(match.group(0)) if match != None else -1
    return temp

def get_spot_temp(sensor_type, sensor_index, spot_index):
    temp = 0
    try:
        if (sensor_type == 'max6696'):
            spot = 'temp_input'
            sysfs_path = MAX6696_SPOT_DIR %(sensor_index * 3 + spot_index + 1) 
            temp = get_file_path_temp(sysfs_path, spot)
            temp = int(temp) * temp_ratio


        elif sensor_type == 'coretemp':
            spot = 'temp%d_input' %(spot_index + 1)
            sysfs_path = ComE_DIR[sensor_index]
            temp = get_file_path_temp(sysfs_path, spot)
            temp = int(temp) * temp_ratio

        
        elif sensor_type == 'ASIC_inner':

            temp = int(get_mac_temp())

        elif sensor_type == 'ssd' :
            temp = get_ssd_temp()

    except BaseException as err:
        if (time_cnt == ABNORMAL_LOG_TIME):
            log = "Failed to get temperature for %s.%d.%d, set the temperature to 85C for default!" %(sensor_type, sensor_index, spot_index);
            log_notice(log)
        temp = DEFAULT_TEMPERATUR_FOR_SENSOR_FAULT

    return temp

def fan_adjust_new(fan_nor_num):

    if fan_nor_num < FAN_NUM :
        data_write(FAN_ADJ_DIR, CURVE_PWM_MAX)
        return

    target_pwm_list = []
    for spot_info in fan_adj_list:
        temp = get_spot_temp(spot_info['type'], spot_info['index'], spot_info['spot'])
        if temp >= spot_info['Th']:
            target_pwm_list.append(spot_info['Nh'])
        elif temp <= spot_info['Tl']:
            target_pwm_list.append(spot_info['Nl'])
        else:
            target_pwm_list.append(spot_info['Nh'] - spot_info['k']*(spot_info['Th'] - temp))
        #print "%s%d.%d  temp %d pwm 0x%x " %(spot_info['type'], spot_info['index'], spot_info['spot'], temp, target_pwm_list[-1] )

    target_pwm = max(target_pwm_list)

    data_write(FAN_ADJ_DIR, int(target_pwm))


def get_fan_normalnum(fan1_status, fan2_status, fan3_status, fan4_status, fan5_status):
    fan_normalnum = 0
    if(fan1_status[0] == STATUS_NORMAL):
        fan_normalnum = fan_normalnum + 1
    if(fan2_status[0] == STATUS_NORMAL):
        fan_normalnum = fan_normalnum + 1
    if(fan3_status[0] == STATUS_NORMAL):
        fan_normalnum = fan_normalnum + 1
    if(fan4_status[0] == STATUS_NORMAL):
        fan_normalnum = fan_normalnum + 1
    if(fan5_status[0] == STATUS_NORMAL):
        fan_normalnum = fan_normalnum + 1
    return fan_normalnum
    
def get_psu_normalnum(psu1_status, psu2_status):
    psu_normalnum = 0
    if(psu1_status == STATUS_NORMAL):
        psu_normalnum = psu_normalnum + 1
    if(psu2_status == STATUS_NORMAL):
        psu_normalnum = psu_normalnum + 1
    return psu_normalnum

def handler(signum, frame):
    logging.debug('INFO:signal pause')
    sys.exit(0)

def monitor_log_file():
    if os.path.exists(KERNEL_LOG_FILE):
        statinfo = os.stat(KERNEL_LOG_FILE)  
        if statinfo.st_size >= LOG_FILE_SIZE:
            filename = "h3c_" + str(datetime.datetime.now()).replace(" ", "_") + ".tar.gz" 
            os.system("tar -Pzcf /var/log/%s %s --remove-files" %(filename, KERNEL_LOG_FILE))
            log_notice("logfile %s reached size %d M, logfile is compressed." %(KERNEL_LOG_FILE, statinfo.st_size / 1024 / 1024 ))

def monitor_dmesg():
    global run_time_seconds
    if run_time_seconds % DMESG_NEW_LOG_INTERVAL == 0:
        cmd = "echo 0 > /sys/switch/debug/i2c/recent_log"
        os.system(cmd)



def main(argv):
    log_file = '%s' % FUNCTION_NAME
    log_level = logging.INFO

    #signal.signal(signal.SIGINT, handler)
    #signal.signal(signal.SIGTERM, handler)
    global monitor
    global g_fan1_status
    global g_fan2_status
    global g_fan3_status
    global g_fan4_status
    global g_fan5_status
    global g_psu1_status
    global g_psu2_status
    global time_cnt
    global run_time_seconds
    global current_running_fan_num
    # Loop forever, doing something useful hopefully:
    #last_led_flash_bit =0
    time.sleep(5)
    monitor = TD3_hw_monitor(log_file, log_level)
    wdt_flag = 1
    data_write(I2C_WDT_REG_DIR,     I2C_WDT_ENABLE_CODE)
    data_write(CPU_INIT_OK_REG_DIR, CPU_INIT_OK_CODE)
    while True:
        
        time.sleep(MONITOR_INTERVAL_SEC)
        run_time_seconds += MONITOR_INTERVAL_SEC
        try: 

            #feed i2c watchdog for bmc
            data_write(I2C_WDT_FEED_DIR, wdt_flag % 2)
            wdt_flag += 1
            g_fan1_status =monitor.manage_fans_monitorin(FAN1_DIR,g_fan1_status,"fan","fan1")
            g_fan2_status =monitor.manage_fans_monitorin(FAN2_DIR,g_fan2_status,"fan","fan2")
            g_fan3_status =monitor.manage_fans_monitorin(FAN3_DIR,g_fan3_status,"fan","fan3")
            g_fan4_status =monitor.manage_fans_monitorin(FAN4_DIR,g_fan4_status,"fan","fan4")
            g_fan5_status =monitor.manage_fans_monitorin(FAN5_DIR,g_fan5_status,"fan","fan5")
            
            current_running_fan_num =get_fan_normalnum(g_fan1_status, g_fan2_status, g_fan3_status, g_fan4_status, g_fan5_status)
            
            fan_adjust_new(current_running_fan_num)
            monitor.manage_temperture_monitor()
            monitor.manage_voltage_monitorin()
            monitor.check_and_enable_port_led()

            g_psu1_status =monitor.manage_psu_monitorin(PSU1_DIR,g_psu1_status,"psu","psu1")
            g_psu2_status =monitor.manage_psu_monitorin(PSU2_DIR,g_psu2_status,"psu","psu2")
            # print"g_fan1_status =%d\n g_psu1_status=%d\ng_psu2_status=%d" % (g_fan1_status,g_psu1_status,g_psu2_status)
            current_running_psu_num =get_psu_normalnum(g_psu1_status, g_psu2_status)
           
            if(current_running_fan_num == FAN_NUM):
                data_write(FAN_LED_FRONT_DIR, LED_COLOR_GREEN)
            elif(current_running_fan_num == FAN_NUM - 1):
                data_write(FAN_LED_FRONT_DIR, LED_COLOR_YELLOW)
            else:
                data_write(FAN_LED_FRONT_DIR, LED_COLOR_RED)

            if(current_running_psu_num == PSU_NUM):
                data_write(PSU_LED_FRONT_DIR, LED_COLOR_GREEN)
            elif(current_running_psu_num == PSU_NUM - 1):
                data_write(PSU_LED_FRONT_DIR, LED_COLOR_YELLOW)
            else:
                data_write(PSU_LED_FRONT_DIR, LED_COLOR_RED)
                
            if(current_running_fan_num == FAN_NUM and current_running_psu_num == PSU_NUM):
                data_write(SYS_LED_FRONT_DIR, LED_COLOR_GREEN)
            elif(current_running_fan_num + current_running_psu_num == FAN_NUM + PSU_NUM - 1):
                data_write(SYS_LED_FRONT_DIR, LED_COLOR_YELLOW)   
            else:
                data_write(SYS_LED_FRONT_DIR, LED_COLOR_RED)
            	  
            if time_cnt == ABNORMAL_LOG_TIME:
                time_cnt =0
            time_cnt = time_cnt + MONITOR_INTERVAL_SEC
            #print "time_cnt =%d" % (time_cnt)

            monitor_dmesg()

        except BaseException as err:
            import traceback
            traceback.print_stack()
            print("ERROR:" + str(err))
            log_notice(str(err))


if __name__ == '__main__':
    main(sys.argv[1:])
#coding = utf-8
