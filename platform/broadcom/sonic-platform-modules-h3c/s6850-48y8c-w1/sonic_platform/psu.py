#!/usr/bin/env python

#############################################################################
#
# Platform-specific PSU status interface for SONiC
#MODULE_AUTHOR("Qianchaoyang <qian.chaoyang@h3c.com>");
#MODULE_DESCRIPTION("h3c psu management");
#############################################################################

try:
    from sonic_platform_base.psu_base import PsuBase
    from sonic_platform.fan import Fan
except ImportError as _e:
    raise ImportError(str(_e) + "- required module not found")


class Psu(PsuBase):
    """
    Platform-specific Psu class
    """

    def __init__(self, psu_index):
        self._fan_list = []
        self._index = psu_index

        PsuBase.__init__(self)

        self._sysfs_psu_dir = "/sys/switch/psu/psu{}/".format(self._index + 1)
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

    def get_powergood_status(self):
        """
        Retrieves the powergood status of PSU
        Returns:
            A boolean, True if PSU has stablized its output voltages and passed all
            its internal self-tests, False if not.
        """
        attr_file = 'status'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        status = 0
        try:
            with open(attr_path, 'r') as psu_powgood:
                status = int(psu_powgood.read())
        except IOError:
            return False
        if status == 1:
            return True
        return False

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
        # Hardware not supported
        return False

    def get_presence(self):
        """
        Retrieves the presence of the PSU
        Returns:
            bool: True if PSU is present, False if not
        """
        attr_file = 'status'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        status = 0
        try:
            with open(attr_path, 'r') as psu_prs:
                status = int(psu_prs.read())
        except IOError:
            return False
        return bool(status)

    def get_status(self):
        """
        Gets the status of the PSU
        Returns:
            bool: True if PSU is operating properly, False if not
        """
        attr_file = 'status'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        status = 0
        try:
            with open(attr_path, 'r') as psu_prs:
                status = int(psu_prs.read())
        except IOError:
            return False
        # 0:  absent
        # 1:  normal
        # 2:  fault
        if status == 1:
            return True
        return False

    def get_voltage(self):
        """
        Retrieves current PSU voltage output

        Returns:
            A float number, the output voltage in volts,
            e.g. 12.1
        """

        attr_file = 'out_vol'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_voltage = 0

        if self.get_presence() is False:
            psu_voltage = 0.0
            return psu_voltage

        try:
            with open(attr_path, 'r') as voltage:
                psu_voltage = float(voltage.read().strip('\n'))
        except IOError:
            return False
        return psu_voltage


    def get_current(self):
        """
        Retrieves present electric current supplied by PSU

        Returns:
            A float number, electric current in amperes,
            e.g. 15.4
        """
        attr_file = 'out_curr'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_current = 0

        if self.get_presence() is False:
            psu_current = 0.0
            return psu_current

        try:
            with open(attr_path, 'r') as current:
                psu_current = float(current.read())
        except IOError:
            return False
        return psu_current

    def get_power(self):
        """
        Retrieves current energy supplied by PSU

        Returns:
            A float number, the power in watts,
            e.g. 302.6
        """
        attr_file = 'out_power'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_power = 0

        if self.get_presence() is False:
            psu_power = 0.0
            return psu_power

        try:
            with open(attr_path, 'r') as power:
                psu_power = float(power.read())
        except IOError:
            return False
        return psu_power

    def get_name(self):
        """
        Retrieves the name of the device

        Returns:
            string: The name of the device
        """
        return "PSU{}".format(self._index)

    def get_serial(self):
        """
        Retrieves the serial number of the device

        Returns:
            string: Serial number of device
        """
        attr_file = 'sn'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_sn = 0

        if self.get_presence() is False:
            psu_sn = 'N/A'
            return psu_sn

        try:
            with open(attr_path, 'r') as _sn:
                psu_sn = _sn.read()
        except IOError:
            return False
        return psu_sn

    def get_model(self):
        """
        Retrieves the model number (or part number) of the device

        Returns:
            string: Model/part number of device
        """
        attr_file = 'product_name'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_product_name = 0

        if self.get_presence() is False:
            psu_product_name = 'N/A'
            return psu_product_name

        try:
            with open(attr_path, 'r') as product_name:
                psu_product_name = product_name.read().strip('\n')
        except IOError:
            return False
        return psu_product_name


    def get_input_voltage(self):
        """
        Get the input voltage of the PSU

        Returns:
            A float number, the input voltage in volts,
        """
        # Hardware not supported

        attr_file = 'in_vol'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_input_voltage = 0

        if self.get_presence() is False:
            psu_input_voltage = 0.0
            return psu_input_voltage

        try:
            with open(attr_path, 'r') as input_voltage:
                psu_input_voltage = float(input_voltage.read().strip('\n'))
        except IOError:
            return False
        return psu_input_voltage

    def get_input_current(self):
        """
        Get the input electric current of the PSU

        Returns:
            A float number, the input current in amperes, e.g 220.3
        """
        # Hardware not supported

        attr_file = 'in_curr'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_input_current = 0

        if self.get_presence() is False:
            psu_input_current = 0.0
            return psu_input_current

        try:
            with open(attr_path, 'r') as input_current:
                psu_input_current = float(input_current.read())
        except IOError:
            return False
        return psu_input_current


    def get_input_power(self):
        """
        Get the input current energy of the PSU

        Returns:
            A float number, the input power in watts, e.g. 302.6
        """
        attr_file = 'in_power'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_input_power = 0

        if self.get_presence() is False:
            psu_input_power = 0.0
            return psu_input_power

        try:
            with open(attr_path, 'r') as input_power:
                psu_input_power = float(input_power.read())
        except IOError:
            return False
        return psu_input_power

    def get_temperature(self):
        """
        Get the temperature of the PSU

        Returns:
            A string, all the temperature with units of the psu. e.g. '32.5 C, 50.3 C, ...'
        """
        attr_file = 'temp_input'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_temperature = 0

        if self.get_presence() is False:
            psu_temperature = 0.0
            return psu_temperature

        try:
            with open(attr_path, 'r') as temperature:
                psu_temperature = temperature.read().strip('\n')
        except IOError:
            return False
        return psu_temperature


    def get_sw_version(self):
        """
        Get the firmware version of the PSU

        Returns:
            A string
        """
        attr_file = 'fw_version'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_sw_version = 0

        if self.get_presence() is False:
            psu_sw_version = 'N/A'
            return psu_sw_version

        try:
            with open(attr_path, 'r') as sw_version:
                psu_sw_version = sw_version.read().strip('\n')
        except IOError:
            return False
        return psu_sw_version

    def get_hw_version(self):
        """
        Get the hardware version of the PSU

        Returns:
            A string
        """
        attr_file = 'hw_version'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_hw_version = 0

        if self.get_presence() is False:
            psu_hw_version = 'N/A'
            return psu_hw_version

        try:
            with open(attr_path, 'r') as hw_version:
                psu_hw_version = hw_version.read().strip('\n')
        except IOError:
            return False
        return psu_hw_version

    def get_vendor(self):
        """
        Retrieves the vendor name of the psu

        Returns:
            string: Vendor name of psu
        """

        attr_file = 'vendor_name'
        attr_path = self._sysfs_psu_dir + '/' + attr_file
        psu_vendor_name = 0

        if self.get_presence() is False:
            psu_vendor_name = 'N/A'
            return psu_vendor_name

        try:
            with open(attr_path, 'r') as vendor_name:
                psu_vendor_name = vendor_name.read().strip('\n')
        except IOError:
            return False
        return psu_vendor_name

    def get_change_event(self):
        """
        Retrieves the psu status event

        Args:
            timeout: Timeout in milliseconds (optional). If timeout == 0,
                this method will block until a change is detected.
            scantime: Scan device change event time, default 0.5s

        Returns:
            dict: {'psu': {'index':'status'}}
        """
        new_presence = self.get_presence()
        if self._old_presence != new_presence:
            self._old_presence = new_presence
            if new_presence:
                return (True, {'psu': {str( self._index ): '1'}} )
            else:
                return (True, {'psu': {str(self._index): '0'} } )

        return (False, {'psu':{}})

