#!/usr/bin/env python
"""
Module contains an implementation of SONiC Platform Base API and
provides the Chassis information
"""
try:
    import operator
    import os
    from sonic_platform.chassis_common import ChassisCommon
    from sonic_platform.sfp import Sfp
    from sonic_platform.eeprom import Eeprom
    from sonic_platform.fan import Fan
    from sonic_platform.psu import Psu
    from sonic_platform.thermal import Thermal
    from sonic_platform.component import Component
    from sonic_platform.fan_drawer import FanDrawer
    from vendor_sonic_platform.devcfg import Devcfg
    from vendor_sonic_platform.utils import read_file
    from sonic_platform import log_wrapper
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")

class Chassis(ChassisCommon):
    """
    Platform-specific Chassis class
    """
    sfp_control = ""
    _port_base_path = {}

    def __init__(self, dev_list=[]):
        super(Chassis, self).__init__()
        log_wrapper.log_init(self)
        fan_list = list()
        thermal_list = list()
        psu_list = list()
        sfp_list = list()
        component_list = list()
        module_list = list()

        if 'syseeprom' in dev_list:
            syseeprom = Eeprom()
            self.logger.log_debug('syseeprom inited.')
        else:
            syseeprom = None

        watchdog = None

        if 'fan' in dev_list:
            for index in range(0, Devcfg.FAN_NUM):
                fan_list.append(Fan(index))
                self.logger.log_debug('fan{} inited.'.format(index))

        if 'fan_drawer' in dev_list:
            for index in range(0, Devcfg.FAN_DRAWER_NUM):
                self._fan_drawer_list.append(FanDrawer(index))

        if 'thermal' in dev_list:
            for index in range(0, Devcfg.THERMAL_NUM):
                thermal_list.append(Thermal(index))
                self.logger.log_debug('thermal{} inited.'.format(index))

        if 'psu' in dev_list:
            for index in range(0, Devcfg.PSU_NUM):
                psu_list.append(Psu(index))
                self.logger.log_debug('psu{} inited.'.format(index))

        if 'sfp' in dev_list:
            for sfpinfo in Devcfg.SFP_INFO:
                for index in range(sfpinfo['start'] - 1, sfpinfo['end']):
                    sfp_list.append(Sfp(index, sfpinfo['type']))
                    self.logger.log_debug('sfp{} inited.'.format(index))

        if 'component' in dev_list:
            for index in range(0, Devcfg.COMPONENT_NUM):
                component_list.append(Component(index))
                self.logger.log_debug('component{} inited.'.format(index))

        self.device_init(component_list, fan_list, psu_list, thermal_list,
                         sfp_list, watchdog, syseeprom, module_list)

    def _get_second_vol_status(self, spotinfo):
        vol_status = []
        vol_type = spotinfo['type']
        uplimit = spotinfo['UpLimit']
        lowlimit = spotinfo['LowLimit']
        his_flag = spotinfo['his_flag']

        try:
            with open(Devcfg.SECOND_VOL_MAIN_PATH + vol_type, 'r') as val_file:
                value = float(val_file.readline().rstrip())
        except Exception as error:
            self.logger.log_error("Error: unable to open file: %s" % str(error))
            return False

        if ((value > uplimit) or (value < lowlimit)):
            if his_flag == 0:
                value = value / float(1000)
                vol_status = "abnormal({}v)".format(value)
                return vol_status
        else:
            if his_flag == 1:
                value = value / float(1000)
                vol_status = "normal({}v)".format(value)
                return vol_status

        return vol_status

    def get_voltage_monitor_event(self):
        """
        Returns:
            (bool, dict):
                - bool: True if any voltage point of the mainboard is abnormal
                - dict: {'voltage': {'monitor point':'status info'}}
                    Ex. (True, {'voltage':{'1.8v':'abnormal(1.6v)'}})
                    Indicates that:
                        has event, the 1.8v monitor point is abnormal(1.6v).
        """
        change_event_list = {
            'voltage': {}
        }
        voltage_dict = {}

        for spot_info in Devcfg.SECOND_VOL_LIST:
            vol_change_value = []
            vol_change_value = self._get_second_vol_status(spot_info)
            if vol_change_value:
                voltage_dict[spot_info['type']] = vol_change_value
                spot_info['his_flag'] = vol_change_value[0]

        for event in list(voltage_dict.values()):
            if event:
                change_event_list['voltage'] = voltage_dict
                return True, change_event_list

        return False, change_event_list

    def get_thermal_manager(self):
        """
        Retrieves the class of thermal manager
        Returns:
            An Class of ThermalManager
        """

        return None

    def initizalize_system_led(self):
        return True

    def get_status_led(self):
        # TODO: get the led value from cpld
        return "green"
