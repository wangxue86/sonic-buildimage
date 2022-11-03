#!/usr/bin/env python

########################################################################
#
# Module contains an implementation of SONiC Platform Base API and
# provides the Thermals' information which are available in the platform
#MODULE_AUTHOR("Qianchaoyang <qian.chaoyang@h3c.com>");
#MODULE_DESCRIPTION("h3c thermal management");
########################################################################


try:
    import os
    from sonic_platform_base.thermal_base import ThermalBase

except ImportError as _e:
    raise ImportError(str(_e) + "- required module not found")


def find_all_hwmon_paths(name):
    """:
    A string, find each path of hwmon
    """
    hw_list = os.listdir('/sys/class/hwmon/')
    for node in hw_list:
        hw_name = ''
        hw_dir = '/sys/class/hwmon/%s/'%(node)
        try:
            with open(hw_dir + 'name', 'r') as temp:
                hw_name = temp.read()
                if name in hw_name:
                    return hw_dir
        except IOError:
            return False
    return hw_dir

def read_sysfs_file(sysfs_file_path):
    """:
    read form  sysfs_file
    """
    _rv = 'ERR'
    if not os.path.isfile(sysfs_file_path):
        return _rv
    try:
        with open(sysfs_file_path, 'r') as temp:
            _rv = temp.read()
    except IOError:
        _rv = 'ERR'
    _rv = _rv.strip('\r\n').lstrip(" ")
    return _rv

def write_sysfs_file(sysfs_file_path, value):
    """:
    write to sysfs_file
    """
    try:
        with open(sysfs_file_path, 'w') as temp:
            temp.write(str(value))
    except IOError:
        return False
    return True


