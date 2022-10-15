#!/usr/bin/env python

#Version: 1.0
########################################################################
#
# provides the fan status which are available in the platform
#MODULE_AUTHOR("Qianchaoyang <qian.chaoyang@h3c.com>");
#MODULE_DESCRIPTION("h3c fan management");
#
#############################################################################

import os
import time
import re

try:
    from sonic_platform_base.fan_base import FanBase
except ImportError as _e:
    raise ImportError(str(_e) + "- required module not found")

try:
    from sonic_daemon_base.daemon_base import Logger
except ImportError as e1:
    from sonic_py_common.logger import Logger
except ImportError as e2:
    raise ImportError(str(e2) + " required module not found")

STATUS_LED_COLOR_GREEN = "green"
STATUS_LED_COLOR_RED = "red"
STATUS_LED_COLOR_OFF = "off"

FAN_ADJ_LIST = [
    {'type': 'max6696', 'index': 0, 'spot': 0, 'Th': 55, 'Tl': 29, 'Nl': 0x27, 'Nh': 0xc2, 'k': 5.96},
    {'type': 'max6696', 'index': 0, 'spot': 1, 'Th': 67, 'Tl': 45, 'Nl': 0x27, 'Nh': 0xc2, 'k': 7.05},
    {'type': 'max6696', 'index': 0, 'spot': 2, 'Th': 85, 'Tl': 55, 'Nl': 0x27, 'Nh': 0xc2, 'k': 5.17},
    {'type': 'ssd', 'index': 0, 'spot': 0, 'Th': 67, 'Tl': 52, 'Nl': 0x27, 'Nh': 0xc2, 'k': 10.33},
    {'type': 'coretemp', 'index': 0, 'spot': 0, 'Th': 90, 'Tl': 65, 'Nl': 0x27, 'Nh': 0xc2, 'k': 6.20}
]

STATUS_ABSENT = 0
STATUS_NORMAL = 1
STATUS_FAULT = 2
STATUS_UNKNOWN = 3
DEFAULT_TEMPERATUR_FOR_SENSOR_FAULT = 80
FAN_NUM = 5
FAN_LIST = list(range(0, FAN_NUM))

# presence change delay 30s, update status
PRESENCE_CHANGE_DELAY = 30

SYSLOG_IDENTIFIER = 'platfom_fan'
logger = Logger(SYSLOG_IDENTIFIER)


def _write_sysfs_file(file_path, value):
    # On successful read, returns the value read from given
    try:
        with open(file_path, 'w') as temp:
            temp.write(value)
    except IOError:
        logger.log_error('open file_path %s error' % file_path, True)
        return False
    return True


def find_all_hwmon_paths(name):
    """:
    A string, find each path of hwmon
    """
    hw_list = os.listdir('/sys/class/hwmon/')
    for node in hw_list:
        hw_dir = '/sys/class/hwmon/%s/' % (node)
        try:
            with open(hw_dir + 'name', 'r') as temp:
                hw_name = temp.read()
                if name in hw_name:
                    return hw_dir
        except IOError:
            logger.log_error('open file_path %s error' % hw_dir, True)
            return False
    try:
        return hw_dir
    except Exception as e:
        print(str(e))



def get_file_path_temp(main_dir, sub_dir):
    """:
    A string, read temp of hwmon
    """
    _dir = os.path.join(main_dir, sub_dir)
    temp_value = 0
    try:
        with open(_dir, 'r') as temp_read:
            temp_value = temp_read.read()
    except IOError:
        return False
    return temp_value


