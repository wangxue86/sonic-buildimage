#!/usr/bin/env python
#
#Name:eeprom.py,version:1.0
#

try:
    from sonic_platform_base.sonic_eeprom.eeprom_tlvinfo import TlvInfoDecoder
except ImportError as _e:
    raise ImportError(str(_e) + "- required module not found")


class Eeprom(TlvInfoDecoder):
    """
    Platform-specific EEPROM class
    """
    I2C_DIR = "/sys/switch/syseeprom/"

    def __init__(self):
        eeprom_path = "/sys/switch/syseeprom/eeprom"
        super(Eeprom, self).__init__(eeprom_path, 0, '', True)