class Thermal(ThermalBase):
    """ Platform-specific Thermal class"""
    THERMAL_NAME = ('DeviceEnv', 'ASIC_Front', 'ASIC_Back',
                    'CPU Core 0', 'CPU Core 1', 'CPU Core 2', 'CPU Core 3')
    def __init_dir__(self, hwm_mon_alias, hwmon_temp_index):
         self.thermal_sensor_dir = find_all_hwmon_paths(hwm_mon_alias)
         self.thermal_status_file = \
             self.thermal_sensor_dir + "temp{}_alarm".format(hwmon_temp_index)
         self.thermal_temperature_file = \
             self.thermal_sensor_dir + "temp{}_input".format(hwmon_temp_index)
         self.thermal_high_threshold_file = \
             self.thermal_sensor_dir + "temp{}_max".format(hwmon_temp_index)
         self.thermal_low_threshold_file = \
             self.thermal_sensor_dir + "temp{}_min".format(hwmon_temp_index)
         self.thermal_high_crit__file = \
             self.thermal_sensor_dir + "temp{}_crit".format(hwmon_temp_index)


    def __init__(self, thermal_index):
        self.index = thermal_index
        self.is_cpu_thermal = False
        self.is_mac_thermal = False
        self.name = self.THERMAL_NAME[self.index]
        super(Thermal, self).__init__()



        if self.index < 3:
            hwmon_temp_index = self.index + 1
            hwm_mon_alias = 'Max6696'
            self.is_6696_thermal = True
            self.hwmon_temp_index = self.index + 1
            #self.thermal_sensor_dir = find_all_hwmon_paths(hwm_mon_alias)
            self.__init_dir__(hwm_mon_alias, hwmon_temp_index)
            if self.index == 0:
                self.set_low_threshold(0)
                self.set_high_threshold(68)
                self.set_high_critical_threshold(73)
            elif self.index == 1:
                self.set_low_threshold(0)
                self.set_high_threshold(75)
                self.set_high_critical_threshold(89)
            elif self.index == 2:
                self.set_low_threshold(0)
                self.set_high_threshold(88)
                self.set_high_critical_threshold(102)
            else:
                pass


        else:
            hwmon_temp_index = self.index - 1
            hwm_mon_alias = 'coretemp'
            self.is_cpu_thermal = True
            self.__init_dir__(hwm_mon_alias, hwmon_temp_index)

        self._old_presence = self.get_presence()
        self._old_status = self.get_status()

    def get_name(self):
        """
        Retrieves the name of the thermal

        Returns:
        string: The name of the thermal
        """
        #name = "Max6696"
        return self.name

    def get_presence(self):
        """
        Retrieves the presence of the thermal

        Returns:
        bool: True if thermal is present, False if not
        """
        return bool(self.thermal_sensor_dir)

    def get_model(self):
        """
        Retrieves the model number (or part number) of the Thermal

        Returns:
        string: Model/part number of Thermal
        """
        return self.get_name()

    def get_serial(self):
        """
        Retrieves the serial number of the Thermal

        Returns:
        string: Serial number of Thermal
        """
        return 'NA'

    def get_status(self):
        """
        Retrieves the operational status of the thermal

        Returns:
        A boolean value, True if thermal is operating properly,
        False if not
        """
        return 'NA'

    def get_temperature(self):
        """
        Retrieves current temperature reading from thermal

        Returns:
        A float number of current temperature in Celsius up to
        nearest thousandth of one degree Celsius, e.g. 30.125
        """
        thermal_temperature = read_sysfs_file(self.thermal_temperature_file)
        if thermal_temperature != 'ERR':
            thermal_temperature = float(thermal_temperature)
        else:
            thermal_temperature = 0
        return thermal_temperature / 1000.0

    def get_high_threshold(self):
        """
        Retrieves the high threshold temperature of thermal

        Returns:
        A float number, the high threshold temperature of thermal in
        Celsius up to nearest thousandth of one degree Celsius,
        e.g. 30.125
        """
        thermal_high_threshold = read_sysfs_file(self.thermal_high_threshold_file)
        if thermal_high_threshold != 'ERR':
            thermal_high_threshold = float(thermal_high_threshold)
        else:
            thermal_high_threshold = 0
        return thermal_high_threshold / 1000.0

    def get_low_threshold(self):
        """
        Retrieves the low threshold temperature of thermal

        Returns:
        A float number, the low threshold temperature of thermal in
        Celsius up to nearest thousandth of one degree Celsius,
        e.g. 30.125
        """
        thermal_low_threshold = read_sysfs_file(self.thermal_low_threshold_file)
        if thermal_low_threshold != 'ERR':
            thermal_low_threshold = float(thermal_low_threshold)
        else:
            thermal_low_threshold = 0
        return thermal_low_threshold / 1000.0

    def set_high_threshold(self, temperature):
        """
        Sets the high threshold temperature of thermal

        Args :
        temperature: A float number up to nearest thousandth of one
        degree Celsius, e.g. 30.125
        Returns:
        A boolean, True if threshold is set successfully, False if
        not
        """
        if self.is_cpu_thermal:
            return False
        if temperature > 127 or temperature < -127:
            print("The temperature out of range")
            return False
        return bool(write_sysfs_file(self.thermal_high_threshold_file, temperature))

    def set_low_threshold(self, temperature):
        """
        Sets the low threshold temperature of thermal
        Args :
        temperature: A float number up to nearest thousandth of one
        degree Celsius, e.g. 30.125
        Returns:
        A boolean, True if threshold is set successfully, False if
        not
        """
        if self.is_cpu_thermal:
            return False
        if temperature > 127 or temperature < -127:
            print("The temperature out of range")
            return False
        return bool(write_sysfs_file(self.thermal_low_threshold_file, temperature))

    def get_high_critical_threshold(self):
        """
        Retrieves the high critical threshold temperature of thermal

        Returns:
            A float number, the high critical threshold temperature of thermal in Celsius
            up to nearest thousandth of one degree Celsius, e.g. 30.125
        """
        thermal_high_critical_threshold = \
            read_sysfs_file(self.thermal_high_crit__file)
        if thermal_high_critical_threshold != 'ERR':
            thermal_high_critical_threshold = float(thermal_high_critical_threshold)
        else:
            thermal_high_critical_threshold = 0
        return thermal_high_critical_threshold / 1000.0

    def get_low_critical_threshold(self):
        """
        Retrieves the low critical threshold temperature of thermal

        Returns:
            A float number, the low critical threshold temperature of thermal in Celsius
            up to nearest thousandth of one degree Celsius, e.g. 30.125
        """
        return 'NA'

    def set_high_critical_threshold(self, temperature):
        """
        Sets the high critical threshold temperature of thermal
        Args :
        temperature: A float number up to nearest thousandth of one
        degree Celsius, e.g. 30.125
        Returns:
        A boolean, True if threshold is set successfully, False if
        not
        """
        if temperature > 127 or temperature < -127:
            print "The temperature out of range"
            return False

        if self.is_mac_thermal or self.is_cpu_thermal:
            temperature = temperature * 1000
            self.crit_threshold = temperature
            return True
        elif self.is_6696_thermal:
            sysfile = self.thermal_sensor_dir + "temp{}_crit".format(self.hwmon_temp_index)
            if write_sysfs_file(sysfile, temperature):
                return True
            else:
                return False
        else:
            return False



    def get_change_event(self):
        """
        Retrieves the thermal status event

        Args:
            timeout: Timeout in milliseconds (optional). If timeout == 0,
                this method will block until a change is detected.
            scantime: Scan device change event time, default 0.5s

        Returns:
             -----------------------------------------------------------------
             device   |     device_id       |  device_event  |  annotate
             -----------------------------------------------------------------
             'thermal'      '<thermal name>'   '0'              Thermal normal
                                               '1'              Thermal Abnormal
             -----------------------------------------------------------------
        """
        new_presence = self.get_presence()

        if self._old_presence != new_presence:
            self._old_presence = new_presence
            if new_presence:
                return (True, {'thermal': {str(self.index):'0'}})
            return (True, {'thermal': {str(self.index):'1'}})

        return (False, {'thermal':{}})
