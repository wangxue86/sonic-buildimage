#
# sfp_eeprominfo.py
#
# Abstract base class for implementing a platform-specific class with which
# to interact with a SFP module in SONiC, extended for sfp_base
#

import re
import time
import struct
from sonic_platform_base.sfp_base import SfpBase
from sonic_platform_base.sonic_sfp.sff8472 import sff8472InterfaceId
from sonic_platform_base.sonic_sfp.sff8472 import sff8472Dom
from sonic_platform_base.sonic_sfp.sff8436 import sff8436InterfaceId
from sonic_platform_base.sonic_sfp.sff8436 import sff8436Dom
# from sonic_platform_base.sonic_sfp.qsfp_dd import qsfp_dd_InterfaceId
# from sonic_platform_base.sonic_sfp.qsfp_dd import qsfp_dd_Dom
from abc import ABCMeta, abstractmethod
try:
    from platform import python_version
except ImportError as e:
    #python_version is not supported in python2
    def python_version():
	    return [2,0,0]

try:
    from sonic_daemon_base.daemon_base import Logger
except ImportError as e1:
    from sonic_py_common.logger import Logger
except ImportError as e2:
    raise ImportError(str(e2) + " required module not found")


# definitions of the offset and width for values in XCVR info eeprom
XCVR_SFP_INFO_OFFSET = 0
XCVR_QSFP_INFO_OFFSET = 128
XCVR_INFO_WIDTH = 126
XCVR_INTFACE_BULK_OFFSET = 0
XCVR_INTFACE_BULK_WIDTH_QSFP = 20
XCVR_INTFACE_BULK_WIDTH_SFP = 21
XCVR_TYPE_OFFSET = 0
XCVR_TYPE_WIDTH = 1
XCVR_EXT_TYPE_OFFSET = 1
XCVR_EXT_TYPE_WIDTH = 1
XCVR_CONNECTOR_OFFSET = 2
XCVR_CONNECTOR_WIDTH = 1
XCVR_COMPLIANCE_CODE_OFFSET = 3
XCVR_COMPLIANCE_CODE_WIDTH = 8
XCVR_ENCODING_OFFSET = 11
XCVR_ENCODING_WIDTH = 1
XCVR_NBR_OFFSET = 12
XCVR_NBR_WIDTH = 1
XCVR_EXT_RATE_SEL_OFFSET = 13
XCVR_EXT_RATE_SEL_WIDTH = 1
XCVR_CABLE_LENGTH_OFFSET = 14
XCVR_CABLE_LENGTH_WIDTH_QSFP = 5
XCVR_CABLE_LENGTH_WIDTH_SFP = 6
XCVR_VENDOR_NAME_OFFSET = 20
XCVR_VENDOR_NAME_WIDTH = 16
XCVR_VENDOR_OUI_OFFSET = 37
XCVR_VENDOR_OUI_WIDTH = 3
XCVR_VENDOR_PN_OFFSET = 40
XCVR_VENDOR_PN_WIDTH = 16
XCVR_HW_REV_OFFSET = 56
XCVR_HW_REV_WIDTH_QSFP = 2
XCVR_HW_REV_WIDTH_SFP = 4
XCVR_WAVELENGTH_SFP_OFFSET = 60
XCVR_WAVELENGTH_QSFP_OFFSET = 186
XCVR_WAVELENGTH_WIDTH = 2
XCVR_VENDOR_SN_OFFSET = 68
XCVR_VENDOR_SN_WIDTH = 16
XCVR_VENDOR_DATE_OFFSET = 84
XCVR_VENDOR_DATE_WIDTH = 8
XCVR_DOM_CAPABILITY_OFFSET = 92
XCVR_DOM_CAPABILITY_WIDTH = 2

#definitions of the offset and width for values in DOM info eeprom
QSFP_VERSION_COMPLIANCE_OFFSET = 1
QSFP_VERSION_COMPLIANCE_WIDTH = 2
QSFP_CHANNEL_RX_LOS_STATUS_OFFSET = 3
QSFP_CHANNEL_RX_LOS_STATUS_WIDTH = 1
QSFP_CHANNEL_TX_FAULT_STATUS_OFFSET = 4
QSFP_CHANNEL_TX_FAULT_STATUS_WIDTH = 1
QSFP_DOM_OFFSET = 22
QSFP_DOM_WIDTH = 60
QSFP_TEMPE_OFFSET = 22
QSFP_TEMPE_WIDTH = 2
QSFP_VOLT_OFFSET = 26
QSFP_VOLT_WIDTH = 2
QSFP_CHANNEL_MON_OFFSET = 34
QSFP_CHANNEL_MON_WIDTH = 16
QSFP_CHANNEL_MON_WITH_TX_POWER_WIDTH = 24
QSFP_IDENTIFIER_OFFSET = 128
QSFP_IDENTIFIER_WIDTH = 1
QSFP_CHANNEL_TX_DISABLE_OFFSET = 86
QSFP_CHANNEL_TX_DISABLE_WIDTH = 1
QSFP_POWER_MODE_OFFSET = 93
QSFP_POWER_MODE_WIDTH = 1
QSFP_CONTROL_OFFSET = 86
QSFP_CONTROL_WIDTH = 8

# THRESHOLD page 03h
QSFP_THRESHOLD_OFFSET = 128
QSFP_THRESHOLD_WIDTH = 96
QSFP_MODULE_THRESHOLD_OFFSET = 128
QSFP_MODULE_THRESHOLD_WIDTH = 24
QSFP_CHANNEL_THRESHOLD_OFFSET = 176
QSFP_CHANNEL_THRESHOLD_WIDTH = 24
QSFP_OPTION_VALUE_OFFSET = 192
QSFP_OPTION_VALUE_WIDTH = 4
QSFP_CHANNEL_MON_MASK_OFFSET = 242
QSFP_CHANNEL_MON_MASK_WIDTH = 4

# A0
SFP_DOM_OFFSET = 96
SFP_DOM_WIDTH = 10
SFP_TEMPE_OFFSET = 96
SFP_TEMPE_WIDTH = 2
SFP_VOLT_OFFSET = 98
SFP_VOLT_WIDTH = 2
SFP_CHANNEL_MON_OFFSET = 100
SFP_CHANNEL_MON_WIDTH = 6
SFP_CHANNEL_STATUS_CONTROL_OFFSET = 110
SFP_CHANNEL_STATUS_CONTROL_WIDTH = 1
# A2
SFP_MODULE_THRESHOLD_OFFSET = 0
SFP_MODULE_THRESHOLD_WIDTH = 56

PAGE_BYTE = 128

qsfp_cable_length_tup = ('Length(km)', 'Length OM3(2m)',
                         'Length OM2(m)', 'Length OM1(m)',
                         'Length Cable Assembly(m)')

sfp_cable_length_tup = ('LengthSMFkm-UnitsOfKm', 'LengthSMF(UnitsOf100m)',
                        'Length50um(UnitsOf10m)', 'Length62.5um(UnitsOfm)',
                        'LengthCable(UnitsOfm)', 'LengthOM3(UnitsOf10m)')

sfp_compliance_code_tup = ('10GEthernetComplianceCode', 'InfinibandComplianceCode',
                           'ESCONComplianceCodes', 'SONETComplianceCodes',
                           'EthernetComplianceCodes', 'FibreChannelLinkLength',
                           'FibreChannelTechnology', 'SFP+CableTechnology',
                           'FibreChannelTransmissionMedia', 'FibreChannelSpeed')

