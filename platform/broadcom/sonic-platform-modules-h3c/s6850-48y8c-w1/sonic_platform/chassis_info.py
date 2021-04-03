#
# chassis_info.py
#
# chassis class for common implementations, extended for chassis_base
#
#

from sonic_platform_base.chassis_base import ChassisBase
from abc import ABCMeta,abstractmethod
import time

class ChassisInfo(ChassisBase):
    """
    chassis class, must be inited with args:
        component_list,
        fan_list,
        psu_list,
        thermal_list,
        sfp_list,
        watchdog,
        syseeprom,
        module_list=None

    Note:
        all device list index form 1
    """
    __metaclass__ = ABCMeta

    def __init__(self):
        super(ChassisInfo, self).__init__()

    def device_init(self,
                component_list=None,
                fan_list=None,
                psu_list=None,
                thermal_list=None,
                sfp_list=None,
                watchdog=None,
                syseeprom=None,
                module_list=None):
        self._component_list = component_list
        self._module_list = module_list
        self._fan_list = fan_list
        self._psu_list = psu_list
        self._thermal_list = thermal_list
        self._sfp_list = sfp_list
        self._watchdog = watchdog
        self._eeprom = syseeprom

    def get_name(self):
        """
        Retrieves the product name for the chassis

        Returns:
            A string containing the product name in the format
        """
        name = ''
        sys_eeprom = self.get_eeprom()
        if sys_eeprom is None:
            self.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        name = sys_eeprom.modelstr(e)
        if name is None:
            self.log_error('syseeprom product name is error.')
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
            self.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        model = sys_eeprom.modelnumber(e)
        if model is None:
            self.log_error('syseeprom model number is error.')
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
            self.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        base_mac = sys_eeprom.base_mac_addr(e)
        if base_mac is None:
            self.log_error('syseeprom base mac is error.')
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
            self.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        switch_mac = sys_eeprom.switchaddrstr(e)
        if switch_mac is None:
            self.log_error('syseeprom switch mac is error.')
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
            self.log_error('syseeprom is not inited.')
            return ''

        e = sys_eeprom.read_eeprom()
        serial_number = sys_eeprom.serial_number_str(e)
        if serial_number is None:
            self.log_error('syseeprom serial number is error.')
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
            self.log_error('syseeprom is not inited.')
            return {}

        e = sys_eeprom.read_eeprom()
        if sys_eeprom._TLV_HDR_ENABLED:
            if not sys_eeprom.is_valid_tlvinfo_header(e):
                self.log_error('syseeprom tlv header error.')
                return {}
            total_len = (ord(e[9]) << 8) | ord(e[10])
            tlv_index = sys_eeprom._TLV_INFO_HDR_LEN
            tlv_end = sys_eeprom._TLV_INFO_HDR_LEN + total_len
        else:
            tlv_index = sys_eeprom.eeprom_start
            tlv_end = sys_eeprom._TLV_INFO_MAX_LEN

        while (tlv_index + 2) < len(e) and tlv_index < tlv_end:
            if not sys_eeprom.is_valid_tlv(e[tlv_index:]):
                self.log_error("Invalid TLV field starting at EEPROM offset %d" % tlv_index)
                break

            tlv = e[tlv_index:tlv_index + 2 + ord(e[tlv_index + 1])]
            name, value = sys_eeprom.decoder(None, tlv)
            sys_eeprom_dict[name] = value

            if ord(e[tlv_index]) == sys_eeprom._TLV_CODE_QUANTA_CRC or \
                ord(e[tlv_index]) == sys_eeprom._TLV_CODE_CRC_32:
                break
            tlv_index += ord(e[tlv_index+1]) + 2

        return sys_eeprom_dict

    @abstractmethod
    def get_reboot_cause(self):
        """
        Must be implemented in subclass

        Retrieves the cause of the previous reboot

        Returns:
            A tuple (string, string) where the first element is a string
            containing the cause of the previous reboot. This string must be
            one of the predefined strings in this class. If the first string
            is "REBOOT_CAUSE_HARDWARE_OTHER", the second string can be used
            to pass a description of the reboot cause.
        """
        raise NotImplementedError

    @abstractmethod
    def get_voltage_monitor_event(self):
        """
        If has voltage monitor, implemented in subclass

        Returns:
            (bool, dict):
                - bool: True if any voltage point of the mainboard is abnormal
                - dict: {'voltage': {'monitor point':'status info'}}
                    Ex. (True, {'voltage':{'1.8v':'abnormal(1.6v)'}})
                    Indicates that:
                        has event, the 1.8v monitor point is abnormal(1.6v).
        """
        raise NotImplementedError

    def get_change_event(self, timeout=0, scantime=0.5, dev_list=['sfp']):
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
                                                           '3'              Psu power los
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

        while True:
            time.sleep(scantime)

            for dev in dev_list:
                change_event_dict.update({dev:{}})
                self.log_notice('get {} event'.format(dev))
                if dev == 'voltage':
                    (event, dev_change_event_dict) = self.get_voltage_monitor_event()
                    if event:
                        change_event_dict[dev].update(dev_change_event_dict[dev])
                else:
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

    def get_sfp(self, index):
        """
        Return a sfp object by index
        Note: the index from 1, because port_config.ini form Ethernet1
        """
        sfp = None
        index -= 1

        try:
            sfp = self._sfp_list[index]
        except IndexError:
            self.log_error("SFP index {} out of range (0-{})\n".format(
                             index, len(self._sfp_list)-1))

        return sfp