class Fan(FanBase):
    """Platform-specific Fan class"""
    led_status_value = {
        STATUS_LED_COLOR_OFF: '0',
        STATUS_LED_COLOR_GREEN: '1',
        STATUS_LED_COLOR_RED: '3'
    }

    def __init__(self, fan_index, is_psu_fan=False):
        FanBase.__init__(self)

        self._index = fan_index
        self.is_psu_fan = is_psu_fan

        if self.is_psu_fan:
            self.fan_path = "/sys/switch/psu/psu{}/".format(self._index + 1)
        else:
            self.fan_path = "/sys/switch/fan/fan{}/".format(self._index + 1)

        self.curve_pwm_max = 194
        self.curve_pwm_min = 39
        self.curve_speed_min = 30
        self.curve_speed_max = 100

        self._presence_change_time = time.time()
        self._all_fan_presence_change_time = time.time()
        self._all_fan_presence_list = []
        self._old_status = self.get_status()
        self._old_presence = self.get_presence()

    def get_direction(self):
        """
        Retrieves the direction of fan
        Returns:
            A string, either FAN_DIRECTION_INTAKE or FAN_DIRECTION_EXHAUST
            depending on fan direction
        """
        if self.is_psu_fan:
            return self.FAN_DIRECTION_INTAKE

        attr_file = 'motor0/motor_direction'
        file_path = self.fan_path + attr_file
        try:
            with open(file_path, 'r') as direction_prs:
                direction = int(direction_prs.read())
        except IOError:
            return False

        if direction == 1:
            temp = self.FAN_DIRECTION_INTAKE
        elif direction == 0:
            temp = self.FAN_DIRECTION_EXHAUST
        else:
            return False
        return temp

    def get_speed(self):
        """
        Retrieves the speed of fan as a percentage of full speed
        Returns:
            An integer, the percentage of full fan speed, in the range 0 (off)
                 to 100 (full speed)
        """
        if self.is_psu_fan:
            attr_file = 'fan0/speed'
            attr_path = self.fan_path + attr_file
            try:
                with open(attr_path, 'r') as speed_psu_fan:
                    speed = float(speed_psu_fan.read().strip('\n'))
                    speed = int(speed / 18000)
                    if speed > 100:
                        speed = 100
                    return speed
            except IOError as _e:
                print(str(_e))
                return False
        else:
            speed = 0
            speed0 = 0
            speed1 = 0
            attr_path0 = self.fan_path + 'motor0/motor_ratio'
            attr_path1 = self.fan_path + 'motor1/motor_ratio'

            try:
                with open(attr_path0, 'r') as speed_prs0:
                    speed0 = int(speed_prs0.read())
            except IOError as _e:
                print(str(_e))
                return False

            try:
                with open(attr_path1, 'r') as speed_prs1:
                    speed1 = int(speed_prs1.read())
            except IOError:
                return False
            speed = int((speed0 + speed1)/2)
            if speed > 100:
                speed = 100
            return speed

    @staticmethod
    def _get_ssd_temp():
        cmd = 'smartctl -A /dev/sda | grep "Temperature_Celsius"'
        _p = os.popen(cmd, "r")
        _s = _p.read()
        _p.close()
        exp = r"(\d+)$"
        match = re.search(exp, _s)
        temp = int(match.group(0)) if match is not None else -1
        return temp

    def _get_spot_temp(self, sensor_type, sensor_index, spot_index):
        temp = 0
        temp_ratio = 0.001

        i350_dir = find_all_hwmon_paths("i350bb")
        come_dir = find_all_hwmon_paths("coretemp")
        _max6696_num = 'Max6696_%d' % sensor_index
        max6696_dir = find_all_hwmon_paths(_max6696_num)

        try:
            if sensor_type == 'max6696':
                spot = 'temp%d_input' % (spot_index + 1)
                sysfs_path = max6696_dir
                temp = get_file_path_temp(sysfs_path, spot)
                temp = int(temp) * temp_ratio

            elif sensor_type == 'coretemp':
                spot = 'temp%d_input' % (spot_index + 1)
                sysfs_path = come_dir
                temp = get_file_path_temp(sysfs_path, spot)
                temp = int(temp) * temp_ratio

            elif sensor_type == 'i350':
                spot = 'temp%d_input' % (spot_index + 1)
                sysfs_path = i350_dir
                temp = get_file_path_temp(sysfs_path, spot)
                temp = int(temp) * temp_ratio

            elif sensor_type == 'ssd':
                temp = self._get_ssd_temp()

        except BaseException as _e:
            print(str(_e))
            temp = DEFAULT_TEMPERATUR_FOR_SENSOR_FAULT

        return temp

    @staticmethod
    def _get_file_path_status(main_dir, sub_dir):
        _dir = os.path.join(main_dir, sub_dir)
        try:
            with open(_dir, "r") as status_read:
                fan_status = status_read.read()
        except IOError:
            return False

        return fan_status

    def _get_all_fan_presence(self):
        presence_list = []

        for index in FAN_LIST:
            _dir = "/sys/switch/fan/fan{}/".format(index + 1)
            status = self._get_file_path_status(_dir, "status")
            status_int = int(status)
            if status_int != STATUS_ABSENT:
                presence_list.append(index)

        if self._all_fan_presence_list != presence_list:
            self._all_fan_presence_change_time = time.time()

        return presence_list

    def _get_fan_normalnum(self):
        return len(self._get_all_fan_presence())

    def get_target_speed(self):
        """
        Retrieves the target (expected) speed of the fan
        Returns:
            An integer, the percentage of full fan speed, in the range 0 (off)
                 to 100 (full speed)
        """
        if self.is_psu_fan:
            return 'N/A'

        fan_nor_num = self._get_fan_normalnum()
        if fan_nor_num < FAN_NUM:
            target_speed = self.curve_speed_max
            return target_speed

        target_pwm_list = []
        for spot_info in FAN_ADJ_LIST:
            temp = self._get_spot_temp(spot_info['type'], spot_info['index'], spot_info['spot'])
            if temp >= spot_info['Th']:
                target_pwm_list.append(spot_info['Nh'])
            elif temp <= spot_info['Tl']:
                target_pwm_list.append(spot_info['Nl'])
            else:
                target_pwm_list.append(spot_info['Nh'] - spot_info['k']*(spot_info['Th'] - temp))

        target_pwm = max(target_pwm_list)

        target_speed = (int(target_pwm) - self.curve_pwm_min) * 80 \
                        / (self.curve_pwm_max - self.curve_pwm_min) + 20

        if target_speed < self.curve_speed_min:
            target_speed = self.curve_speed_min

        return target_speed

    def get_speed_tolerance(self):
        """
        Retrieves the speed tolerance of the fan
        Returns:
            An integer, the percentage of variance from target speed which is
                 considered tolerable
        """
        if self.is_psu_fan:
            #return 'N/A'
            return 20

        speed_tolerance = 0
        attr_path = self.fan_path +'motor0/speed_tolerance'

        try:
            with open(attr_path, 'r') as tolerance_prs:
                speed_tolerance = int(tolerance_prs.read())
        except IOError:
            return False

        return speed_tolerance+5


    def set_speed(self, speed):
        """
        Sets the fan speed
        Args:
            speed: An integer, the percentage of full fan speed to set fan to,
                   in the range 0 (off) to 100 (full speed)
        Returns:
            A boolean, True if speed is set successfully, False if not

        Note:
            Depends on pwm or target mode is selected:
            1) pwm = speed_pc * 255             <-- Currently use this mode.
            2) target_pwm = speed_pc * 100 / 255
             2.1) set pwm{}_enable to 3

        """
        if self.is_psu_fan:
            return False
        speed_pwm_file = "/sys/switch/debug/fan/fan_speed_pwm"
        fan_speed_pwm = ((speed - 20) * (self.curve_pwm_max - self.curve_pwm_min) / 80) \
                         + self.curve_pwm_min
        try:
            with open(speed_pwm_file, 'w') as val_file:
                val_file.write(str(fan_speed_pwm))
        except IOError:
            return False

        time.sleep(0.01)
        return True

    def set_status_led(self, color):
        """
        Sets the state of the fan module status LED
        Args:
            color: A string representing the color with which to set the
                   fan module status LED
        Returns:
            bool: True if status LED state is set successfully, False if not
        """
        if self.is_psu_fan:
            return False

        attr_file = 'led_status'

        if color in list(self.led_status_value.keys()):
            if _write_sysfs_file(self.fan_path + attr_file, self.led_status_value[color]) == 0:
                return False
        else:
            print("Error:Not Support Color={}!".format(color))
            return False

        return True

    def get_status_led(self):
        """
        Gets the state of the fan status LED

        Returns:
            A string, one of the predefined STATUS_LED_COLOR_* strings above
        """
        if self.is_psu_fan:
            return 'N/A'

        led_status = 0
        attr_path = self.fan_path + 'led_status'
        try:
            with open(attr_path, 'r') as led_status_prs:
                led_status = int(led_status_prs.read())
        except IOError:
            return False

        for key, value in list(self.led_status_value.items()):
            if str(led_status) == value:
                return key

        return 'N/A'

    def get_presence(self):
        """
        Retrieves the presence of the fan
        Returns:
            bool: True if fan is present, False if not
        """
        if self.is_psu_fan:
            attr_file = 'status'
            attr_path = self.fan_path + attr_file

            try:
                with open(attr_path, 'r') as psu_fan_prs:
                    status = int(psu_fan_prs.read())
            except IOError as _e:
                print(str(_e))
                return False
            return bool(status)
        else:
            presence = False

            if self._index in self._get_all_fan_presence():
                presence = True

            return presence

    def get_status(self):
        """
        Retrieves the operational status of the device

        Returns:
            A boolean value, True if device is operating properly, False if not
        """
        if self.is_psu_fan:
            if self.get_speed() < 10 or bool(self.get_presence) is not True:
                return False
            else:
                return True
        status = True
        attr_path0 = self.fan_path +'motor0/motor_speed'
        attr_path1 = self.fan_path +'motor1/motor_speed'
        try:
            with open(attr_path0, 'r') as speed_prs0:
                speed0 = int(speed_prs0.read())
        except IOError:
            return False

        try:
            with open(attr_path1, 'r') as speed_prs1:
                speed1 = int(speed_prs1.read())
        except IOError:
            return False
        # confirm by hardware designer
        if speed0 < 2000 or speed1 < 2000:
            status = False
        return status

    def get_hw_version(self):
        """
        Get the hardware version of the fan

        Returns:
            A string
        """
        if self.is_psu_fan:
            return 'N/A'

        attr_file = 'hw_version'
        attr_path = self.fan_path + attr_file
        fan_hw_version = 0

        try:
            with open(attr_path, 'r') as hw_version:
                fan_hw_version = hw_version.read().strip('\n')
        except IOError:
            return False
        return fan_hw_version

    def get_name(self):
        """
        Retrieves the name of the device

        Returns:
            string: The name of the device
        """
        if self.is_psu_fan:
            return "FAN IN POWER{}".format(self._index + 1)

        return "FAN{}".format(self._index + 1)

    def get_serial(self):
        """
        Retrieves the serial number of the device
        Returns:
            string: Serial number of device
        """
        if self.is_psu_fan:
            return 'N/A'

        attr_file = 'sn'
        attr_path = self.fan_path + attr_file
        fan_sn = 0

        try:
            with open(attr_path, 'r') as _sn:
                fan_sn = _sn.read().strip('\n')
        except IOError as _e:
            print(str(_e))
            return False

        return fan_sn

    def get_model(self):
        """
        Retrieves the model number (or part number) of the device
        Returns:
            string: Model/part number of device
        """
        if self.is_psu_fan:
            return 'N/A'

        attr_file = 'product_name'
        attr_path = self.fan_path + attr_file
        fan_product_name = 0

        try:
            with open(attr_path, 'r') as product_name:
                fan_product_name = product_name.read().strip('\n')
        except IOError as _e:
            print(str(_e))
            return False

        return fan_product_name

    def get_change_event(self):
        """
        Retrieves the psu status event

        Args:None

        Returns:
         -----------------------------------------------------------------
         device   |     device_id       |  device_event  |  annotate
         -----------------------------------------------------------------
         'fan'          '<fan number>'     '0'              Fan removed
                                           '1'              Fan inserted
         ------------------------------------------------------------------
            dict: {'fan': {'index':'status'}}
        """
        new_presence = self.get_presence()
        if self._old_presence != new_presence:
            self._old_presence = new_presence
            if new_presence:
                return (True, {'fan': {str(self._index):'1'}})
            else:
                return (True, {'fan': {str(self._index):'0'}})

        return (False, {'fan':{}})