qsfp_compliance_code_tup = ('10/40G Ethernet Compliance Code', 'SONET Compliance codes',
                            'SAS/SATA compliance codes', 'Gigabit Ethernet Compliant codes',
                            'Fibre Channel link length/Transmitter Technology',
                            'Fibre Channel transmission media', 'Fibre Channel Speed')

SYSLOG_IDENTIFIER = 'platfom_sfpinfo'
logger = Logger(SYSLOG_IDENTIFIER)

class SfpInfo(SfpBase):
    """
    Abstract base class for interfacing with a SFP module

    Args:
        cage_type: the port cage type SFP or QSFP
        i2c_path: the module i2c path
            Ex. "/sys/bus/i2c/devices/i2c-18/18-"
    """
    __metaclass__ = ABCMeta
    # Device type definition. Note, this is a constant.
    DEVICE_TYPE = "sfp"
    _cage_type = None
    _is_qsfp = None
    _index = None

    def __init__(self, cage_type='SFP', i2c_path=None):
        super(SfpInfo, self).__init__()
        self._cage_type = cage_type
        self._i2c_path = i2c_path
        self._dom_supported = None
        self._dom_temp_supported = None
        self._dom_volt_supported = None
        self._dom_rx_power_supported = None
        self._dom_tx_power_supported = None
        self._calibration = None
        self._qsfp_page3_available = None
        self._dom_tx_disable_supported = None
        self._old_presence = False
        self._start_ms = 0

    def cage_type(self):
        return self._cage_type

    def index(self):
        return self._index

    def get_name(self):
        """
        return device name, sfp{index}
        """
        return 'sfp{}'.format(self._index)

    def i2c_path(self):
        return self._i2c_path

    @abstractmethod
    def eeprom_path_conversion(self, eeprom_path, offset, A):
        """
        For sfp port eeprom path conversion according to odm driver.

        """
        if A == 2:
            #eeprom_path = self.i2c_path().replace('0050', '0051')
            offset += 256

        return eeprom_path, offset

    def read_eeprom(self, offset, num, A=None, page=None):
        """
        For the optoe driven sfp read the eeprom raw.
        Args:
            A: is for SFP, A=0/2
            page: is for QSFP, page=0/1/2/3
            offset:
            num:
        Note:
           ODM modify the eeprom_path according to sfp driver.
        """
        eeprom_raw = []

        if not self.get_presence():
            return None

        for i in range(0, num):
            eeprom_raw.append("0x00")

        (eeprom_path, offset) = self.eeprom_path_conversion(self.i2c_path(), offset, A)

        logger.log_debug('{} read {}, offset={} num={}'.format(self.get_name(), eeprom_path, offset, num))

        try:
            with open(eeprom_path, mode="rb", buffering=0) as eeprom:
                if page is not None:
                    # qsfp page change
                    offset += PAGE_BYTE * page
                eeprom.seek(offset)
                raw = eeprom.read(num)
        except Exception as err:
            logger.log_error(str(err))
            return None

        try:
            if len(raw) == 0:
                return None
            version = int(python_version()[0])
            for n in range(0, num):
                if version >= 3:
                    eeprom_raw[n] = hex(raw[n])[2:].zfill(2)
                else:
                    eeprom_raw[n] = hex(ord(raw[n]))[2:].zfill(2)
        except:
            return None

        return eeprom_raw

    def write_eeprom(self, offset, value_list, A=None, page=None):
        """
        Write sfp/qsfp eeprom
        Args:
            A: is for SFP, A=0/2
            page: is for QSFP, page=00/01/02/03
            offset:
            value_list: hex list
        Note:
            ODM modify the eeprom_path according to sfp driver.
        """
        if not self.get_presence():
            return False

        (eeprom_path, offset) = self.eeprom_path_conversion(self.i2c_path(), offset, A)

        logger.log_debug('{} read {}, offset={} value={}'.format(self.get_name(), eeprom_path, offset, value_list))

        try:
            with open(eeprom_path, "wb+") as eeprom:
                if page is not None:
                    # qsfp page change
                    offset += PAGE_BYTE * page

                eeprom.seek(offset)
                for v in value_list:
                    eeprom.write(struct.pack('B', int(v)))

        except Exception as err:
            logger.log_error(str(err))
            return False

        return True

    def is_qsfp(self):
        """
        Check transceiver is a qsfp module
        """
        if self._is_qsfp is None:
            if self.cage_type() == 'SFP':
                self._is_qsfp = False
            elif self.cage_type() == 'QSFP':
                byte0 = self.read_eeprom(0, 1)
                if byte0 is not None:
                    self._is_qsfp = (byte0 != '03' and byte0 != '0b')
                else:
                    self._is_qsfp = True
            else:
                logger.log_error('Not implemented {} cage type.'.format(self.cage_type()))
                return None

        return self._is_qsfp

    def _xcvr_capability_clean(self):
        """
        Clean the transceiver capability when transceiver is not presence
        Can be invoked by odm's get_presence() when presence is False
        """
        self._dom_supported = None
        self._dom_temp_supported = None
        self._dom_volt_supported = None
        self._dom_rx_power_supported = None
        self._dom_tx_power_supported = None
        self._calibration = None
        self._qsfp_page3_available = None
        self._dom_tx_disable_supported = None
        self._old_presence = False

    def _xcvr_capability_detect(self):
        self._dom_supported = False
        self._dom_temp_supported = False
        self._dom_volt_supported = False
        self._dom_rx_power_supported = False
        self._dom_tx_power_supported = False
        self._calibration = 0
        self._qsfp_page3_available = False
        self._dom_tx_disable_supported = False
        if self.is_qsfp():
            self._calibration = 1
            sfpi_obj = sff8436InterfaceId()
            if sfpi_obj is None:
                self._dom_supported = False
                return None

            # QSFP capability byte parse, through this byte can know whether it support tx_power or not.
            # Todo: in the future when decided to migrate to support SFF-8636 instead of SFF-8436,
            # need to add more code for determining the capability and version compliance
            # in SFF-8636 dom capability definitions evolving with the versions.
            qsfp_dom_capability_raw = self.read_eeprom(XCVR_QSFP_INFO_OFFSET + XCVR_DOM_CAPABILITY_OFFSET,
                                                       XCVR_DOM_CAPABILITY_WIDTH)
            if qsfp_dom_capability_raw is None:
                return None

            qsfp_version_compliance_raw = self.read_eeprom(QSFP_VERSION_COMPLIANCE_OFFSET,
                                                           QSFP_VERSION_COMPLIANCE_WIDTH)
            if qsfp_version_compliance_raw is None:
                return None
            qsfp_version_compliance = int(qsfp_version_compliance_raw[0], 16)
            if 'parse_dom_capability' in dir(sfpi_obj):
			    dom_capability = sfpi_obj.parse_dom_capability(qsfp_dom_capability_raw, 0)
            else:
			    return None
            if qsfp_version_compliance >= 0x08:
                self._dom_temp_supported = dom_capability['data']['Temp_support']['value'] == 'On'
                self._dom_volt_supported = dom_capability['data']['Voltage_support']['value'] == 'On'
                self._dom_rx_power_supported = dom_capability['data']['Rx_power_support']['value'] == 'On'
                self._dom_tx_power_supported = dom_capability['data']['Tx_power_support']['value'] == 'On'
            else:
                self._dom_temp_supported = True
                self._dom_volt_supported = True
                self._dom_rx_power_supported = dom_capability['data']['Rx_power_support']['value'] == 'On'
                self._dom_tx_power_supported = True

            self._dom_supported = True
            self._calibration = 1
            sfpd_obj = sff8436Dom(calibration_type=self.get_calibration())
            if sfpd_obj is None:
                return None
            qsfp_option_value_raw = self.read_eeprom(QSFP_OPTION_VALUE_OFFSET,
                                                     QSFP_OPTION_VALUE_WIDTH)
            if qsfp_option_value_raw is not None:
                optional_capability = sfpd_obj.parse_option_params(qsfp_option_value_raw, 0)
                self._dom_tx_disable_supported = optional_capability['data']['TxDisable']['value'] == 'On'
            dom_status_indicator = sfpd_obj.parse_dom_status_indicator(qsfp_version_compliance_raw, 1)
            self._qsfp_page3_available = dom_status_indicator['data']['FlatMem']['value'] == 'Off'

        else:
            sfpi_obj = sff8472InterfaceId()
            if sfpi_obj is None:
                return None

            sfp_dom_capability_raw = self.read_eeprom(XCVR_DOM_CAPABILITY_OFFSET,
                                                      XCVR_DOM_CAPABILITY_WIDTH)
            if sfp_dom_capability_raw is None:
                return None

            sfp_dom_capability = int(sfp_dom_capability_raw[0], 16)
            self._dom_supported = (sfp_dom_capability & 0x40 != 0)
            if self._dom_supported:
                self._dom_temp_supported = True
                self._dom_volt_supported = True
                self._dom_rx_power_supported = True
                self._dom_tx_power_supported = True
                if sfp_dom_capability & 0x20 != 0:
                    self._calibration = 1
                elif sfp_dom_capability & 0x10 != 0:
                    self._calibration = 2
                else:
                    self._calibration = 0

            self._dom_tx_disable_supported = (int(sfp_dom_capability_raw[1], 16) & 0x40 != 0)

    def dom_supported(self):
        """
        Check transceiver is support diagnosis
        """
        if self._dom_supported is not None:
            return self._dom_supported

        self._xcvr_capability_detect()

        return self._dom_supported

    def dom_temp_supported(self):
        """
        Check transceiver is support temperature diagnosis
        """
        if self._dom_temp_supported is not None:
            return self._dom_temp_supported

        self._xcvr_capability_detect()

        return self._dom_temp_supported

    def dom_volt_supported(self):
        """
        Check transceiver is support voltage diagnosis
        """
        if self._dom_volt_supported is not None:
            return self._dom_volt_supported

        self._xcvr_capability_detect()

        return self._dom_volt_supported

    def dom_rx_power_supported(self):
        """
        Check transceiver is support rx power diagnosis
        """
        if self._dom_rx_power_supported is not None:
            return self._dom_rx_power_supported

        self._xcvr_capability_detect()

        return self._dom_rx_power_supported

    def dom_tx_power_supported(self):
        """
        Check transceiver is support tx power diagnosis
        """
        if self._dom_tx_power_supported is not None:
            return self._dom_tx_power_supported

        self._xcvr_capability_detect()

        return self._dom_tx_power_supported

    def dom_tx_disable_supported(self):
        """
        Check transceiver is support diagnosis
        """
        if self._dom_supported is not None:
            return self._dom_supported

        self._xcvr_capability_detect()

        return self._dom_supported

    def get_calibration(self):
        """
        Check transceiver is support diagnosis
        """
        if self._calibration is not None:
            return self._calibration

        self._xcvr_capability_detect()

        return self._calibration

    def qsfp_page3_available(self):
        """
        Check transceiver is support diagnosis
        """
        if self._qsfp_page3_available is not None:
            return self._qsfp_page3_available

        self._xcvr_capability_detect()

        return self._qsfp_page3_available

    def get_transceiver_info(self):
        """
        Retrieves transceiver info of this SFP

        Returns:
            A dict which contains following keys/values :
        ========================================================================
        keys                       |Value Format   |Information
        ---------------------------|---------------|----------------------------
        type                       |1*255VCHAR     |type of SFP
        hardware_rev               |1*255VCHAR     |hardware version of SFP
        serial                     |1*255VCHAR     |serial number of the SFP
        manufacturer               |1*255VCHAR     |SFP vendor name
        model                  |1*255VCHAR     |SFP model name
        connector                  |1*255VCHAR     |connector information
        encoding                   |1*255VCHAR     |encoding information
        ext_identifier             |1*255VCHAR     |extend identifier
        ext_rateselect_compliance  |1*255VCHAR     |extended rateSelect compliance
        cable_length               |INT            |cable length in m
        mominal_bit_rate           |INT            |nominal bit rate by 100Mbs
        specification_compliance   |1*255VCHAR     |specification compliance
        vendor_date                |1*255VCHAR     |vendor date
        vendor_oui                 |1*255VCHAR     |vendor OUI
        ========================================================================
        """
        xcvr_info_keys = [
            'type', 'hardware_rev', 'serial', 'manufacturer',
            'model', 'connector', 'encoding', 'ext_identifier', 'cable_length', 'cable_type',
            'ext_rateselect_compliance', 'nominal_bit_rate', 'specification_compliance',
            'vendor_date', 'vendor_oui', 'application_advertisement',
            'wavelength'
        ]
        transceiver_info_dict = dict.fromkeys( xcvr_info_keys, 'N/A' )
        compliance_code_dict = {}

        if not self.get_presence():
            self._xcvr_capability_clean()
            return None

        if self.is_qsfp():
            offset = XCVR_QSFP_INFO_OFFSET
            vendor_rev_width = XCVR_HW_REV_WIDTH_QSFP
            interface_bulk_width = XCVR_INTFACE_BULK_WIDTH_QSFP
            wavelength_offset = XCVR_WAVELENGTH_QSFP_OFFSET
            sfp_type = 'QSFP'
            sfpi_obj = sff8436InterfaceId()
        else:
            offset = XCVR_SFP_INFO_OFFSET
            vendor_rev_width = XCVR_HW_REV_WIDTH_SFP
            interface_bulk_width = XCVR_INTFACE_BULK_WIDTH_SFP
            wavelength_offset = XCVR_WAVELENGTH_SFP_OFFSET
            sfp_type = 'SFP'

            sfpi_obj = sff8472InterfaceId()

        if sfpi_obj is None:
          #  logger.log_error( 'Error: sfp_object open failed' )
            return None

        xcvr_info_raw = self.read_eeprom(offset, XCVR_INFO_WIDTH)
        if xcvr_info_raw is None:
            return None

        sfp_interface_bulk_data = sfpi_obj.parse_sfp_info_bulk(
            xcvr_info_raw[XCVR_INTFACE_BULK_OFFSET:interface_bulk_width], 0 )
        sfp_vendor_name_data = sfpi_obj.parse_vendor_name(

            xcvr_info_raw[XCVR_VENDOR_NAME_OFFSET:XCVR_VENDOR_NAME_OFFSET + XCVR_VENDOR_NAME_WIDTH], 0)
        sfp_vendor_pn_data = sfpi_obj.parse_vendor_pn(
            xcvr_info_raw[XCVR_VENDOR_PN_OFFSET:XCVR_VENDOR_PN_OFFSET + XCVR_VENDOR_PN_WIDTH], 0)
        sfp_vendor_rev_data = sfpi_obj.parse_vendor_rev(
            xcvr_info_raw[XCVR_HW_REV_OFFSET:XCVR_HW_REV_OFFSET + vendor_rev_width], 0)
        sfp_vendor_sn_data = sfpi_obj.parse_vendor_sn(
            xcvr_info_raw[XCVR_VENDOR_SN_OFFSET:XCVR_VENDOR_SN_OFFSET + XCVR_VENDOR_SN_WIDTH], 0)
        sfp_vendor_oui_data = sfpi_obj.parse_vendor_oui(
            xcvr_info_raw[XCVR_VENDOR_OUI_OFFSET:XCVR_VENDOR_OUI_OFFSET + XCVR_VENDOR_OUI_WIDTH], 0)
        sfp_vendor_date_data = sfpi_obj.parse_vendor_date(
            xcvr_info_raw[XCVR_VENDOR_DATE_OFFSET:XCVR_VENDOR_DATE_OFFSET + XCVR_VENDOR_DATE_WIDTH], 0)

        transceiver_info_dict['type'] = sfp_interface_bulk_data['data']['type']['value']
        transceiver_info_dict['type_abbrv_name'] = sfp_interface_bulk_data['data']['type_abbrv_name']['value']
        transceiver_info_dict['manufacturer'] = sfp_vendor_name_data['data']['Vendor Name']['value']
        transceiver_info_dict['model'] = sfp_vendor_pn_data['data']['Vendor PN']['value']
        transceiver_info_dict['hardware_rev'] = sfp_vendor_rev_data['data']['Vendor Rev']['value']
        transceiver_info_dict['serial'] = sfp_vendor_sn_data['data']['Vendor SN']['value']
        transceiver_info_dict['vendor_oui'] = sfp_vendor_oui_data['data']['Vendor OUI']['value']
        transceiver_info_dict['vendor_date'] = sfp_vendor_date_data['data']['VendorDataCode(YYYY-MM-DD Lot)'][
            'value']
        transceiver_info_dict['connector'] = sfp_interface_bulk_data['data']['Connector']['value']
        transceiver_info_dict['encoding'] = sfp_interface_bulk_data['data']['EncodingCodes']['value']
        transceiver_info_dict['ext_identifier'] = sfp_interface_bulk_data['data']['Extended Identifier']['value']
        transceiver_info_dict['ext_rateselect_compliance'] = sfp_interface_bulk_data['data']['RateIdentifier'][
            'value']

        transceiver_info_dict['application_advertisement'] = 'N/A'
        transceiver_info_dict['vendor_rev'] = sfp_vendor_rev_data['data']['Vendor Rev']['value']

        wavelength_raw = xcvr_info_raw[wavelength_offset -
                                       XCVR_INFO_WIDTH:wavelength_offset - XCVR_INFO_WIDTH + XCVR_WAVELENGTH_WIDTH]
        if self.is_qsfp():
            for i, key in enumerate( qsfp_cable_length_tup ):
                if key in sfp_interface_bulk_data['data'] and sfp_interface_bulk_data['data'][key]['value']:
                    transceiver_info_dict['cable_type{}'.format( i )] = key
                    transceiver_info_dict['cable_length{}'.format( i )] = str(
                        sfp_interface_bulk_data['data'][key]['value'] )

            for key in qsfp_compliance_code_tup:
                if key in sfp_interface_bulk_data['data']['Specification compliance']['value']:
                    compliance_code_dict[key] = \
                    sfp_interface_bulk_data['data']['Specification compliance']['value'][key]['value']
                transceiver_info_dict['specification_compliance'] = str( compliance_code_dict )

                transceiver_info_dict['nominal_bit_rate'] = str(
                    sfp_interface_bulk_data['data']['Nominal Bit Rate(100Mbs)']['value'] )

            transceiver_info_dict['wavelength'] = int( wavelength_raw[0], 16 ) / 20
        else:
            for i, key in enumerate( sfp_cable_length_tup ):
                if key in sfp_interface_bulk_data['data'] and sfp_interface_bulk_data['data'][key]['value']:
                    transceiver_info_dict['cable_type{}'.format( i )] = key
                    transceiver_info_dict['cable_length{}'.format( i )] = str(
                        sfp_interface_bulk_data['data'][key]['value'] )

            for key in sfp_compliance_code_tup:
                if key in sfp_interface_bulk_data['data']['Specification compliance']['value']:
                    compliance_code_dict[key] = \
                    sfp_interface_bulk_data['data']['Specification compliance']['value'][key]['value']
            transceiver_info_dict['specification_compliance'] = str( compliance_code_dict )

            transceiver_info_dict['nominal_bit_rate'] = str(
                sfp_interface_bulk_data['data']['NominalSignallingRate(UnitsOf100Mbd)']['value'] )

            transceiver_info_dict['wavelength'] = int( wavelength_raw[0], 16 )

        return transceiver_info_dict


    def get_transceiver_bulk_status(self):
        """
        Retrieves transceiver bulk status of this SFP

        Returns:
            A dict which contains following keys/values :
        ========================================================================
        keys                       |Value Format   |Information
        ---------------------------|---------------|----------------------------
        rx_los                     |BOOLEAN        |RX loss-of-signal status, True if has RX los, False if not.
        tx_fault                   |BOOLEAN        |TX fault status, True if has TX fault, False if not.
        reset_status               |BOOLEAN        |reset status, True if SFP in reset, False if not.
        lp_mode                    |BOOLEAN        |low power mode status, True in lp mode, False if not.
        tx_disable                 |BOOLEAN        |TX disable status, True TX disabled, False if not.
        tx_disabled_channel        |HEX            |disabled TX channels in hex, bits 0 to 3 represent channel 0
                                   |               |to channel 3.
        temperature                |INT            |module temperature in Celsius
        voltage                    |INT            |supply voltage in mV
        tx<n>bias                  |INT            |TX Bias Current in mA, n is the channel number,
                                   |               |for example, tx2bias stands for tx bias of channel 2.
        rx<n>power                 |INT            |received optical power in mW, n is the channel number,
                                   |               |for example, rx2power stands for rx power of channel 2.
        tx<n>power                 |INT            |TX output power in mW, n is the channel number,
                                   |               |for example, tx2power stands for tx power of channel 2.
        ========================================================================
        """

        dom_dict_keys = [
            'rx_los', 'tx_fault', 'reset_status', 'lp_mode',
            'tx_disable', 'tx_disabled_channel', 'temperature', 'voltage',
            'rx1power', 'rx2power', 'rx3power', 'rx4power',
            'tx1bias', 'tx2bias', 'tx3bias', 'tx4bias',
            'tx1power', 'tx2power', 'tx3power', 'tx4power']
        transceiver_dom_info_dict = dict.fromkeys(dom_dict_keys, 'N/A')

        if not self.get_presence():
            self._xcvr_capability_clean()
            return transceiver_dom_info_dict

        if not self.dom_supported():
            return transceiver_dom_info_dict

        if self.is_qsfp():
            xcvr_dom_info_dict = self._get_qsfp_transceiver_bulk_status()
        else:
            xcvr_dom_info_dict = self._get_sfp_transceiver_bulk_status()

        if xcvr_dom_info_dict:
            transceiver_dom_info_dict.update(xcvr_dom_info_dict)
            transceiver_dom_info_dict['rx_los'] = self.get_rx_los()
            transceiver_dom_info_dict['tx_fault'] = self.get_tx_fault()
            transceiver_dom_info_dict['reset_status'] = self.get_reset_status()
            transceiver_dom_info_dict['lp_mode'] = self.get_lpmode()
            transceiver_dom_info_dict['tx_disable'] = self.get_tx_disable()
            transceiver_dom_info_dict['tx_disable_channel'] = self.get_tx_disable_channel()

        return transceiver_dom_info_dict

    def _get_qsfp_transceiver_bulk_status(self):
        transceiver_dom_info_dict = {}

        sfpd_obj = sff8436Dom(calibration_type=self.get_calibration())
        if sfpd_obj is None:
            logger.log_error('Error: sfpd_object open failed')
            return None

        sfpi_obj = sff8436InterfaceId()
        if sfpi_obj is None:
            logger.log_error('Error: sfpi_object open failed')
            return None

        #
        # QSFP capability byte parse, through this byte can know whether it support tx_power or not.
        # Todo: in the future when decided to migrate to support SFF-8636 instead of SFF-8436,
        # need to add more code for determining the capability and version compliance
        # in SFF-8636 dom capability definitions evolving with the versions.
        qsfp_dom_capability_raw = self.read_eeprom(XCVR_QSFP_INFO_OFFSET + XCVR_DOM_CAPABILITY_OFFSET,
                                                   XCVR_DOM_CAPABILITY_WIDTH)
        if qsfp_dom_capability_raw is not None:
            qspf_dom_capability_data = sfpi_obj.parse_dom_capability(qsfp_dom_capability_raw, 0)
        else:
            return None

        qsfp_dom_raw = self.read_eeprom(QSFP_DOM_OFFSET, QSFP_DOM_WIDTH)
        if qsfp_dom_raw is None:
            return None

        qsfp_temp_raw = qsfp_dom_raw[QSFP_TEMPE_OFFSET -
                                     QSFP_DOM_OFFSET:QSFP_TEMPE_OFFSET - QSFP_DOM_OFFSET + QSFP_TEMPE_WIDTH]
        qsfp_volt_raw = qsfp_dom_raw[QSFP_VOLT_OFFSET -
                                     QSFP_DOM_OFFSET:QSFP_VOLT_OFFSET - QSFP_DOM_OFFSET + QSFP_VOLT_WIDTH]
        qsfp_channel_raw = qsfp_dom_raw[QSFP_CHANNEL_MON_OFFSET -
                                        QSFP_DOM_OFFSET:QSFP_CHANNEL_MON_OFFSET -
                                        QSFP_DOM_OFFSET +
                                        QSFP_CHANNEL_MON_WITH_TX_POWER_WIDTH]

        dom_temperature_data = sfpd_obj.parse_temperature(qsfp_temp_raw, 0)
        dom_voltage_data = sfpd_obj.parse_voltage(qsfp_volt_raw, 0)
        dom_channel_monitor_data = sfpd_obj.parse_channel_monitor_params_with_tx_power(qsfp_channel_raw, 0)

        transceiver_dom_info_dict['temperature'] = dom_temperature_data['data']['Temperature']['value']
        transceiver_dom_info_dict['voltage'] = dom_voltage_data['data']['Vcc']['value']

        if self.dom_tx_power_supported:
            transceiver_dom_info_dict['tx1power'] = dom_channel_monitor_data['data']['TX1Power']['value']
            transceiver_dom_info_dict['tx2power'] = dom_channel_monitor_data['data']['TX2Power']['value']
            transceiver_dom_info_dict['tx3power'] = dom_channel_monitor_data['data']['TX3Power']['value']
            transceiver_dom_info_dict['tx4power'] = dom_channel_monitor_data['data']['TX4Power']['value']

        if self.dom_rx_power_supported:
            transceiver_dom_info_dict['rx1power'] = dom_channel_monitor_data['data']['RX1Power']['value']
            transceiver_dom_info_dict['rx2power'] = dom_channel_monitor_data['data']['RX2Power']['value']
            transceiver_dom_info_dict['rx3power'] = dom_channel_monitor_data['data']['RX3Power']['value']
            transceiver_dom_info_dict['rx4power'] = dom_channel_monitor_data['data']['RX4Power']['value']

        transceiver_dom_info_dict['tx1bias'] = dom_channel_monitor_data['data']['TX1Bias']['value']
        transceiver_dom_info_dict['tx2bias'] = dom_channel_monitor_data['data']['TX2Bias']['value']
        transceiver_dom_info_dict['tx3bias'] = dom_channel_monitor_data['data']['TX3Bias']['value']
        transceiver_dom_info_dict['tx4bias'] = dom_channel_monitor_data['data']['TX4Bias']['value']

        return transceiver_dom_info_dict

    def _get_sfp_transceiver_bulk_status(self):
        transceiver_dom_info_dict = {}

        sfpd_obj = sff8472Dom(calibration_type=self.get_calibration())
        if sfpd_obj is None:
            logger.log_error('Error: sfpd_object open failed')
            return None

        dom_raw = self.read_eeprom(SFP_DOM_OFFSET, SFP_DOM_WIDTH, A=2)
        if dom_raw is None:
            return None

        dom_temp_raw = dom_raw[SFP_TEMPE_OFFSET - SFP_DOM_OFFSET:SFP_TEMPE_OFFSET - SFP_DOM_OFFSET + SFP_TEMPE_WIDTH]
        dom_volt_raw = dom_raw[SFP_VOLT_OFFSET - SFP_DOM_OFFSET:SFP_VOLT_OFFSET - SFP_DOM_OFFSET + SFP_VOLT_WIDTH]
        dom_channel_raw = dom_raw[SFP_CHANNEL_MON_OFFSET - SFP_DOM_OFFSET:SFP_CHANNEL_MON_OFFSET - SFP_DOM_OFFSET + SFP_CHANNEL_MON_WIDTH]

        dom_temperature_data = sfpd_obj.parse_temperature(dom_temp_raw, 0)
        dom_voltage_data = sfpd_obj.parse_voltage(dom_volt_raw, 0)
        dom_channel_monitor_data = sfpd_obj.parse_channel_monitor_params(dom_channel_raw, 0)

        transceiver_dom_info_dict['temperature'] = dom_temperature_data['data']['Temperature']['value']
        transceiver_dom_info_dict['voltage'] = dom_voltage_data['data']['Vcc']['value']
        transceiver_dom_info_dict['rx1power'] = dom_channel_monitor_data['data']['RXPower']['value']
        transceiver_dom_info_dict['tx1bias'] = dom_channel_monitor_data['data']['TXBias']['value']
        transceiver_dom_info_dict['tx1power'] = dom_channel_monitor_data['data']['TXPower']['value']

        return transceiver_dom_info_dict

    def get_transceiver_threshold_info(self):
        """
        Retrieves transceiver threshold info of this SFP

        Returns:
            A dict which contains following keys/values :
        ========================================================================
        keys                       |Value Format   |Information
        ---------------------------|---------------|----------------------------
        temphighalarm              |FLOAT          |High Alarm Threshold value of temperature in Celsius.
        templowalarm               |FLOAT          |Low Alarm Threshold value of temperature in Celsius.
        temphighwarning            |FLOAT          |High Warning Threshold value of temperature in Celsius.
        templowwarning             |FLOAT          |Low Warning Threshold value of temperature in Celsius.
        vcchighalarm               |FLOAT          |High Alarm Threshold value of supply voltage in mV.
        vcclowalarm                |FLOAT          |Low Alarm Threshold value of supply voltage in mV.
        vcchighwarning             |FLOAT          |High Warning Threshold value of supply voltage in mV.
        vcclowwarning              |FLOAT          |Low Warning Threshold value of supply voltage in mV.
        rxpowerhighalarm           |FLOAT          |High Alarm Threshold value of received power in dBm.
        rxpowerlowalarm            |FLOAT          |Low Alarm Threshold value of received power in dBm.
        rxpowerhighwarning         |FLOAT          |High Warning Threshold value of received power in dBm.
        rxpowerlowwarning          |FLOAT          |Low Warning Threshold value of received power in dBm.
        txpowerhighalarm           |FLOAT          |High Alarm Threshold value of transmit power in dBm.
        txpowerlowalarm            |FLOAT          |Low Alarm Threshold value of transmit power in dBm.
        txpowerhighwarning         |FLOAT          |High Warning Threshold value of transmit power in dBm.
        txpowerlowwarning          |FLOAT          |Low Warning Threshold value of transmit power in dBm.
        txbiashighalarm            |FLOAT          |High Alarm Threshold value of tx Bias Current in mA.
        txbiaslowalarm             |FLOAT          |Low Alarm Threshold value of tx Bias Current in mA.
        txbiashighwarning          |FLOAT          |High Warning Threshold value of tx Bias Current in mA.
        txbiaslowwarning           |FLOAT          |Low Warning Threshold value of tx Bias Current in mA.
        ========================================================================
        """
        dom_dict_keys = [
            'temphighalarm', 'temphighwarning', 'templowalarm', 'templowwarning',
            'vcchighalarm', 'vcchighwarning', 'vcclowalarm', 'vcclowwarning',
            'rxpowerhighalarm', 'rxpowerhighwarning', 'rxpowerlowalarm', 'rxpowerlowwarning',
            'txpowerhighalarm', 'txpowerhighwarning', 'txpowerlowalarm', 'txpowerlowwarning',
            'txbiashighalarm', 'txbiashighwarning', 'txbiaslowalarm', 'txbiaslowwarning']
        transceiver_threshold_info_dict = dict.fromkeys(dom_dict_keys, 'N/A')

        if not self.get_presence():
            self._xcvr_capability_clean()
            return transceiver_threshold_info_dict

        if self.is_qsfp():
            transceiver_threshold_info_dict.update(self._get_qsfp_transceiver_threshold_info())
        else:
            transceiver_threshold_info_dict.update(self._get_sfp_transceiver_threshold_info())

        return transceiver_threshold_info_dict

    def _get_qsfp_transceiver_threshold_info(self):
        transceiver_threshold_info_dict = {}

        if not self.dom_supported() or not self.qsfp_page3_available():
            return {}
        sfpd_obj = sff8436Dom(calibration_type=self.get_calibration())
        if sfpd_obj is None:
            return {}

        dom_module_threshold_raw = self.read_eeprom(QSFP_MODULE_THRESHOLD_OFFSET,
                                                    QSFP_MODULE_THRESHOLD_WIDTH,
                                                    page=3)
        if dom_module_threshold_raw is None:
            return {}

        dom_channel_threshold_raw = self.read_eeprom(QSFP_CHANNEL_THRESHOLD_OFFSET,
                                                     QSFP_CHANNEL_THRESHOLD_WIDTH,
                                                     page=3)
        if dom_channel_threshold_raw is None:
            return {}

        dom_module_threshold_data = sfpd_obj.parse_module_threshold_values(dom_module_threshold_raw, 0)
        dom_channel_threshold_data = sfpd_obj.parse_channel_threshold_values(dom_channel_threshold_raw, 0)

        # Threshold Data
        transceiver_threshold_info_dict['temphighalarm'] = dom_module_threshold_data['data']['TempHighAlarm']['value']
        transceiver_threshold_info_dict['temphighwarning'] = dom_module_threshold_data['data']['TempHighWarning']['value']
        transceiver_threshold_info_dict['templowalarm'] = dom_module_threshold_data['data']['TempLowAlarm']['value']
        transceiver_threshold_info_dict['templowwarning'] = dom_module_threshold_data['data']['TempLowWarning']['value']
        transceiver_threshold_info_dict['vcchighalarm'] = dom_module_threshold_data['data']['VccHighAlarm']['value']
        transceiver_threshold_info_dict['vcchighwarning'] = dom_module_threshold_data['data']['VccHighWarning']['value']
        transceiver_threshold_info_dict['vcclowalarm'] = dom_module_threshold_data['data']['VccLowAlarm']['value']
        transceiver_threshold_info_dict['vcclowwarning'] = dom_module_threshold_data['data']['VccLowWarning']['value']
        transceiver_threshold_info_dict['rxpowerhighalarm'] = dom_channel_threshold_data['data']['RxPowerHighAlarm']['value']
        transceiver_threshold_info_dict['rxpowerhighwarning'] = dom_channel_threshold_data['data']['RxPowerHighWarning']['value']
        transceiver_threshold_info_dict['rxpowerlowalarm'] = dom_channel_threshold_data['data']['RxPowerLowAlarm']['value']
        transceiver_threshold_info_dict['rxpowerlowwarning'] = dom_channel_threshold_data['data']['RxPowerLowWarning']['value']
        transceiver_threshold_info_dict['txbiashighalarm'] = dom_channel_threshold_data['data']['TxBiasHighAlarm']['value']
        transceiver_threshold_info_dict['txbiashighwarning'] = dom_channel_threshold_data['data']['TxBiasHighWarning']['value']
        transceiver_threshold_info_dict['txbiaslowalarm'] = dom_channel_threshold_data['data']['TxBiasLowAlarm']['value']
        transceiver_threshold_info_dict['txbiaslowwarning'] = dom_channel_threshold_data['data']['TxBiasLowWarning']['value']
        transceiver_threshold_info_dict['txpowerhighalarm'] = dom_channel_threshold_data['data']['TxPowerHighAlarm']['value']
        transceiver_threshold_info_dict['txpowerhighwarning'] = dom_channel_threshold_data['data']['TxPowerHighWarning']['value']
        transceiver_threshold_info_dict['txpowerlowalarm'] = dom_channel_threshold_data['data']['TxPowerLowAlarm']['value']
        transceiver_threshold_info_dict['txpowerlowwarning'] = dom_channel_threshold_data['data']['TxPowerLowWarning']['value']

        return transceiver_threshold_info_dict

    def _get_sfp_transceiver_threshold_info(self):
        transceiver_threshold_info_dict = {}
        if not self.dom_supported():
            return {}

        sfpd_obj = sff8472Dom(calibration_type=self.get_calibration())
        if sfpd_obj is None:
            return {}

        dom_module_threshold_raw = self.read_eeprom(SFP_MODULE_THRESHOLD_OFFSET,
                                                    SFP_MODULE_THRESHOLD_WIDTH,
                                                    A=2)
        if dom_module_threshold_raw is None:
            return {}

        dom_module_threshold_data = sfpd_obj.parse_alarm_warning_threshold(dom_module_threshold_raw, 0)

        # Threshold Data
        transceiver_threshold_info_dict['temphighalarm'] = dom_module_threshold_data['data']['TempHighAlarm']['value']
        transceiver_threshold_info_dict['templowalarm'] = dom_module_threshold_data['data']['TempLowAlarm']['value']
        transceiver_threshold_info_dict['temphighwarning'] = dom_module_threshold_data['data']['TempHighWarning']['value']
        transceiver_threshold_info_dict['templowwarning'] = dom_module_threshold_data['data']['TempLowWarning']['value']
        transceiver_threshold_info_dict['vcchighalarm'] = dom_module_threshold_data['data']['VoltageHighAlarm']['value']
        transceiver_threshold_info_dict['vcclowalarm'] = dom_module_threshold_data['data']['VoltageLowAlarm']['value']
        transceiver_threshold_info_dict['vcchighwarning'] = dom_module_threshold_data['data']['VoltageHighWarning']['value']
        transceiver_threshold_info_dict['vcclowwarning'] = dom_module_threshold_data['data']['VoltageLowWarning']['value']
        transceiver_threshold_info_dict['txbiashighalarm'] = dom_module_threshold_data['data']['BiasHighAlarm']['value']
        transceiver_threshold_info_dict['txbiaslowalarm'] = dom_module_threshold_data['data']['BiasLowAlarm']['value']
        transceiver_threshold_info_dict['txbiashighwarning'] = dom_module_threshold_data['data']['BiasHighWarning']['value']
        transceiver_threshold_info_dict['txbiaslowwarning'] = dom_module_threshold_data['data']['BiasLowWarning']['value']
        transceiver_threshold_info_dict['txpowerhighalarm'] = dom_module_threshold_data['data']['TXPowerHighAlarm']['value']
        transceiver_threshold_info_dict['txpowerlowalarm'] = dom_module_threshold_data['data']['TXPowerLowAlarm']['value']
        transceiver_threshold_info_dict['txpowerhighwarning'] = dom_module_threshold_data['data']['TXPowerHighWarning']['value']
        transceiver_threshold_info_dict['txpowerlowwarning'] = dom_module_threshold_data['data']['TXPowerLowWarning']['value']
        transceiver_threshold_info_dict['rxpowerhighalarm'] = dom_module_threshold_data['data']['RXPowerHighAlarm']['value']
        transceiver_threshold_info_dict['rxpowerlowalarm'] = dom_module_threshold_data['data']['RXPowerLowAlarm']['value']
        transceiver_threshold_info_dict['rxpowerhighwarning'] = dom_module_threshold_data['data']['RXPowerHighWarning']['value']
        transceiver_threshold_info_dict['rxpowerlowwarning'] = dom_module_threshold_data['data']['RXPowerLowWarning']['value']

        return transceiver_threshold_info_dict

    def get_status(self):
        """
        Now just return precence status
        """
        return self.get_presence()

    @abstractmethod
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
            self._xcvr_capability_clean()
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
            rx_los_raw = self.read_eeprom(SFP_CHANNEL_STATUS_CONTROL_OFFSET,
                                          SFP_CHANNEL_STATUS_CONTROL_WIDTH)
            if rx_los_raw is None:
                return None
            rx_los_data = int(rx_los_raw[0], 16)
            rx_los_list.append(rx_los_data & 0x02 != 0)

        return rx_los_list

    @abstractmethod
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
            self._xcvr_capability_clean()
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
            tx_fault_raw = self.read_eeprom(SFP_CHANNEL_STATUS_CONTROL_OFFSET,
                                            SFP_CHANNEL_STATUS_CONTROL_WIDTH)
            if tx_fault_raw is None:
                return None
            tx_fault_data = int(tx_fault_raw[0], 16)
            tx_fault_list.append(tx_fault_data & 0x04 != 0)

        return tx_fault_list


    @abstractmethod
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
            self._xcvr_capability_clean()
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
            tx_disable_raw = self.read_eeprom(SFP_CHANNEL_STATUS_CONTROL_OFFSET,
                                              SFP_CHANNEL_STATUS_CONTROL_WIDTH)
            if tx_disable_raw is None:
                return None
            tx_disable_data = int(tx_disable_raw[0], 16)
            tx_disable_list.append(tx_disable_data & 0x08 != 0)


        return tx_disable_list

    def get_tx_disable_channel(self):
        """
        Retrieves the TX disabled channels in this SFP

        Returns:
            A list of boolean values, representing the TX disable status
            of each available channel, value is True if SFP channel
            has TX disable, False if not.
            E.g., for a tranceiver with four channels: [False, False, True, False]
        """
        if not self.get_presence():
            self._xcvr_capability_clean()
            return None

        tx_disable_list = self.get_tx_disable()
        if tx_disable_list is None:
            return None

        tx_disable = 0
        for i in range(len(tx_disable_list)):
            if tx_disable_list[i]:
                tx_disable |= 1 << i

        return tx_disable

    def get_lpmode(self):
        """
        Retrieves the lpmode (low power mode) status of this SFP

        Returns:
            A Boolean, True if lpmode is enabled, False if disabled
        """
        if not self.get_presence():
            self._xcvr_capability_clean()
            return None

        lpmode = False
        if self.is_qsfp():
            power_raw = self.read_eeprom(QSFP_POWER_MODE_OFFSET,
                                         QSFP_POWER_MODE_WIDTH)
            if power_raw is None:
                return None
            power_data = int(power_raw[0], 16)
            # if lpmod, power-override bit and power-set bit are both setted
            #               bit0                bit1
            lpmode = power_data & 0x03 != 0
        else:
            logger.log_error('SFP not support low power mode setting.')
            return None

        return lpmode


    def get_power_override(self):
        """
        Retrieves the power-override status of this SFP

        Returns:
            A Boolean, True if power-override is enabled, False if disabled
        """
        if not self.get_presence():
            self._xcvr_capability_clean()
            return None

        power_override = False
        if self.is_qsfp():
            power_raw = self.read_eeprom(QSFP_POWER_MODE_OFFSET,
                                         QSFP_POWER_MODE_WIDTH)
            if power_raw is None:
                return None
            power_data = int(power_raw[0], 16)
            power_override = power_data & 0x01 != 0
        else:
            logger.log_error('SFP not support power override setting.')
            return None

        return power_override

    def get_temperature(self):
        """
        Retrieves the temperature of this SFP

        Returns:
            A float representing the current temperature in Celsius
        """
        if self.dom_supported() and self.dom_temp_supported():
            transceiver_bulk_status = self.get_transceiver_bulk_status()
            if transceiver_bulk_status:
                return transceiver_bulk_status.get("temperature", "N/A")

        return None

    def get_voltage(self):
        """
        Retrieves the supply voltage of this SFP

        Returns:
            A float representing the supply voltage in mV
        """
        if self.dom_supported() and self.dom_volt_supported():
            transceiver_bulk_status = self.get_transceiver_bulk_status()
            if transceiver_bulk_status:
                return transceiver_bulk_status.get("voltage", "N/A")

        return None

    def get_tx_bias(self):
        """
        Retrieves the TX bias current of all SFP channels

        Returns:
            A list of floats, representing TX bias in mA
            for each available channel
            E.g., for a tranceiver with four channels: ['110.09', '111.12', '108.21', '112.09']
        """
        if self.dom_supported():
            transceiver_bulk_status = self.get_transceiver_bulk_status()
            if transceiver_bulk_status:
                tx_bias_list = []
                keys = list(transceiver_bulk_status.keys())
                for key in sorted(keys):
                    if re.search('tx.*bias', key):
                        tx_bias_list.append(transceiver_bulk_status.get(key, "N/A"))
                return tx_bias_list

        return None

    def get_rx_power(self):
        """
        Retrieves the received optical power of all SFP channels

        Returns:
            A list of floats, representing received optical
            power in mW for each available channel
            E.g., for a tranceiver with four channels: ['1.77', '1.71', '1.68', '1.70']
        """
        if self.dom_supported() and self.dom_rx_power_supported():
            transceiver_bulk_status = self.get_transceiver_bulk_status()
            if transceiver_bulk_status:
                rx_power_list = []
                keys = list(transceiver_bulk_status.keys())
                for key in sorted(keys):
                    if re.search('rx.*power', key):
                        rx_power_list.append(transceiver_bulk_status.get(key, "N/A"))
                return rx_power_list

        return None

    def get_tx_power(self):
        """
        Retrieves the TX power of all SFP channels

        Returns:
            A list of floats, representing TX power in mW
            for each available channel
            E.g., for a tranceiver with four channels: ['1.86', '1.86', '1.86', '1.86']
        """
        if self.dom_supported() and self.dom_tx_power_supported():
            transceiver_bulk_status = self.get_transceiver_bulk_status()
            if transceiver_bulk_status:
                tx_power_list = []
                keys = list(transceiver_bulk_status.keys())
                for key in sorted(keys):
                    if re.search('tx.*power', key):
                        tx_power_list.append(transceiver_bulk_status.get(key, "N/A"))
                return tx_power_list

        return None

    @abstractmethod
    def set_reset(self, reset):
        """
        Reset SFP and return all user module settings to their default state.

        Args:
            reset: True  ---- set reset
                   False ---- set unreset

        Returns:
            A boolean, True if successful, False if not
        """
        raise NotImplementedError

    def reset(self):
        """
        Reset SFP and return all user module settings to their default state.

        Returns:
            A boolean, True if successful, False if not
        """
        self.set_reset(True)
        time.sleep(0.5)
        self.set_reset(False)

    @abstractmethod
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
            self._xcvr_capability_clean()
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

            reg = self.read_eeprom(SFP_CHANNEL_STATUS_CONTROL_OFFSET, SFP_CHANNEL_STATUS_CONTROL_WIDTH)
            if reg is None:
                return False
            value = int(reg[0], 16)
            if tx_disable:
                value |= (0x1 << 6)
            else:
                value &= ~(0x1 << 6)
            rev = self.write_eeprom(SFP_CHANNEL_STATUS_CONTROL_OFFSET, [value])

            # sleep 100ms
            time.sleep(0.1)
            return rev

    def tx_disable_channel(self, channel, disable):
        """
        Sets the tx_disable for specified SFP channels

        Args:
            channel : A hex of 4 bits (bit 0 to bit 3) which represent channel 0 to 3,
                      e.g. 0x5 for channel 0 and channel 2.
            disable : A boolean, True to disable TX channels specified in channel,
                      False to enable

        Returns:
            A boolean, True if successful, False if not
        """
        if not self.get_presence():
            self._xcvr_capability_clean()
            return False

        if self.is_qsfp():
            tx_disable_raw = self.read_eeprom(QSFP_CHANNEL_TX_DISABLE_OFFSET,
                                              QSFP_CHANNEL_TX_DISABLE_WIDTH)
            if tx_disable_raw is None:
                return None
            value = int(tx_disable_raw[0], 16)

            for i in range(0, 4):
                if channel & (0x01 << i):
                    if disable:
                        value |= 0x01 << i
                    else:
                        value &= ~(0x01 << i)

            logger.log_debug('{} tx_disable_channel set {}'.format(self.get_name(), value))

            rev = self.write_eeprom(QSFP_CHANNEL_TX_DISABLE_OFFSET, [value])
            time.sleep(0.1)
            return rev
        else:
            logger.log_error('SFP not support tx_diable_channel.')

            return False

    def set_lpmode(self, lpmode):
        """
        Sets the lpmode (low power mode) of SFP

        Args:
            lpmode: A Boolean, True to enable lpmode, False to disable it
            Note  : lpmode can be overridden by set_power_override

        Returns:
            A boolean, True if lpmode is set successfully, False if not
        """
        if not self.get_presence():
            self._xcvr_capability_clean()
            return False

        if self.is_qsfp():
            power_raw = self.read_eeprom(QSFP_POWER_MODE_OFFSET, QSFP_POWER_MODE_WIDTH)
            if power_raw is None:
                return None
            power_data = int(power_raw[0], 16)
            if lpmode:
                power_data |= 0x03
            else:
                power_data &= ~(0x03)
            rev = self.write_eeprom(QSFP_POWER_MODE_OFFSET, [power_data])

            time.sleep(0.1)
            return rev
        else:
            logger.log_error('SFP not support low power mode setting.')
            return False

    def set_power_override(self, power_override, power_set):
        """
        Sets SFP power level using power_override and power_set

        Args:
            power_override :
                    A Boolean, True to override set_lpmode and use power_set
                    to control SFP power, False to disable SFP power control
                    through power_override/power_set and use set_lpmode
                    to control SFP power.
            power_set :
                    Only valid when power_override is True.
                    A Boolean, True to set SFP to low power mode, False to set
                    SFP to high power mode.

        Returns:
            A boolean, True if power-override and power_set are set successfully,
            False if not
        """
        if not self.get_presence():
            self._xcvr_capability_clean()
            return False

        if self.is_qsfp():
            power_raw = self.read_eeprom(QSFP_POWER_MODE_OFFSET, QSFP_POWER_MODE_WIDTH)
            if power_raw is None:
                return None
            power_data = int(power_raw[0], 16)

            if power_override:
                power_data |= 0x01
                if power_set:
                    power_data |= 0x02
                else:
                    power_data &= ~(0x02)
            else:
                power_data &= ~(0x3)

            rev = self.write_eeprom(QSFP_POWER_MODE_OFFSET, [power_data])

            time.sleep(0.1)
            return rev
        else:
            logger.log_error('SFP not support low power mode setting.')
            return False

    def get_change_event(self):
        """
        Retrieves the sfp status event

        Returns:
             -----------------------------------------------------------------
             device   |     device_id       |  device_event  |  annotate
             -----------------------------------------------------------------
             'sfp'          '<sfp number>'     '0'              Sfp removed
                                               '1'              Sfp inserted
                                               '2'              I2C bus stuck
                                               '3'              Bad eeprom
                                               '4'              Unsupported cable
                                               '5'              High Temperature
                                               '6'              Bad cable
             -----------------------------------------------------------------
        """
        new_presence = self.get_presence()
        now_ms = time.time() * 1000
        if self._old_presence != new_presence:
            if new_presence:
                # delay 1000ms for report changed to presence
                if now_ms - self._start_ms >= 1000:
                    self._old_presence = new_presence
                    return (True, {'sfp': {self.index():'1'}})
            else:
                # immediately report changed to unpresence
                self._old_presence = new_presence
                return (True, {'sfp': {self.index():'0'}})

        else:
            self._start_ms = now_ms

        return (False, {'sfp':{}})
