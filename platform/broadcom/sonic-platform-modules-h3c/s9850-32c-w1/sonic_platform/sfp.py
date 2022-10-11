#!/usr/bin/env python

#############################################################################
#
# Module contains an implementation of SONiC Platform Base API and
# provides the platform information
#
#############################################################################

try:
    import os
    import time
    import struct
    from sonic_platform.sfp_eeprominfo import SfpInfo
except ImportError as e:
    raise ImportError(str(e) + "- required module not found")

try:
    from sonic_daemon_base.daemon_base import Logger
except ImportError as e1:
    from sonic_py_common.logger import Logger
except ImportError as e2:
    print("Failed to import Logger, " + str(e2))
    raise ImportError(str(e2) + " required module not found")

PORT_START = 1
PORT_END   = 32
QSFP_START = 1
QSFP_END   = 32

#definitions of the offset and width for values in DOM info eeprom
QSFP_CHANNEL_RX_LOS_STATUS_OFFSET = 3
QSFP_CHANNEL_RX_LOS_STATUS_WIDTH = 1
QSFP_CHANNEL_TX_FAULT_STATUS_OFFSET = 4
QSFP_CHANNEL_TX_FAULT_STATUS_WIDTH = 1

QSFP_CHANNEL_TX_DISABLE_OFFSET = 86
QSFP_CHANNEL_TX_DISABLE_WIDTH = 1

SYSLOG_IDENTIFIER = 'platfom_sfp'
logger = Logger(SYSLOG_IDENTIFIER)

