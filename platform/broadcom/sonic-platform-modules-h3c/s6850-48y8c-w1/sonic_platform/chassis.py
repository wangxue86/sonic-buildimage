#!/usr/bin/env python

#############################################################################
#
# Module contains an implementation of SONiC Platform Base API and
# provides the platform information
#
#############################################################################

try:
    import re
    from sonic_platform_base.chassis_base import ChassisBase
    from sonic_platform.sfp import Sfp
    from sonic_platform.eeprom import Eeprom
    from sonic_platform.fan import Fan
    from sonic_platform.psu import Psu
    from sonic_platform.thermal import Thermal
    from sonic_platform.component import Component
    from sonic_platform.watchdog import Watchdog
    import time
    import os
    from sonic_platform.fan_drawer import FanDrawer
except ImportError as _e:
    raise ImportError(str(_e) + "- required module not found")

try:
    from sonic_daemon_base.daemon_base import Logger
except ImportError as e1:
    from sonic_py_common.logger import Logger
except ImportError as e2:
    raise ImportError(str(e2) + " required module not found")

#one drawer for each fan
FAN_DRAWER_LIST = list(range(0, 5))
FAN_LIST       = list(range(0, 5))
PSU_LIST       = list(range(0, 2))
THERMAL_LIST   = list(range(0, 7))
COMPONENT_LIST = list(range(0, 3))
SFP_PORT_LIST  = list(range(0, 48))
QSFP_PORT_LIST = list(range(48, 56))

REBOOT_CAUSE_POWER_LOSS_FLAG = 0
REBOOT_CAUSE_THERMAL_OVERLOAD_CPU_FLAG = 1
REBOOT_CAUSE_THERMAL_OVERLOAD_ASIC_FLAG = 2
REBOOT_CAUSE_THERMAL_OVERLOAD_OTHER_FLAG = 3
REBOOT_CAUSE_INSUFFICIENT_FAN_SPEED_FLAG = 4
REBOOT_CAUSE_WATCHDOG_FLAG = 5
REBOOT_CAUSE_HARDWARE_OTHER_FLAG = 6
REBOOT_CAUSE_NON_HARDWARE_FLAG = 7
REBOOT_CAUSE_HARDWARE_OTHER_BOOT_SW_FLAG = 8
REBOOT_CAUSE_HARDWARE_OTHER_BUTTON_FLAG = 9

SYSLOG_IDENTIFIER = 'platfom_chassis'
logger = Logger(SYSLOG_IDENTIFIER)

