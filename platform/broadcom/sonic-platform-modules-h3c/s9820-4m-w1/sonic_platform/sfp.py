#!/usr/bin/env python
"""
Module contains an implementation of SONiC Platform Base API and
provides the Sfp information
"""
try:
    import sys
    import time
    import fcntl
    from vendor_sonic_platform.devcfg import Devcfg
    from vendor_sonic_platform.utils import UtilsHelper
    from sonic_platform import log_wrapper
    from sonic_platform.sfp_common import SfpCommon
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")

QSFPDD_CHANNEL_RX_LOS_STATUS_OFFSET = 147
QSFPDD_CHANNEL_RX_LOS_STATUS_WIDTH = 1
QSFPDD_CHANNEL_RX_LOS_STATUS_PAGE = 0x11
QSFPDD_CHANNEL_TX_FAULT_STATUS_OFFSET = 214
QSFPDD_CHANNEL_TX_FAULT_STATUS_WIDTH = 1
QSFPDD_CHANNEL_TX_FAULT_STATUS_PAGE = 0x10
QSFPDD_CHANNEL_TX_DISABLE_OFFSET = 130
QSFPDD_CHANNEL_TX_DISABLE_WIDTH = 1
QSFPDD_CHANNEL_TX_DISABLE_PAGE = 0x10

