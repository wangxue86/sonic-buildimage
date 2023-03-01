#!/usr/bin/env python
"""
Version: 1.0
Module contains an implementation of SONiC Platform Base API and
provides the Fans' information which are available in the platform
"""

try:
    from sonic_platform_base.fan_base import FanBase
    from sonic_platform import log_wrapper
    from vendor_sonic_platform.devcfg import Devcfg
    import os
    import time
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")

FAN_LIST = list(range(0, Devcfg.FAN_NUM))


class Fan(FanBase):
    """
    Platform-specific Fan class
    """
    curve_pwm_max = Devcfg.PWM_REG_MAX
    curve_pwm_min = Devcfg.PWM_REG_MIN
    curve_speed_max = Devcfg.SPEED_TARGET_MAX
    curve_speed_min = Devcfg.SPEED_TARGET_MIN

    def __init__(self, fan_index, is_psu_fan=False):
        FanBase.__init__(self)
        log_wrapper.log_init(self)

        self.led_status_value = {
            self.STATUS_LED_COLOR_OFF: '0',
            self.STATUS_LED_COLOR_GREEN: '1',
            self.STATUS_LED_COLOR_RED: '2'
        }

        if 'STATUS_REMOVED' not in dir(self):
            self.FAN_DIRECTION_R2F = self.FAN_DIRECTION_INTAKE
            self.FAN_DIRECTION_F2R = self.FAN_DIRECTION_EXHAUST

        self._index = fan_index
        self.is_psu_fan = is_psu_fan

        if self.is_psu_fan:
            self.fan_path = Devcfg.PSU_SUB_PATH.format(self._index + 1)
        else:
            self.fan_path = Devcfg.FAN_SUB_PATH.format(self._index + 1)
        self.old_status = 'ok'

    def _write_sysfs_file(self, file_path, value):
        # On successful read, returns the value read from given
        try:
            with open(file_path, 'w') as temp:
                temp.write(value)
        except Exception as err:
            self.logger.log_error(str(err) + '{} write error!'.format(file_path))
            return False

        return True

    def get_attr_val(self, attr_file, default_val):
        """
            get data in attr_file
            @param attr_file: sysfs file name
            @param default_val: default value

            Returns:
                data in attr_file
        """
        attr_path = self.fan_path + '/' + attr_file
        attr_value = default_val

        if self.get_presence() is False:
            return attr_value

        try:
            with open(attr_path, 'r') as attrfd:
                attr_value = attrfd.read().strip('\n')
                val_type = type(default_val)
                if val_type == float:
                    attr_value = float(attr_value)
                elif val_type == str:
                    if not self._is_ascii(attr_value) or attr_value == '':
                        attr_value = 'N/A'
        except Exception:
            self.log.error("failed to read " + attr_path)

        return attr_value

    def get_direction(self):
        """
        Retrieves the direction of fan
        Returns:
            A string, either FAN_DIRECTION_INTAKE or FAN_DIRECTION_EXHAUST
            depending on fan direction
        """
        if self.is_psu_fan:
            return self.FAN_DIRECTION_F2R

        attr_file = 'motor0/direction'
        file_path = self.fan_path + attr_file

        try:
            with open(file_path, 'r') as direction_prs:
                direction = int(direction_prs.read())
        except Exception as err:
            self.logger.log_error(str(err) + '{} read error!'.format(file_path))
            return False

        # F2R: exhaust
        # R2F: intake
        if direction == 0:
            temp = self.FAN_DIRECTION_F2R
        elif direction == 1:
            temp = self.FAN_DIRECTION_R2F
        else:
            return False

        return temp

    def get_speed(self):
        """
        Retrieves the speed of fan as a percentage of full speed
        Returns:
            An integer, the percentage of full fan speed, in the range 0 (off)
                 to 12000 (full speed)
        """
        if self.is_psu_fan:
            attr_file = 'fan'
            attr_path = self.fan_path + attr_file

            try:
                with open(attr_path, 'r') as speed_psu_fan:
                    speed = float(speed_psu_fan.read().strip('\n'))
            except Exception as err:
                self.logger.log_error(str(err))
                # return False
                speed = 0

            motor_max_speed = Devcfg.DEFAULT_PSUFAN_MOTOR_MAX_SPEED

            psu_vendor = str(self.get_vendor())

            for list_info in Devcfg.PSUFAN_MOTOR_MAX_SPEED_LIST:
                if list_info['type'] in psu_vendor:
                    motor_max_speed = list_info['motor']

            speed = speed * 100 / motor_max_speed
        else:
            speed = 0
            motor0_speed = 0
            motor1_speed = 0
            attr_path0 = self.fan_path + 'motor0/speed'
            attr_path1 = self.fan_path + 'motor1/speed'
            motor0_max_speed = Devcfg.DEFAULT_MOTOR0_MAX_SPEED
            motor1_max_speed = Devcfg.DEFAULT_MOTOR1_MAX_SPEED

            try:
                with open(attr_path0, 'r') as speed_prs0:
                    motor0_speed = int(speed_prs0.read())
            except Exception as err:
                self.logger.log_error(str(err))
                # return False
                motor0_speed = 0

            try:
                with open(attr_path1, 'r') as speed_prs1:
                    motor1_speed = int(speed_prs1.read())
            except Exception as err:
                self.logger.log_error(str(err))
                # return False
                motor1_speed = 0

            fan_vendor = str(self.get_vendor())

            for list_info in Devcfg.FAN_MOTOR_MAX_SPEED_LIST:
                if list_info['type'] in fan_vendor:
                    motor0_max_speed = list_info['motor0']
                    motor1_max_speed = list_info['motor1']

            speed0 = motor0_speed * 100 / motor0_max_speed
            speed1 = motor1_speed * 100 / motor1_max_speed
            speed = (speed0 + speed1) / 2

        try:
            speed = int(speed + 0.5)
        except Exception as err:
            self.logger.log_error(str(err))

        if speed > 100:
            speed = 100

        return speed

    @staticmethod
    def _get_file_path_status(main_dir, sub_dir):
        path_status_dir = os.path.join(main_dir, sub_dir)

        try:
            with open(path_status_dir, "r") as status_read:
                fan_status = status_read.read()
        except Exception as err:
            print(str(err))
            return False

        return fan_status

    @staticmethod
    def _get_all_fan_normal():
        normal_list = []

        for index in FAN_LIST:
            fan_presence_dir = Devcfg.FAN_SUB_PATH.format(index + 1)
            status = Fan._get_file_path_status(fan_presence_dir, "status")
            status_int = int(status)
            if status_int == Devcfg.STATUS_OK:
                normal_list.append(index)

        return normal_list

    @staticmethod
    def get_fan_normalnum():
        """
        get fan normalnum
        """
        return len(Fan._get_all_fan_normal())

    @classmethod
    def get_speed_pwm_file_attr(cls):
        """
        get speed pwm file attr
        """
        try:
            last_modify_time = os.stat(Devcfg.FAN_ADJ_DIR).st_mtime
            with open(Devcfg.FAN_ADJ_DIR, 'r') as tar_fd:
                read_target_speed = int(tar_fd.read())
            target_speed = (read_target_speed - cls.curve_pwm_min) * \
                           (cls.curve_speed_max - cls.curve_speed_min) \
                           / (cls.curve_pwm_max - cls.curve_pwm_min) + cls.curve_speed_min

            if target_speed < cls.curve_speed_min:
                target_speed = cls.curve_speed_min

            return last_modify_time, target_speed
        except (OSError, ValueError, IOError) as err:
            cls.logger.log_error(str(err))
            return time.time(), cls.curve_speed_max

    def get_target_speed(self):
        """
         get target speed
        """
        return self.get_speed_pwm_file_attr()[1]

    def get_speed_tolerance(self):
        """
        Retrieves the speed tolerance of the fan
        Returns:
            An integer, the percentage of variance from target speed which is
                 considered tolerable
        """
        vendor = str(self.get_vendor())
        if self.is_psu_fan:
            for list_info in Devcfg.PSUFAN_MOTOR_MAX_SPEED_LIST:
                if list_info['type'] in vendor:
                    tolerance = list_info['tolerance']
                    return tolerance
                if list_info['type'] == 'DEFAULT':
                    default_tolerance = list_info['tolerance']
        else:
            for list_info in Devcfg.FAN_MOTOR_MAX_SPEED_LIST:
                if list_info['type'] in vendor:
                    tolerance = list_info['tolerance']
                    return tolerance
                if list_info['type'] == 'DEFAULT':
                    default_tolerance = list_info['tolerance']
        return default_tolerance

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

        #SPEED_PWM_FILE = "/sys/switch/debug/fan/fan_speed_pwm"
        fan_speed_pwm = ((speed - 20) * (self.curve_pwm_max - self.curve_pwm_min) / 80) \
            + self.curve_pwm_min

        try:
            with open(Devcfg.SPEED_PWM_FILE, 'w') as val_file:
                val_file.write(str(fan_speed_pwm))
        except Exception as err:
            self.logger.log_error(str(err) + '{} write error!'.format(Devcfg.SPEED_PWM_FILE))
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
            if self._write_sysfs_file(self.fan_path + attr_file, self.led_status_value[color]) == 0:
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
                led_status_str = led_status_prs.read().strip('\n')
                if led_status_str.isdigit():
                    led_status = int(led_status_str)
                else:
                    return 'N/A'
        except Exception as err:
            self.logger.log_error(str(err))
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
        attr_file = 'status'
        attr_path = self.fan_path + attr_file
        status = 0

        try:
            with open(attr_path, 'r') as fan_prs:
                status = int(fan_prs.read())
        except Exception as err:
            self.logger.log_error(str(err))
            return False

        return not status == 0

    def _get_init_status(self):
        if self.is_psu_fan:
            if self.get_presence():
                return 'ok'
            return 'removed'

        if not self.get_presence():
            return 'removed'

        now_speed = self.get_speed()
        target_speed = now_speed
        speed_tolerance = self.get_speed_tolerance()

        if target_speed - now_speed > target_speed * speed_tolerance / 100:
            return 'speed low'
        if now_speed - target_speed > target_speed * speed_tolerance / 100:
            return 'speed high'
        return 'ok'

    def get_psu_fan_status(self):  # never return high ?
        """
        Retrieves status string of the psu fan
            STATUS_OK
            STATUS_SPEED_LOW
            STATUS_SPEED_HIGH
        Returns:
            eg: return 'speed low'
        """
        attr_file = 'status_word'
        attr_path = self.fan_path + attr_file
        psu_status_word = 0

        if not self.get_presence():
            return 'removed'
        try:
            with open(attr_path, 'r') as status_word:
                psu_status_word = int(status_word.read())
        except (IOError, ValueError) as err:
            self.log_error(str(err))
            return 'removed'
        power_fan_mask = (1 << 10)
        if psu_status_word & power_fan_mask:
            return 'speed low'
        return 'ok'

    def get_status(self):
        """
        Retrieves status string of the fan
            STATUS_OK
            STATUS_SPEED_LOW
            STATUS_SPEED_HIGH
        Returns:
            eg: return 'speed low'
        """
        if self.is_psu_fan:
            return self.get_psu_fan_status()

        if not self.get_presence():
            return 'removed'

        current_time = time.time()
        now_speed = self.get_speed()
        speed_tolerance = self.get_speed_tolerance()
        start_time, target_speed = self.get_speed_pwm_file_attr()
        if current_time - float(start_time) > 5:
            if target_speed - now_speed > target_speed * speed_tolerance / 100:
                # self.logger.log_warning("Fan-{} speed low "
                #                         .format(self._index + 1))
                self.old_status = 'speed low'
                return 'speed low'
            if now_speed - target_speed > target_speed * speed_tolerance / 100:
                # self.logger.log_warning("Fan-{} speed high"
                #                         .format(self._index + 1))
                self.old_status = 'speed high'
                return 'speed high'
            # self.logger.log_warning("Fan-{} speed ok".format(self._index + 1))
            self.old_status = 'ok'
            return 'ok'
        # self.logger.log_warning("time<5 Fan-{} speed ok".format(self._index + 1))
        return self.old_status

    @staticmethod
    def _is_ascii(string):
        for single in string:
            if ord(single) >= 128:
                return False
        return True

    def get_vendor(self):
        """
        Retrieves the vendor name of the fan
        Returns:
            string: Vendor name of fan
        """
        # TODO: record vendor info when psu fan init; or remove fan
        if self.is_psu_fan:
            import sonic_platform.platform
            chassis = sonic_platform.platform.Platform(['fan', 'psu']).get_chassis()
            return chassis.get_psu(self._index).get_vendor()

        attr_file = 'vendor_name'
        attr_path = self.fan_path + attr_file
        fan_vendor_name = 0

        try:
            with open(attr_path, 'r') as vendor_name:
                fan_vendor_name = vendor_name.read().strip('\n')
                if not self._is_ascii(fan_vendor_name) or fan_vendor_name == '':
                    return 'N/A'
        except Exception as err:
            self.logger.log_error(str(err))
            return False

        return fan_vendor_name

    def get_hw_version(self):
        """
        Get the hardware version of the fan
        Returns:
            A string
        """
        if self.is_psu_fan:
            return 'N/A'

        attr_file = 'hd_version'
        attr_path = self.fan_path + attr_file
        fan_hw_version = 0

        try:
            with open(attr_path, 'r') as hw_version:
                fan_hw_version = hw_version.read().strip('\n')
                if not self._is_ascii(fan_hw_version) or fan_hw_version == '':
                    return 'N/A'
        except Exception as e:
            self.logger.log_error(str(e))
            return False

        return fan_hw_version

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
            with open(attr_path, 'r') as sn:
                fan_sn = sn.read().strip('\n')
                if not self._is_ascii(fan_sn) or fan_sn == '':
                    return 'N/A'
        except Exception as e:
            self.logger.log_error(str(e))
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
                if not self._is_ascii(fan_product_name) or fan_product_name == '':
                    return 'N/A'
        except Exception as e:
            self.logger.log_error(str(e))
            return False

        return fan_product_name