class Chassis(ChassisBase):
    """
    Platform-specific Chassis class
    """
    LAST_REBOOT_CAUSE_PATH = "/var/cache/last_reboot_cause"
    reboot_cause_cpld_path = "/sys/switch/cpld/cpld0/hw_reboot"

    def __init__(self):
        #print 'come to class chassis'

        super(Chassis, self).__init__()

        self._eeprom = Eeprom()

        self._watchdog = Watchdog()

        for index in FAN_LIST:
            self._fan_list.append(Fan(index))

        for index in THERMAL_LIST:
            self._thermal_list.append(Thermal(index))

        for index in PSU_LIST:
            self._psu_list.append(Psu(index))

        #self._sfp_list.append(Sfp(0, 'SFP'))# set a null
        for index in SFP_PORT_LIST:
            self._sfp_list.append(Sfp(index, 'SFP'))
        for index in QSFP_PORT_LIST:
            self._sfp_list.append(Sfp(index, 'QSFP'))

        for index in COMPONENT_LIST:
            self._component_list.append(Component(index))

        for drawer_index in FAN_DRAWER_LIST:
            #one drawer for each fan
            self._fan_drawer_list.append(FanDrawer(drawer_index, [self._fan_list[drawer_index]]))

        try:
            rv = None
            if not os.path.exists(self.reboot_cause_cpld_path):
                time.sleep(5)
                print("wait 5 sec for %s" %(self.reboot_cause_cpld_path))
            with open(self.reboot_cause_cpld_path, 'r') as fd:
                rv = fd.read().rstrip('\r\n')
        except Exception as error:
            print(("Error: unable to open REBOOT_CAUSE_CPLD_PATH file: %s" %(str(error))))
        reg = "RESET_TYPE_\w+=1\s"
        reason = re.findall(reg, rv)
        all_reason = ''.join(reason)
        try:
            with open(self.LAST_REBOOT_CAUSE_PATH, 'w') as fd:
                if "RESET_TYPE_COLD" in all_reason:
                    fd.write(str(REBOOT_CAUSE_POWER_LOSS_FLAG))
                elif "RESET_TYPE_WDT" in all_reason:
                    fd.write(str(REBOOT_CAUSE_WATCHDOG_FLAG))
                elif "RESET_TYPE_SOFT" in all_reason:
                    fd.write(str(REBOOT_CAUSE_NON_HARDWARE_FLAG))
                elif "RESET_TYPE_BOOT_SW" in all_reason:
                    fd.write(str(REBOOT_CAUSE_HARDWARE_OTHER_BOOT_SW_FLAG))
                elif "RESET_TYPE_MAINBOARD_REST" in all_reason:
                    fd.write(str(REBOOT_CAUSE_HARDWARE_OTHER_BUTTON_FLAG))
                elif "RESET_TYPE_CPU_THERMAL" in all_reason:
                    fd.write(str(REBOOT_CAUSE_THERMAL_OVERLOAD_CPU_FLAG))
                else:
                    fd.write(str(REBOOT_CAUSE_HARDWARE_OTHER_FLAG))
        except Exception as error:
                logger.log_error("Error: unable to open LAST_REBOOT_CAUSE_PATH file")


    def get_reboot_cause(self):
        """
        Retrieves the cause of the previous reboot
        """
        _rv = "-1"
        try:
            with open(self.LAST_REBOOT_CAUSE_PATH, 'r') as fd:
                _rv = fd.read().rstrip('\r\n')
        except Exception as error:
            print("Error: unable to open LAST_REBOOT_CAUSE_PATH file: %s" %(str(error)))

        if _rv == str(REBOOT_CAUSE_POWER_LOSS_FLAG):
            return (self.REBOOT_CAUSE_POWER_LOSS, None)
        elif _rv == str(REBOOT_CAUSE_THERMAL_OVERLOAD_CPU_FLAG):
            return (self.REBOOT_CAUSE_THERMAL_OVERLOAD_CPU, None)
        elif _rv == str(REBOOT_CAUSE_THERMAL_OVERLOAD_ASIC_FLAG):
            return (self.REBOOT_CAUSE_THERMAL_OVERLOAD_ASIC, None)
        elif _rv == str(REBOOT_CAUSE_THERMAL_OVERLOAD_OTHER_FLAG):
            return (self.REBOOT_CAUSE_THERMAL_OVERLOAD_OTHER, None)
        elif _rv == str(REBOOT_CAUSE_INSUFFICIENT_FAN_SPEED_FLAG):
            return (self.REBOOT_CAUSE_INSUFFICIENT_FAN_SPEED, None)
        elif _rv == str(REBOOT_CAUSE_WATCHDOG_FLAG):
            return (self.REBOOT_CAUSE_WATCHDOG, None)
        elif _rv == str(REBOOT_CAUSE_HARDWARE_OTHER_FLAG):
            return (self.REBOOT_CAUSE_HARDWARE_OTHER, None)
        elif _rv == str(REBOOT_CAUSE_HARDWARE_OTHER_BOOT_SW_FLAG):
            return (self.REBOOT_CAUSE_HARDWARE_OTHER, 'SW_BIOS_REBOOT')
        elif _rv == str(REBOOT_CAUSE_HARDWARE_OTHER_BUTTON_FLAG):
            return (self.REBOOT_CAUSE_HARDWARE_OTHER, 'PRESS_RESET_BUTTON')

        else:
            return (self.REBOOT_CAUSE_NON_HARDWARE, None)



    def get_sfp(self, index):
        """
        Return a sfp object by index
        Note: the index from 1, because  port_config.ini form Ethernet1
        """
        sfp = None
        index -= 1

        try:
            sfp = self._sfp_list[index]
        except IndexError:
            logger.log_error("SFP index {} out of range (0-{})\n".format(
                             index, len(self._sfp_list)-1))

        return sfp

    def get_change_event(self, timeout=0):
        """
        Returns a nested dictionary containing all devices which have
        experienced a change at chassis level

        Args:
            timeout: Timeout in milliseconds (optional). If timeout == 0,
                this method will block until a change is detected.
            scantime: Scan device change event time, default 0.5s

        Returns:
            (bool, dict):
                - bool: True if call successful, False if not;
                - dict: A nested dictionary where key is a device type,
                        value is a dictionary with key:value pairs in the format of
                        {'device_id':'device_event'}, where device_id is the device ID
                        for this device and device_event.
                        The known devices's device_id and device_event was defined as table below.
                         -----------------------------------------------------------------
                         device   |     device_id       |  device_event  |  annotate
                         -----------------------------------------------------------------
                         'fan'          '<fan number>'     '0'              Fan removed
                                                           '1'              Fan inserted
                                                           '2'              Fan OK
                                                           '3'              Fan speed low
                                                           '4'              Fan speed high

                         'psu'          '<psu number>'     '0'              Psu removed
                                                           '1'              Psu inserted
                                                           '2'              Psu ok
                                                           '3'              Psu power loss
                                                           '4'              Psu fan error
                                                           '5'              Psu voltage error
                                                           '6'              Psu current error
                                                           '7'              Psu power error
                                                           '8'              Psu temperature error

                         'sfp'          '<sfp number>'     '0'              Sfp removed
                                                           '1'              Sfp inserted
                                                           '2'              I2C bus stuck
                                                           '3'              Bad eeprom
                                                           '4'              Unsupported cable
                                                           '5'              High Temperature
                                                           '6'              Bad cable
                                                           '7'              Sfp ok

                         'thermal'      '<thermal name>'   '0'              Thermal normal
                                                           '1'              Thermal Abnormal

                         'voltage'      '<monitor point>'  '0'              Vout normal
                                                           '1'              Vout abnormal
                         -----------------------------------------------------------------
        """
        start_ms = time.time() * 1000
        change_event_dict = {}
        dev_change_event_dict = {}
        event = False
        dev_list = ['fan', 'psu', 'sfp']

        while True:
            time.sleep(0.5)

            for dev in dev_list:
                change_event_dict.update({dev:{}})
               # self.log_notice('get {} event'.format(dev))

                d_list = getattr(self, 'get_all_{}s'.format(dev))()
                for d in d_list:
                    (event, dev_change_event_dict) = getattr(d, 'get_change_event')()
                    if event:
                        change_event_dict[dev].update(dev_change_event_dict[dev])

            for event in list(change_event_dict.values()):
                if len(event):
                    return True, change_event_dict

            if timeout:
                now_ms = time.time() * 1000
                if (now_ms - start_ms >= timeout):
                    for event in list(change_event_dict.values()):
                        if len(event):
                            return True, change_event_dict
                        else:
                            # return False, change_event_dict
                            # temporary modification for xcvrd
                            return True, change_event_dict

    def get_name(self):
        """
        Retrieves the product name for the chassis

        Returns:
            A string containing the product name in the format
        """
        name = ''
        sys_eeprom = self.get_eeprom()
        if sys_eeprom is None:
            logger.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        name = sys_eeprom.modelstr( e )
        if name is None:
            logger.log_error('syseeprom product name is error.')
            return ''
        return name

    def get_status(self):
        """
        Retrieves the operational status of the chassis
        Returns:
            bool: A boolean value, True if chassis is operating properly
            False if not
        """
        return True

    def get_model(self):
        """
        Retrieves the model number (or part number) of the chassis
        Returns:
            string: Model/part number of chassis
        """
        model = ''
        sys_eeprom = self.get_eeprom()
        if sys_eeprom is None:
            logger.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        model = sys_eeprom.manufacture_data_str( e )
        if model is None:
            logger.log_error('syseeprom model number is error.')
            return ''
        return model

    def get_presence(self):
        """
        Retrieves the presence of the chassis
        Returns:
            bool: True if chassis is present, False if not
        """
        return True

    def get_base_mac(self):
        """
        Retrieves the base MAC address for the chassis

        Returns:
            A string containing the MAC address in the format
            'XX:XX:XX:XX:XX:XX'
        """
        base_mac = ''
        sys_eeprom = self.get_eeprom()
        if sys_eeprom is None:
            logger.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        base_mac = sys_eeprom.base_mac_addr( e )
        if base_mac is None:
            logger.log_error( 'syseeprom base mac is error.' )
            return ''

        return base_mac.upper()

    def get_switch_mac(self):
        """
        Retrieves the switch MAC address for the chassis

        Returns:
            A string containing the MAC address in the format
            'XX:XX:XX:XX:XX:XX'
        """
        switch_mac = ''
        sys_eeprom = self.get_eeprom()
        if sys_eeprom is None:
            logger.log_error( 'syseeprom is not inited.' )
            return ''

        e = sys_eeprom.read_eeprom()
        switch_mac = sys_eeprom.switchaddrstr( e )
        if switch_mac is None:
            logger.log_error( 'syseeprom switch mac is error.' )
            return ''

        return switch_mac.upper()

    def get_serial_number(self):
        """
        Retrieves the hardware serial number for the chassis

        Returns:
            A string containing the hardware serial number for this chassis.
        """
        serial_number = ''
        sys_eeprom = self.get_eeprom()
        if sys_eeprom is None:
            logger.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        serial_number = sys_eeprom.serial_number_str(e)
        if serial_number is None:
            logger.log_error('syseeprom serial number is error.')
            return ''

        return serial_number

    def get_serial(self):
        return self.get_serial_number()

    def get_system_eeprom_info(self):
        """
        Retrieves the full content of system EEPROM information for the chassis

        Returns:
            A dictionary where keys are the type code defined in
            OCP ONIE TlvInfo EEPROM format and values are their corresponding
            values.
            Ex. { '0x21':'AG9064', '0x22':'V1.0', '0x23':'AG9064-0109867821',
                  '0x24':'001c0f000fcd0a', '0x25':'02/03/2018 16:22:00',
                  '0x26':'01', '0x27':'REV01', '0x28':'AG9064-C2358-16G'}
        """
        sys_eeprom_dict = dict()
        sys_eeprom = self.get_eeprom()
        if sys_eeprom is None:
            logger.log_error('syseeprom is not inited.')
            return {}

        e = sys_eeprom.read_eeprom()
        if sys_eeprom._TLV_HDR_ENABLED:
            if not sys_eeprom.is_valid_tlvinfo_header( e ):
                logger.log_error( 'syseeprom tlv header error.' )
                return {}
            total_len = (ord( e[9] ) << 8) | ord( e[10] )
            tlv_index = sys_eeprom._TLV_INFO_HDR_LEN
            tlv_end = sys_eeprom._TLV_INFO_HDR_LEN + total_len
        else:
            tlv_index = sys_eeprom.eeprom_start
            tlv_end = sys_eeprom._TLV_INFO_MAX_LEN

        while (tlv_index + 2) < len( e ) and tlv_index < tlv_end:
            if not sys_eeprom.is_valid_tlv( e[tlv_index:] ):
                logger.log_error( "Invalid TLV field starting at EEPROM offset %d" % tlv_index )
                break

            tlv = e[tlv_index:tlv_index + 2 + ord( e[tlv_index + 1] )]
            name, value = sys_eeprom.decoder( None, tlv )
            sys_eeprom_dict[name] = value

            if ord( e[tlv_index] ) == sys_eeprom._TLV_CODE_QUANTA_CRC or \
                    ord( e[tlv_index] ) == sys_eeprom._TLV_CODE_CRC_32:
                break
            tlv_index += ord( e[tlv_index + 1] ) + 2

        return sys_eeprom_dict





