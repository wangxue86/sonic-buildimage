#!/usr/bin/env python
"""
Version: 1.0
Module contains an implementation of SONiC Platform Base API and
provides the Psus' information which are available in the platform
"""

try:
    from sonic_platform_base.psu_base import PsuBase
    from sonic_platform.fan import Fan
    from sonic_platform import log_wrapper
    from vendor_sonic_platform.devcfg import Devcfg
except ImportError as e:
    raise ImportError(str(e) + "- required module not found")


class Psu(PsuBase):
    """
    Platform-specific Psu class
    """
    ALARM_NORMAL = 'ok'
    ALARM_TEMP_ERR = 'temperature error'
    ALARM_FAN_ERR = 'fan error'
    ALARM_VOL_ERR = 'voltage error'

    def __init__(self, psu_index):
        self._fan_list = []
        self._index = psu_index
        PsuBase.__init__(self)

        log_wrapper.log_init(self)

        if 'STATUS_REMOVED' not in dir(self):
            # Possible psu status
            self.STATUS_REMOVED = 'removed'
            self.STATUS_INSERTED = 'inserted'
            self.STATUS_OK = 'ok'
            self.STATUS_POWER_LOSS = 'power loss'
            self.STATUS_FAN_ERR = 'fan error'
            self.STATUS_VOL_ERR = 'voltage error'
            self.STATUS_CURRENT_ERR = 'current error'
            self.STATUS_POWER_ERR = 'power error'
            self.STATUS_TEMP_ERR = 'temperature error'

        self.sysfs_psu_dir = Devcfg.PSU_SUB_PATH.format(self._index + 1)
        self._old_presence = self.get_presence()
        self._old_status = self.get_status()

        psu_fan = Fan(fan_index=self._index, is_psu_fan=True)
        self._fan_list.append(psu_fan)

    def get_num_fans(self):
        """
        Retrieves the number of fan modules available on this PSU
        Returns:
            An integer, the number of fan modules available on this PSU
        """

        return len(self._fan_list)

    def get_all_fans(self):
        """
        Retrieves all fan modules available on this PSU
        Returns:
            A list of objects derived from FanBase representing all fan
            modules available on this PSU
        """

        return self._fan_list

    def get_fan(self, index):
        """
        Retrieves object representing the fan module contained in this PSU
        Returns:
            An object dervied from FanBase representing the fan module
            contained in this PSU
        """
        return super(Psu, self).get_fan(index)

    def get_powergood_status(self):
        """
        Retrieves the powergood status of PSU
        Returns:
            A boolean, True if PSU has stablized its output voltages and passed all
            its internal self-tests, False if not.
        """
        attr_file = 'status'
        attr_path = self.sysfs_psu_dir + attr_file
        status = 0

        try:
            with open(attr_path, 'r') as psu_powgood:
                status = int(psu_powgood.read())
        except Exception as e:
            self.logger.log_error(str(e))
            return False

        return status == 1

    def set_status_led(self, color):
        """
        Sets the state of the PSU status LED
        Args:
            color: A string representing the color with which to set the PSU status LED
                   Note: Only support green and off
        Returns:
            bool: True if status LED state is set successfully, False if not
        """
        # Hardware not supported
        return False

    def get_status_led(self):
        """
        Get the state of the PSU status LED
        Returns:
            A string list, all of the predefined STATUS_LED_COLOR_* strings above
        """
        psu_led_status_list = []
        status = self.get_attr_val('led_status', 0)
        if status == 0:
            psu_led_status_list.append(self.STATUS_LED_COLOR_OFF)
        elif status == 1:
            psu_led_status_list.append(self.STATUS_LED_COLOR_GREEN)
        elif status == 3:
            psu_led_status_list.append(self.STATUS_LED_COLOR_AMBER)
        else:
            psu_led_status_list.append("N/A")

        return psu_led_status_list

    def get_presence(self):
        """
        Retrieves the presence of the PSU
        Returns:
            bool: True if PSU is present, False if not
        """
        attr_file = 'status'
        attr_path = self.sysfs_psu_dir + attr_file
        status = 0

        try:
            with open(attr_path, 'r') as psu_prs:
                status = int(psu_prs.read())
        except Exception as e:
            self.logger.log_error(str(e))
            return False

        return not status == 0

    def get_status(self):
        """
        Gets the status of the PSU
        Returns:
            A string list, all of the predefined STATUS_* strings above
            # Possible psu status
              STATUS_OK = 'ok'
              STATUS_POWER_LOS = 'power los'
              STATUS_FAN_ERR = 'fan error'
              STATUS_VOL_ERR = 'vol error'
              STATUS_CURRENT_ERR = 'current error'
              STATUS_POWER_ERR = 'power error'
              STATUS_TEMP_ERR = 'temperature error'
        """
        attr_file = 'status_word'
        attr_path = self.sysfs_psu_dir + attr_file
        psu_status_word = 0
        psu_status_list = []

        if not self.get_presence():
            psu_status_list.append(self.STATUS_REMOVED)
            return psu_status_list

        try:
            with open(attr_path, 'r') as status_word:
                psu_status_word = int(status_word.read())
        except Exception as e:
            self.logger.log_error(str(e))
            return psu_status_list

        power_los_mask = (1 << 6)
        power_vol_mask = (1 << 5 | 1 << 3 | 1 << 15)
        power_current_mask = (1 << 4 | 1 << 14)
        power_temp_mask = (1 << 2)
        power_power_mask = (1 << 11)
        power_io_po_mask = (1 << 14)
        power_fan_mask = (1 << 10)

        if psu_status_word & power_los_mask:
            psu_status_list.append(self.STATUS_POWER_LOSS)
        else:
            if psu_status_word & power_vol_mask:
                psu_status_list.append(self.STATUS_VOL_ERR)
            if psu_status_word & power_current_mask:
                psu_status_list.append(self.STATUS_CURRENT_ERR)
            if psu_status_word & power_temp_mask:
                psu_status_list.append(self.STATUS_TEMP_ERR)
            if (psu_status_word & power_power_mask) or (psu_status_word & power_io_po_mask):
                psu_status_list.append(self.STATUS_POWER_ERR)
            if psu_status_word & power_fan_mask:
                psu_status_list.append(self.STATUS_FAN_ERR)

        if not psu_status_list:
            psu_status_list.append(self.STATUS_OK)

        return psu_status_list

    def get_voltage(self):
        """
        Retrieves current PSU voltage output
        Returns:
            A float number, the output voltage in volts,
            e.g. 12.1
        """
        return self.get_attr_val('out_vol', 0.0)

    def get_current(self):
        """
        Retrieves present electric current supplied by PSU
        Returns:
            A float number, electric current in amperes,
            e.g. 15.4
        """
        return self.get_attr_val('out_curr', 0.0)

    def get_power(self):
        """
        Retrieves current energy supplied by PSU
        Returns:
            A float number, the power in watts,
            e.g. 302.6
        """
        return self.get_attr_val('out_power', 0.0)

    def _is_ascii(self, string):
        for s in string:
            if ord(s) >= 128:
                return False
        return True

    def get_serial(self):
        """
        Retrieves the serial number of the device
        Returns:
            string: Serial number of device
        """
        return self.get_attr_val('sn', 'N/A')

    def get_model(self):
        """
        Retrieves the model number (or part number) of the device
        Returns:
            string: Model/part number of device
        """
        return self.get_attr_val('product_name', 'N/A')

    def get_input_voltage(self):
        """
        Get the input voltage of the PSU
        Returns:
            A float number, the input voltage in volts,
        """
        # Hardware not supported
        if Devcfg.PSU_IS_1600W == True:
            return self.get_attr_val('in_vol', 0.0)
        else:
            return 'N/A'

    def get_input_current(self):
        """
        Get the input electric current of the PSU
        Returns:
            A float number, the input current in amperes, e.g 220.3
        """
        # Hardware not supported
        if Devcfg.PSU_IS_1600W == True:
            return self.get_attr_val('in_curr', 0.0)
        else:
            return 'N/A'

    def get_input_power(self):
        """
        Get the input current energy of the PSU
        Returns:
            A float number, the input power in watts, e.g. 302.6
        """
        return self.get_attr_val('in_power', 0.0)

    def get_temperature(self):
        """
        Get the temperature of the PSU
        Returns:
            A float number, the temperature of the psu. e.g. '32.5, 50.3, ...'
        """
        return self.get_attr_val('temp_input', '0.0')

    # @property
    def get_sw_version(self):
        """
        Get the firmware version of the PSU
        Returns:
            A string
        """
        attr_file = 'fw_version'
        attr_path = self.sysfs_psu_dir + attr_file
        psu_sw_version = 0

        if not self.get_presence():
            psu_sw_version = 'N/A'
            return psu_sw_version

        try:
            with open(attr_path, 'r') as sw_version:
                psu_sw_version = sw_version.read().strip('\n')
                if not self._is_ascii(psu_sw_version) or psu_sw_version == '':
                    return 'N/A'
                if self.get_vendor() == 'FSP':
                    psu_sw_version = psu_sw_version[1:]
        except Exception as e:
            self.logger.log_error(str(e))
            return False

        return psu_sw_version

    def get_hw_version(self):
        """
        Get the hardware version of the PSU
        Returns:
            A string
        """
        attr_file = 'hd_version'
        attr_path = self.sysfs_psu_dir + attr_file
        psu_hw_version = 0

        if not self.get_presence():
            psu_hw_version = 'N/A'
            return psu_hw_version

        try:
            with open(attr_path, 'r') as hw_version:
                psu_hw_version = hw_version.read().strip('\n')
                if not self._is_ascii(psu_hw_version) or psu_hw_version == '':
                    return 'N/A'
                if self.get_vendor() == 'FSP':
                    psu_hw_version = psu_hw_version[1:]
        except Exception as e:
            self.logger.log_error(str(e))
            return False

        return psu_hw_version

    def get_vendor(self):
        """
        Retrieves the vendor name of the psu
        Returns:
            string: Vendor name of psu
        """

        attr_file = 'vendor_name_id'
        attr_path = self.sysfs_psu_dir + attr_file

        if not self.get_presence():
            psu_vendor_name = 'N/A'
            return psu_vendor_name

        try:
            with open(attr_path, 'r') as vendor_name:
                psu_vendor_name = vendor_name.read().strip('\n')
                if 'GRE' in psu_vendor_name:
                    psu_vendor_name = 'GRE'
                elif 'FSP' in psu_vendor_name or '3Y POWER' in psu_vendor_name:
                    psu_vendor_name = 'FSP'
                elif 'DELTA' in psu_vendor_name:
                    psu_vendor_name = 'DELTA'
                else:
                    psu_vendor_name = 'N/A'

        except Exception as e:
            self.logger.log_error(str(e))
            return False

        return psu_vendor_name

    def get_date(self):
        """
        Get the date of the PSU
        Returns:
            A string, the date of the PSU. e.g. '2009-09-03'
        """
        return self.get_attr_val('date', 'N/A')

    def get_alarm(self):
        """
        Gets the alarm of the PSU
        Returns:
            A string list, all of the predefined ALARM_* strings above
            # Possible psu alarm
                ALARM_NORMAL = 'ok'
                ALARM_TEMP_ERR = 'temperature error'
                ALARM_FAN_ERR = 'fan error'
                ALARM_VOL_ERR = 'voltage error'
        """
        attr_file = 'alarm'
        attr_path = self.sysfs_psu_dir + attr_file
        psu_alarm_word = 0
        psu_alarm_list = []

        if not self.get_presence():
            psu_alarm_list.append(self.STATUS_REMOVED)
            return psu_alarm_list

        try:
            with open(attr_path, 'r') as alarm_word:
                psu_alarm_word = int(alarm_word.read())
        except Exception as e:
            self.logger.log_error(str(e))
            return psu_alarm_list

        temp_mask = (1 << 0)
        fan_mask = (1 << 1)
        vol_mask = (1 << 2)

        if psu_alarm_word & temp_mask:
            psu_alarm_list.append(self.ALARM_TEMP_ERR)
        if psu_alarm_word & fan_mask:
            psu_alarm_list.append(self.ALARM_FAN_ERR)
        if psu_alarm_word & vol_mask:
            psu_alarm_list.append(self.ALARM_VOL_ERR)

        if not psu_alarm_list:
            psu_alarm_list.append(self.ALARM_NORMAL)

        return psu_alarm_list

    def get_alarm_threshold_curr(self):
        """
        Get the current alarm threshold of the PSU
        Returns:
            A float number, the current alarm threshold of the PSU
        """
        return self.get_attr_val('alarm_threshold_curr', '0.0')

    def get_alarm_threshold_vol(self):
        """
        Get the voltage alarm threshold of the PSU
        Returns:
            A float number, the voltage alarm threshold of the PSU
        """
        return self.get_attr_val('alarm_threshold_vol', '0.0')

    def get_max_output_power(self):
        """
        Get the max output power of the PSU
        Returns:
            A float number, the max output power of the PSU
        """
        return self.get_attr_val('max_output_power', '0.0')

    def get_name(self):
        """
        Retrieves the name of the device

        Returns:
            string: The name of the device
        """
        return "PSU{}".format(self._index)


    def get_pn(self):
        """
        Get the part number of the PSU
        Returns:
            A string, the part number of the PSU
        """
        return self.get_attr_val('part_number', 'N/A')

    def get_attr_val(self, attr_file, default_val):
        """
            get message in attr_file

            Returns:
                message in attr_file
        """
        attr_path = self.sysfs_psu_dir + '/' + attr_file
        attr_value = default_val

        if self.get_presence() is False:
            return attr_value

        try:
            with open(attr_path, 'r') as attrfd:
                attr_value = attrfd.read().strip('\n')
                valtype = type(default_val)
                if valtype == float:
                    attr_value = float(attr_value)
                elif valtype == int:
                    attr_value = int(attr_value)
                elif valtype == str:
                    if not self._is_ascii(attr_value) or attr_value == '':
                        attr_value = 'N/A'
        except Exception as e:
            self.logger.log_error(str(e))

        return attr_value