class Sfp(SfpInfo):
    """
    Platform-specific Sfp class
    """

    def __init__(self, index, cage_type):


        sfp_base_path = '/sys/switch/xcvr/Eth{0}GE{1}/'
        eeprom_path = "/sys/class/i2c-adapter/i2c-0/0-{:0>4x}/eeprom"
        self._index = index + 1

        if self._index < QSFP_START:
            speed = 25
            i2c_path = eeprom_path.format(1 + 2 * (self._index - 1))
        else:
            speed = 100
            i2c_path = eeprom_path.format(self._index + 48)

        self.sfp_base_path = sfp_base_path.format(speed, self._index)
        super(Sfp, self).__init__(cage_type, i2c_path)

        self._old_presence = self.get_presence()

    def eeprom_path_conversion(self, eeprom_path, offset, A):
        """
        For sfp port eeprom path conversion according to odm driver.

        """
        if A == 2:
            #eeprom_path = self.i2c_path().replace('0050', '0051')
            offset += 256

        return eeprom_path, offset

    def get_presence(self):
        """
        Retrieves the presence of the sfp
        """
        if self._index >= QSFP_START:
            presence_ctrl = self.sfp_base_path + 'module_present'
        else:
            presence_ctrl = self.sfp_base_path + 'pre_n'

        try:
            with open(presence_ctrl, 'r') as reg_file:
                content = reg_file.readline().rstrip()
        except IOError:
            return False
        if content == "1":
            return True
        return False


    def get_rx_los(self):
        """
        Retrieves the RX LOS (loss-of-signal) status of SFP


        Returns:
            A list of boolean values, representing the RX LOS status
            of each available channel, value is True if SFP channel
            has RX LOS, False if not.
            E.g., for a tranceiver with four channels: [False, False, True, False]
            Note : RX LOS status is latched until a call to get_rx_los or a reset.
        """
        if not self.get_presence():
            return None

        rx_los_list = []
        if self.is_qsfp():
            rx_los_raw = self.read_eeprom(QSFP_CHANNEL_RX_LOS_STATUS_OFFSET,
                                        QSFP_CHANNEL_RX_LOS_STATUS_WIDTH)
            if rx_los_raw is None:
                return None
            rx_los_data = int(rx_los_raw[0], 16)
            rx_los_list.append(rx_los_data & 0x01 != 0)
            rx_los_list.append(rx_los_data & 0x02 != 0)
            rx_los_list.append(rx_los_data & 0x04 != 0)
            rx_los_list.append(rx_los_data & 0x08 != 0)
        else:
            try:
                with open(self.sfp_base_path + 'rx_los','r') as rx_los:
                    value = rx_los.read().strip()
            except Exception as err:
                logger.log_error(str(err))
                return None
            rx_los_data = True if value == '1' else False
            rx_los_list.append(rx_los_data)

        return rx_los_list

    def get_tx_fault(self):
        """
        Retrieves the TX fault status of SFP


        Returns:
            A list of boolean values, representing the TX fault status
            of each available channel, value is True if SFP channel
            has TX fault, False if not.
            E.g., for a tranceiver with four channels: [False, False, True, False]
            Note : TX fault status is lached until a call to get_tx_fault or a reset.
        """
        if not self.get_presence():
            return None

        tx_fault_list = []
        if self.is_qsfp():
            tx_fault_raw = self.read_eeprom(QSFP_CHANNEL_TX_FAULT_STATUS_OFFSET,
                                        QSFP_CHANNEL_TX_FAULT_STATUS_WIDTH)
            if tx_fault_raw is None:
                return None
            tx_fault_data = int(tx_fault_raw[0], 16)
            tx_fault_list.append(tx_fault_data & 0x01 != 0)
            tx_fault_list.append(tx_fault_data & 0x02 != 0)
            tx_fault_list.append(tx_fault_data & 0x04 != 0)
            tx_fault_list.append(tx_fault_data & 0x08 != 0)
        else:
            try:
                with open(self.sfp_base_path + 'tx_fault', 'r') as tx_fault:
                    value = tx_fault.read().strip()
            except Exception as err:
                logger.log_error(str(err))
                return None
            tx_fault_data = True if value == '1' else False
            tx_fault_list.append(tx_fault_data)

        return tx_fault_list

    def get_tx_disable(self):
        """
        Retrieves the tx_disable status of this SFP

        Returns:
            A list of boolean values, representing the TX disable status
            of each available channel, value is True if SFP channel
            is TX disabled, False if not.
            E.g., for a tranceiver with four channels: [False, False, True, False]
        """
        if not self.get_presence():
            return None

        tx_disable_list = []
        if self.is_qsfp():
            tx_disable_raw = self.read_eeprom(QSFP_CHANNEL_TX_DISABLE_OFFSET,
                                        QSFP_CHANNEL_TX_DISABLE_WIDTH)
            if tx_disable_raw is None:
                return None
            tx_disable_data = int(tx_disable_raw[0], 16)
            tx_disable_list.append(tx_disable_data & 0x01 != 0)
            tx_disable_list.append(tx_disable_data & 0x02 != 0)
            tx_disable_list.append(tx_disable_data & 0x04 != 0)
            tx_disable_list.append(tx_disable_data & 0x08 != 0)
        else:
            try:
                with open(self.sfp_base_path + 'tx_dis', 'r') as tx_dis:
                    value = tx_dis.read().strip()
            except Exception as err:
                logger.log_error(str(err))
                return None
            tx_dis_data = True if value == '1' else False
            tx_disable_list.append(tx_dis_data)

        return tx_disable_list

    def get_reset_status(self):
        reset_status = None
        reset_ctrl = self.sfp_base_path + 'reset'
        try:
            with open(reset_ctrl, 'r') as reg_file:
                reg_hex = reg_file.readline().rstrip()
        except IOError:
            return False
        if reg_hex == '1':
            reset_status = True
        else:
            reset_status = False

        return reset_status

    def set_reset(self, reset):
        """
        Reset SFP and return all user module settings to their default state.

        Args:
            reset: True  ---- set reset
                   False ---- set unreset

        Returns:
            A boolean, True if successful, False if not
        """
        if self._index < QSFP_START or self._index > QSFP_END:
            return False
        reset_file = self.sfp_base_path + "reset"
        val_file = None
        value = "1" if reset == True else "0"
        try:
            with open(reset_file, 'w') as val_file:
                val_file.write(value)
        except IOError:
            return False

        return True

    def tx_disable(self, tx_disable):
        """
        Disable SFP TX for all channels

        Args:
            tx_disable : A Boolean, True to enable tx_disable mode, False to disable
                         tx_disable mode.

        Returns:
            A boolean, True if tx_disable is set successfully, False if not
        """
        if not self.get_presence():
            return False

        tx_disable_list = []
        if self.is_qsfp():
            if not self.dom_tx_disable_supported():
                return False

            if tx_disable:
                value = 0xf
            else:
                value = 0
            rev = self.write_eeprom(QSFP_CHANNEL_TX_DISABLE_OFFSET, [value])
            time.sleep(0.1)
            return rev
        else:
            # if OMD not link the tx_disable pin to a cpld, set reg.
            # A0, reg 110
            # bit7    bit6
            # status  softcontrol
            if not self.dom_tx_disable_supported():
                return False
            if tx_disable:
                value = 1
            else:
                value = 0
            try:
                with open(self.sfp_base_path + 'tx_dis', 'w') as tx_dis:
                    tx_dis.write(str(value))
            except Exception as err:
                logger.log_error(str(err))
                return False
            return True

    def get_lpmode(self):
        setting = 0
                       
        if not self.is_qsfp():
            return False
           
        try:
            with open(self.sfp_base_path + 'lpmode', 'r') as lpmode:
                setting = int(lpmode.read())
        except IOError as err:
            logger.log_error(str(err))
            return False
        except ValueError as err:
            logger.log_error(str(err))
            return False
        
        if setting == 0:
            return False
        else:
            return True
                         
    def set_lpmode(self, setting):
        setting = 0
        if not self.is_qsfp():
            return False
                                                                    
        try:
            with open(self.sfp_base_path + 'lpmode', 'w') as lpmode:
                if setting == True:
                    lpmode.write('1')
                else:
                    lpmode.write('0')
        except IOError as err:
            logger.log_error(str(err))
            return False
        
        return True