class Sfp(SfpCommon):
    """
    Platform-specific Sfp class
    """

    def __init__(self, index, cage_type):
        self._index = index + 1
        log_wrapper.log_init(self)

        slot  = int((self._index - 1) / Devcfg.PORT_NUM_PER_SLOT + 1)
        port = (self._index - 1) % Devcfg.PORT_NUM_PER_SLOT + 1

        self.sfp_base_path = Devcfg.SFP_BASH_PATH.format(slot, port)
        super(Sfp, self).__init__(cage_type, self.sfp_base_path)
        self._old_presence = self.get_presence()

    def is_valid_index(self):
        """
        check if is QSFP
        """
        return self._index >= Devcfg.QSFP_START and self._index <= Devcfg.QSFP_END

    def eeprom_path_conversion(self, eeprom_path, offset, A):
        """
        For sfp port eeprom path conversion according to odm driver.

        """
        if A == 2:
            eeprom_path = self.i2c_path() +'eeprom/dom/dom_raw'
        else:
            eeprom_path = self.i2c_path() +'eeprom/raw'

        return eeprom_path,offset


    def get_optic_temp(self):
        value = 'N/A'
        if not self.get_presence():
            return 'N/A'
        temp_path = self.sfp_base_path + 'eeprom/dom/temperature'
        with open(temp_path, 'rb+') as temp:
            value = temp.read().strip()
            positive_value = value
            if value[0] == '-':
                positive_value = value[1:]
            if not positive_value.isdigit():
                self.logger.log_error("read {}".format(value))
                return 'N/A'
        return float(value) / 1000

    def get_presence(self):
        """
        Retrieves the presence of the sfp
        """

        presence_ctrl = self.sfp_base_path + 'module_present'

        try:
            with open(presence_ctrl, 'r') as reg_file:
                content = reg_file.readline().rstrip()
        except Exception as err:
            self.logger.log_error(str(err))
            return False

        if content == "1":
            return True

        return False

    def get_rx_los(self):
        """
        Retrieves the RX LOS (loss-of-signal) status of SFP
        Implemnted by ODM if the sfp rx_los pin is link to a cpld.
        Otherwise call super.xxx() which is implemnted by common.
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
            rx_los_raw = self.read_eeprom(Devcfg.QSFP_CHANNEL_RX_LOS_STATUS_OFFSET,
                                          Devcfg.QSFP_CHANNEL_RX_LOS_STATUS_WIDTH)
            if rx_los_raw is None:
                return None
            rx_los_data = int(rx_los_raw[0], 16)
            rx_los_list.append(rx_los_data & 0x01 != 0)
            rx_los_list.append(rx_los_data & 0x02 != 0)
            rx_los_list.append(rx_los_data & 0x04 != 0)
            rx_los_list.append(rx_los_data & 0x08 != 0)
        elif self.is_qsfpdd():
            rx_los_raw = self.read_eeprom(
                QSFPDD_CHANNEL_RX_LOS_STATUS_OFFSET,
                QSFPDD_CHANNEL_RX_LOS_STATUS_WIDTH,
                page=QSFPDD_CHANNEL_RX_LOS_STATUS_PAGE)
            if rx_los_raw is None:
                return None
            rx_los_data = int(rx_los_raw[0], 16)
            rx_los_list.append(rx_los_data & 0x01 != 0)
            rx_los_list.append(rx_los_data & 0x02 != 0)
            rx_los_list.append(rx_los_data & 0x04 != 0)
            rx_los_list.append(rx_los_data & 0x08 != 0)
            rx_los_list.append(rx_los_data & 0x10 != 0)
            rx_los_list.append(rx_los_data & 0x20 != 0)
            rx_los_list.append(rx_los_data & 0x40 != 0)
            rx_los_list.append(rx_los_data & 0x80 != 0)
        else:
            try:
                with open(self.sfp_base_path + 'rx_los', 'r') as rx_los:
                    value = rx_los.read().strip()
            except Exception as err:
                self.logger.log_error(str(err))
                return None
            rx_los_data = True if value == '1' else False
            rx_los_list.append(rx_los_data)

        return rx_los_list

    def get_tx_fault(self):
        """
        Retrieves the TX fault status of SFP
        Implemnted by ODM if the sfp tx_fault pin is link to a cpld.
        Otherwise call super.xxx() which is implemnted by common.
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
            tx_fault_raw = self.read_eeprom(Devcfg.QSFP_CHANNEL_TX_FAULT_STATUS_OFFSET,
                                            Devcfg.QSFP_CHANNEL_TX_FAULT_STATUS_WIDTH)
            if tx_fault_raw is None:
                return None
            tx_fault_data = int(tx_fault_raw[0], 16)
            tx_fault_list.append(tx_fault_data & 0x01 != 0)
            tx_fault_list.append(tx_fault_data & 0x02 != 0)
            tx_fault_list.append(tx_fault_data & 0x04 != 0)
            tx_fault_list.append(tx_fault_data & 0x08 != 0)
        elif self.is_qsfpdd():
            tx_fault_raw = self.read_eeprom(QSFPDD_CHANNEL_TX_FAULT_STATUS_OFFSET,
                                            QSFPDD_CHANNEL_TX_FAULT_STATUS_WIDTH, page=QSFPDD_CHANNEL_TX_FAULT_STATUS_PAGE)
            if tx_fault_raw is None:
                return None
            tx_fault_data = int(tx_fault_raw[0], 16)
            tx_fault_list.append(tx_fault_data & 0x01 != 0)
            tx_fault_list.append(tx_fault_data & 0x02 != 0)
            tx_fault_list.append(tx_fault_data & 0x04 != 0)
            tx_fault_list.append(tx_fault_data & 0x08 != 0)
            tx_fault_list.append(tx_fault_data & 0x10 != 0)
            tx_fault_list.append(tx_fault_data & 0x20 != 0)
            tx_fault_list.append(tx_fault_data & 0x40 != 0)
            tx_fault_list.append(tx_fault_data & 0x80 != 0)
        else:
            try:
                with open(self.sfp_base_path + 'tx_fault', 'r') as tx_fault:
                    value = tx_fault.read().strip()
            except Exception as err:
                self.logger.log_error(str(err))
                return None
            tx_fault_data = True if value == '1' else False
            tx_fault_list.append(tx_fault_data)

        return tx_fault_list

    def get_tx_disable(self):
        """
        Retrieves the tx_disable status of this SFP
        Implemnted by ODM if the sfp tx_disable pin is link to a cpld.
        Otherwise call super.xxx() which is implemnted by common.
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
            tx_disable_raw = self.read_eeprom(Devcfg.QSFP_CHANNEL_TX_DISABLE_OFFSET,
                                              Devcfg.QSFP_CHANNEL_TX_DISABLE_WIDTH)
            if tx_disable_raw is None:
                return None
            tx_disable_data = int(tx_disable_raw[0], 16)
            tx_disable_list.append(tx_disable_data & 0x01 != 0)
            tx_disable_list.append(tx_disable_data & 0x02 != 0)
            tx_disable_list.append(tx_disable_data & 0x04 != 0)
            tx_disable_list.append(tx_disable_data & 0x08 != 0)
        elif self.is_qsfpdd():
            tx_disable_raw = self.read_eeprom(QSFPDD_CHANNEL_TX_DISABLE_OFFSET,
                                              QSFPDD_CHANNEL_TX_DISABLE_WIDTH, page=QSFPDD_CHANNEL_TX_DISABLE_PAGE)
            if tx_disable_raw is None:
                return None
            tx_disable_data = int(tx_disable_raw[0], 16)
            tx_disable_list.append(tx_disable_data & 0x01 != 0)
            tx_disable_list.append(tx_disable_data & 0x02 != 0)
            tx_disable_list.append(tx_disable_data & 0x04 != 0)
            tx_disable_list.append(tx_disable_data & 0x08 != 0)
            tx_disable_list.append(tx_disable_data & 0x10 != 0)
            tx_disable_list.append(tx_disable_data & 0x20 != 0)
            tx_disable_list.append(tx_disable_data & 0x40 != 0)
            tx_disable_list.append(tx_disable_data & 0x80 != 0)
        else:
            try:
                with open(self.sfp_base_path + 'tx_dis', 'r') as tx_dis:
                    value = tx_dis.read().strip()
            except Exception as err:
                self.logger.log_error(str(err))
                return None
            tx_dis_data = True if value == '1' else False
            tx_disable_list.append(tx_dis_data)

        return tx_disable_list

    def get_reset_status(self):
        """
        Retrieves the reset status of SFP
        Returns:
            A Boolean, True if reset enabled, False if disabled
        """
        if not self.is_valid_index():
            return False
        reset_status = None
        reset_ctrl = self.sfp_base_path + 'reset'

        try:
            with open(reset_ctrl, 'r') as reg_file:
                reg_hex = reg_file.readline().rstrip()
        except Exception as err:
            self.logger.log_error(str(err))
            return False

        if reg_hex == '1':
            reset_status = False
        else:
            reset_status = True

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
        if not self.is_valid_index():
            return False
        reset_file = self.sfp_base_path + "reset"
        val_file = None
        value = "0" if reset is True else "1"

        try:
            with open(reset_file, 'w') as val_file:
                val_file.write(value.encode())
        except Exception as err:
            self.logger.log_error(str(err))
            return False

        return True

    def tx_disable(self, tx_disable):
        """
        Disable SFP TX for all channels
        Implemnted by ODM if the sfp tx_disable pin is link to a cpld.
        Otherwise call super.xxx() which is implemnted by common.
        Args:
            tx_disable : A Boolean, True to enable tx_disable mode, False to disable
                         tx_disable mode.

        Returns:
            A boolean, True if tx_disable is set successfully, False if not
        """

        if not self.get_presence():
            return False

        if self.is_qsfp():
            if not self.dom_tx_disable_supported():
                return False

            if tx_disable:
                value = 0xf
            else:
                value = 0
            rev = self.write_eeprom(Devcfg.QSFP_CHANNEL_TX_DISABLE_OFFSET, [value])
            time.sleep(0.1)
            return rev
        if self.is_qsfpdd():
            if not self.dom_tx_disable_supported():
                return False

            if tx_disable:
                value = 0xff
            else:
                value = 0
            rev = self.write_eeprom(130, [value], page=0x10)
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
                self.logger.log_error(str(err))
                return False

            return True
