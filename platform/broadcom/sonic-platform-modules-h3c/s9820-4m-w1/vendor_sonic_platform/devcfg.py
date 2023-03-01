#!/usr/bin/python
# -*- coding: UTF-8 -*-
"""
Device config file
"""
try:
    import re
    import os
    import logging
    import syslog
    import subprocess
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")


class Devcfg(object):
    """
    Device config
    """
    ##################################
    # directory
    ##################################
    MNT_DIR = "/mnt/"
    EFI_MOUNT_DIR = MNT_DIR + "update_bios/"
    EFI_DIR = EFI_MOUNT_DIR + "EFI/"
    EFI_PARTITION = "/dev/sda1"
    EFI_BOOT_DIR = EFI_DIR + "BOOT/"

    CLASS_DIR = "/sys/class/"
    SWITCH_DIR = "/sys/switch/"

    CPLD_DIR = SWITCH_DIR + "cpld/"
    DEBUG_DIR = SWITCH_DIR + "debug/"
    EEPROM_DIR = SWITCH_DIR + "syseeprom/"
    FAN_DIR = SWITCH_DIR + "fan/"
    SENSOR_DIR = SWITCH_DIR + "sensor/"
    SFP_DIR = SWITCH_DIR + "xcvr/"
    SLOT_DIR = SWITCH_DIR + "slot/"
    PSU_DIR = SWITCH_DIR + "psu/"
    SYSLED_DIR = SWITCH_DIR + "sysled/"

    HWMON_DIR = CLASS_DIR + "hwmon/"

    DEBUG_CPLD_DIR = DEBUG_DIR + "cpld/"
    DEBUG_FAN_DIR = DEBUG_DIR + "fan/"
    DEBUG_SENSOR_DIR = DEBUG_DIR + "sensor/"

    EEPROM_DATA_DIR = EEPROM_DIR + "raw_data"

    PSU_SUB_PATH = PSU_DIR + "psu{}/"
    FAN_SUB_PATH = FAN_DIR + "fan{}/"

    SFP_BASH_PATH = SFP_DIR + 'Eth100GE{0}-{1}/'

    SPEED_PWM_FILE = DEBUG_FAN_DIR + "fan_speed_pwm"
    SECOND_VOL_MAIN_PATH = DEBUG_SENSOR_DIR + "adm1166/"

    VMECPU_DIR = "/usr/local/bin/vmecpu"
    VME_DIR = "/usr/local/bin/vme"

    BIOS_VERSION_PATTERN = r"BIOS Version *([^\n]+)"

    ##################################
    # parameter value
    ##################################
    PORT_START = 1
    PORT_END = 128

    PORT_NUM_PER_SLOT = 32

    QSFP_START = 1
    QSFP_END = 128

    PSU_IS_1600W = True

    # SFP_TYPE, SFP_START, SFP_END
    SFP_INFO = [
        {"type": "QSFP", "start": 1, "end": 128}
    ]

    # definitions of the offset and width for values in DOM info eeprom
    QSFP_CHANNEL_RX_LOS_STATUS_OFFSET = 3
    QSFP_CHANNEL_RX_LOS_STATUS_WIDTH = 1
    QSFP_CHANNEL_TX_FAULT_STATUS_OFFSET = 4
    QSFP_CHANNEL_TX_FAULT_STATUS_WIDTH = 1
    QSFP_CHANNEL_TX_DISABLE_OFFSET = 86
    QSFP_CHANNEL_TX_DISABLE_WIDTH = 1

    CHASSIS_COMPONENTS = [
        ["BIOS", ("Performs initialization of hardware components during booting")],
        ["CPU-CPLD", "Used for managing CPU board devices and power"],
        ["MAIN_BOARD-CPLD-1", ("Used for managing Fan, PSU, system LEDs, QSFP modules ")],
        ["MAIN_BOARD-CPLD-2", ("Used for managing Fan, PSU, system LEDs, QSFP modules ")]
    ]
    COMPONENT_NUM = len(CHASSIS_COMPONENTS)

    MAC_SHUT = 101
    TVR_SHUT = 140
    FAN_ADJ_LIST = [
        {'type': 'max6696' , 'name': "Switch PCB left",'index': 0, 'spot': 0 , 'Th': 75, 'Tl': 55, 'Nl': 0x14, 'Nh': 0x64, 'k': 4.0, 'crit': 85, 'max': 80, 'min':0, 'Tshut': None},
        {'type': 'max6696' , 'name': "MAC inner-1",    'index': 0, 'spot': 1 , 'Th': 85, 'Tl': 60, 'Nl': 0x14, 'Nh': 0x64, 'k': 3.2, 'crit': 93, 'max': 88, 'min':0, 'Tshut': 107 },
        {'type': 'max6696' , 'name': "MAC inner-2",    'index': 0, 'spot': 2 , 'Th': 85, 'Tl': 60, 'Nl': 0x14, 'Nh': 0x64, 'k': 3.2, 'crit': 93, 'max': 88, 'min':0, 'Tshut': 107 },
        {'type': 'max6696' , 'name': "Switch PCB right",'index': 1, 'spot': 0 , 'Th': 75, 'Tl': 55, 'Nl': 0x14, 'Nh': 0x64, 'k': 4.0, 'crit': 85, 'max': 80, 'min':0, 'Tshut': None},
        {'type': 'max6696' , 'name': "Switch PCB down",'index': 1, 'spot': 1 , 'Th': 80, 'Tl': 60, 'Nl': 0x14, 'Nh': 0x64, 'k': 4.0, 'crit': 90, 'max': 85, 'min':0, 'Tshut': None},
        {'type': 'max6696' , 'name': "Switch PCB up",  'index': 1, 'spot': 2 , 'Th': 75, 'Tl': 55, 'Nl': 0x14, 'Nh': 0x64, 'k': 4.0, 'crit': 85, 'max': 80, 'min':0, 'Tshut': None},
        {'type': 'i350'    , 'name': "I350",           'index': 0, 'spot': 0 , 'Th': 80, 'Tl': 60, 'Nl': 0x14, 'Nh': 0x64, 'k': 4.0, 'crit': 90, 'max': 85, 'min':0, 'Tshut': None},
        {'type': 'coretemp', 'name': "CPU core",      'index': 0, 'spot': 0 , 'Th': 85, 'Tl': 60, 'Nl': 0x14, 'Nh': 0x64, 'k': 3.2, 'crit': 93, 'max': 88, 'min':0, 'Tshut': None}
    ]

    SECOND_VOL_LIST = [
        {'type': 'vh_12v', 'UpLimit': 13200, 'LowLimit': 10800, 'his_flag': 1},
    ]

    DEFAULT_MOTOR0_MAX_SPEED = 11000
    DEFAULT_MOTOR1_MAX_SPEED = 12000
    DEFAULT_PSUFAN_MOTOR_MAX_SPEED = 30000
    PWM_REG_MAX = 100
    PWM_REG_MIN = 20
    SPEED_TARGET_MAX = 100
    SPEED_TARGET_MIN = 20
    FAN_MOTOR_MAX_SPEED_LIST = [
        {'type': 'DELTA', 'motor0': 11000, 'motor1': 12500, 'tolerance': 20},
        {'type': 'FOXCONN', 'motor0': 11200, 'motor1': 12800, 'tolerance': 20},
        {'type': 'DEFAULT', 'motor0': 11000, 'motor1': 12000, 'tolerance': 20},
    ]
    PSUFAN_MOTOR_MAX_SPEED_LIST = [
        {'type': 'DELTA', 'motor': 32500, 'tolerance': 10},
        {'type': 'FSP', 'motor': 30000, 'tolerance': 10},
        {'type': 'DEFAULT', 'motor': 30000, 'tolerance': 10},
    ]
    CPU_THERMAL_IDX_START = 2
    CPU_THERMAL_NUM = 4
    THERMAL_INFO = [
        {'hwmon_temp_index': 1, 'hwmon_alias': 'coretemp', 'name': "CPU Core", 'type': 'cpu'},
        {'hwmon_temp_index': 1, 'hwmon_alias': 'Max6696_0', 'name': "Switch PCB left", 'type': 'max6696'},
        {'hwmon_temp_index': 2, 'hwmon_alias': 'Max6696_0', 'name': "MAC inner-1", 'type': 'max6696'},
        {'hwmon_temp_index': 3, 'hwmon_alias': 'Max6696_0', 'name': "MAC inner-2", 'type': 'max6696'},
        {'hwmon_temp_index': 1, 'hwmon_alias': 'Max6696_1', 'name': "Switch PCB right", 'type': 'max6696'},
        {'hwmon_temp_index': 2, 'hwmon_alias': 'Max6696_1', 'name': "Switch PCB down", 'type': 'max6696'},
        {'hwmon_temp_index': 3, 'hwmon_alias': 'Max6696_1', 'name': "Switch PCB up", 'type': 'max6696'}
    ]

    THERMAL_NUM = len(THERMAL_INFO)

    DEFAULT_TEMPERATUR_FOR_SENSOR_FAULT = 80
    DEFAULT_TEMPERATUR_FOR_OPTIC_FAULT = 52

    FAN_DRAWER_NUM = 1
    FAN_NUM = 6
    PSU_NUM = 4

    STATUS_ABSENT = 0
    STATUS_OK = 1
    STATUS_NOT_OK = 2

    # presence change delay 30s, update status
    PRESENCE_CHANGE_DELAY = 30

    BMC_ENABLED = False
    ###hw-mon##

    PSU_MONITOR_ENABLE = False

    FAN_LED_DIR = SYSLED_DIR + "fan_led_status_front"
    PSU_LED_DIR = SYSLED_DIR + "psu_led_status_front"
    SYS_LED_DIR = SYSLED_DIR + "sys_led_status_front"

    FAN_ADJ_DIR = DEBUG_FAN_DIR + "fan_speed_pwm"

    CPU_INIT_OK_REG_DIR = CPLD_DIR + "cpld2/reg_cpu_init_ok"
    MAC_INIT_OK_REG_DIR = CPLD_DIR + "cpld2/reg_mac_init_ok"
    MAC_INNER_TEMP_REG_DIR = SENSOR_DIR + "temp4/temp_input"
    MAC_INNER_TEMP_FORMULA_COEF = 0.23755
    CPU_INIT_OK_CODE = 1
    MAC_INIT_OK_CODE = 1

    LED_COLOR_CODE_RED = 3
    LED_COLOR_CODE_YELLOW = 2
    LED_COLOR_CODE_GREEN = 1
    LED_COLOR_CODE_DARK = 0

    COME_DIR_NUM = 1
    MONITOR_INTERVAL_SEC = 1
    MONITOR_INTERVAL_COUNT = 5
    ABNORMAL_LOG_TIME = 60
    DMESG_NEW_LOG_INTERVAL = 3600

    CURVE_PWM_MAX = 100
    TEMP_RATIO = 0.001

    """
    def __init__(self):
        '''
        Nothing to do
        '''
    def get1(self):
        return 1

    def get2(self):
        return 2
    """
