#!/usr/bin/env python
"""
Module contains an implementation of SONiC Platform Base API and
provides the Thermals' information which are available in the platform
"""

try:
    import os
    from sonic_platform_base.thermal_base import ThermalBase
    from vendor_sonic_platform.devcfg import Devcfg
    from sonic_platform import log_wrapper
    from vendor_sonic_platform.utils import get_mac_temp_validata
    from vendor_sonic_platform.utils import get_sfp_max_temp
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")


class Thermal(ThermalBase):
    """H3C Platform-specific Thermal class"""

    def __init__(self, thermal_index):
        self.index = thermal_index
        thermal_info = Devcfg.THERMAL_INFO[self.index]
        self.is_cpu_thermal = thermal_info['type'] == 'cpu'
        self.is_6696_thermal = thermal_info['type'] == 'max6696'
        self.is_mac_thermal = thermal_info['type'] == 'mac'
        self.is_sfp_thermal = thermal_info['type'] == 'sfp'
        self.cpu_low_threshold = 0.0
        self.name = thermal_info['name']
        super(Thermal, self).__init__()

        log_wrapper.log_init(self)
        self.low_threshold = 0.0
        self.high_threshold = 0.0
        self.crit_threshold = 0.0

        hwmon_temp_index = thermal_info['hwmon_temp_index']
        hwm_mon_alias = thermal_info['hwmon_alias']

        if 'Max6696' in hwm_mon_alias:
            self.thermal_sensor_dir = self._find_all_hwmon_paths(hwm_mon_alias, True)
        else:
            self.thermal_sensor_dir = self._find_all_hwmon_paths(hwm_mon_alias, False)
        self.thermal_status_file = self.thermal_sensor_dir + "temp{}_alarm".format(hwmon_temp_index)
        self.thermal_temperature_file = self.thermal_sensor_dir + "temp{}_input".format(hwmon_temp_index)
        self.thermal_high_threshold_file = self.thermal_sensor_dir + "temp{}_max".format(hwmon_temp_index)
        self.thermal_low_threshold_file = self.thermal_sensor_dir + "temp{}_min".format(hwmon_temp_index)
        self.thermal_high_critical_threshold_file = self.thermal_sensor_dir + "temp{}_crit".format(hwmon_temp_index)
        self._old_presence = self.get_presence()
        self._old_status = self.get_status()

        self.get_default_temp_threshold(self.name)

        self.set_low_threshold(self.low_threshold)
        self.set_high_threshold(self.high_threshold)
        self.set_high_critical_threshold(self.crit_threshold)

    def get_default_temp_threshold(self, name):
        for node in Devcfg.FAN_ADJ_LIST:
            if (node['name'] == self.name) and (node['crit'] is not None):
                self.low_threshold = node['min']
                self.high_threshold = node['max']
                self.crit_threshold = node['crit']
                break

    def _find_all_hwmon_paths(self, name, is_max6696):
        hw_dir = None
        hw_list = os.listdir(Devcfg.HWMON_DIR)
        hw_list.sort(key=lambda x: int(x[5:]))

        for node in hw_list:
            hw_name = ''
            hw_dir = Devcfg.HWMON_DIR + ('%s/' % (node))

            try:
                with open(hw_dir + 'name', 'r') as temp:
                    hw_name = temp.read()
                if name in hw_name:
                    return hw_dir
                    '''
                    if is_max6696 is False:
                        return hw_dir
                    else:
                        with open(hw_dir + 'temp_alias', 'r') as temp:
                            hw_name = temp.read()
                        if self.name in hw_name:
                            return hw_dir
                    '''
                            
            except Exception as error:
                self.logger.log_error(str(error))
                return 'N/A'
        return hw_dir

    def _read_sysfs_file(self, sysfs_file_path):
        value = 'ERR'
        if not os.path.isfile(sysfs_file_path):
            return value
        try:
            with open(sysfs_file_path, 'r') as temp:
                value = temp.read().strip()
                positive_value = value
                if value[0] == '-':
                    positive_value = value[1:]
                if not positive_value.replace('.', '').isdigit():
                    value = 'ERR'
                    self.logger.log_error("read {} : {}".format(sysfs_file_path, value))
        except Exception as error:
            self.logger.log_error(str(error))
            value = 'ERR'
        return value

    def _write_sysfs_file(self, sysfs_file_path, value):
        try:
            with open(sysfs_file_path, 'w') as temp:
                temp.write(str(value))
        except Exception as error:
            self.logger.log_error(str(error))
            return False
        return True

    def get_name(self):
        """
        Retrieves the name of the thermal
        Returns:
        string: The name of the thermal
        """
        return self.name

    def get_presence(self):
        """
        Retrieves the presence of the thermal
        Returns:
        bool: True if thermal is present, False if not
        """
        if self.is_mac_thermal:
            return True
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
        return 'N/A'

    def get_status(self):
        """
        Retrieves the operational status of the thermal
        Returns:
        A boolean value, True if thermal is operating properly,
        False if not
        """
        return True

    def get_temperature(self):
        """
        Retrieves current temperature reading from thermal
        Returns:
        A float number of current temperature in Celsius up to
        nearest thousandth of one degree Celsius, e.g. 30.125
        """
        all_temp = []
        if self.is_mac_thermal:
            return float(format(get_mac_temp_validata(), '.1f'))
        elif self.is_sfp_thermal:
            temp = get_sfp_max_temp()
            if 'N/A' != temp:
                return float(format(temp, '.1f'))
            else:
                return temp
        elif self.is_cpu_thermal:
            for i in range(Devcfg.CPU_THERMAL_IDX_START, Devcfg.CPU_THERMAL_IDX_START + Devcfg.CPU_THERMAL_NUM):
                sysfile = self.thermal_sensor_dir + "temp{}_input".format(i)
                # print(sysfile)
                temp = self._read_sysfs_file(sysfile)
                if temp != 'ERR':
                    temp = float(temp)
                    all_temp.append(temp)
                else:
                    return 'N/A'
            temp = max(all_temp)
            return float(format(temp / 1000.0, '.1f'))
        elif self.is_6696_thermal:
            thermal_temperature = self._read_sysfs_file(self.thermal_temperature_file)
            if thermal_temperature != 'ERR':
                thermal_temperature = float(thermal_temperature)/ 1000.0
            else:
                return 'N/A'  # TODO: should be float
            return float(format(thermal_temperature, '.1f'))

    def get_high_threshold(self):
        """
        Retrieves the high threshold temperature of thermal
        Returns:
        A float number, the high threshold temperature of thermal in
        Celsius up to nearest thousandth of one degree Celsius,
        e.g. 30.125
        """
        if self.is_mac_thermal or self.is_cpu_thermal or self.is_sfp_thermal:
            return float(format(self.high_threshold / 1000.0, '.1f'))
        elif self.is_6696_thermal:
            thermal_high_threshold = self._read_sysfs_file(self.thermal_high_threshold_file)
            if thermal_high_threshold != 'ERR':
                thermal_high_threshold = float(thermal_high_threshold)/ 1000.0
            else:
                return 'N/A'  # TODO: should be float
            return thermal_high_threshold

    def get_low_threshold(self):
        """
        Retrieves the low threshold temperature of thermal
        Returns:
        A float number, the low threshold temperature of thermal in
        Celsius up to nearest thousandth of one degree Celsius,
        e.g. 30.125
        """
        if self.is_mac_thermal or self.is_cpu_thermal or self.is_sfp_thermal:
            return float(format(self.low_threshold / 1000.0, '.1f'))
        elif self.is_6696_thermal:
            thermal_low_threshold = self._read_sysfs_file(self.thermal_low_threshold_file)

        if thermal_low_threshold != 'ERR':
            thermal_low_threshold = float(thermal_low_threshold)/ 1000.0
        else:
            return 'N/A'  # TODO: should be float
        return thermal_low_threshold

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
        if temperature > 127 or temperature < -127:
            return False
        if self.is_mac_thermal or self.is_cpu_thermal or self.is_sfp_thermal:
            temperature = temperature * 1000
            self.high_threshold = temperature
            return True
        elif self.is_6696_thermal:
            return self._write_sysfs_file(self.thermal_high_threshold_file, temperature)

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
        if temperature > 127 or temperature < -127:
            return False
        if self.is_mac_thermal or self.is_cpu_thermal or self.is_sfp_thermal:
            temperature = temperature * 1000
            self.low_threshold = temperature
            return True
        elif self.is_6696_thermal:

            return self._write_sysfs_file(self.thermal_low_threshold_file, temperature)

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
            return False
        if self.is_mac_thermal or self.is_cpu_thermal or self.is_sfp_thermal:
            temperature = temperature * 1000
            self.crit_threshold = temperature
            return True
        elif self.is_6696_thermal:
            return self._write_sysfs_file(self.thermal_high_critical_threshold_file, temperature)

    def get_high_critical_threshold(self):
        """
        Retrieves the high critical threshold temperature of thermal
        Returns:
            A float number, the high critical threshold temperature of thermal in Celsius
            up to nearest thousandth of one degree Celsius, e.g. 30.125
        """
        if self.is_mac_thermal or self.is_cpu_thermal or self.is_sfp_thermal:
            return float(format(self.crit_threshold / 1000.0, '.1f'))
        elif self.is_6696_thermal:
            thermal_high_critical_threshold = self._read_sysfs_file(self.thermal_high_critical_threshold_file)
            if thermal_high_critical_threshold != 'ERR':
                thermal_high_critical_threshold = float(thermal_high_critical_threshold)/ 1000.0
            else:
                return 'N/A'  # TODO: should be float

            return thermal_high_critical_threshold

    def get_low_critical_threshold(self):
        """
        Retrieves the low critical threshold temperature of thermal
        Returns:
            A float number, the low critical threshold temperature of thermal in Celsius
            up to nearest thousandth of one degree Celsius, e.g. 30.125
        """
        return 'N/A'  # TODO: should be float

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
                return (True, {'thermal': {str(self.index): '0'}})
            return (True, {'thermal': {str(self.index): '1'}})
        return (False, {'thermal': {}})
