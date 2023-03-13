#!/usr/bin/env python
"""
Module contains an implementation of SONiC Platform Base API and
provides the 'Syseeprom' information which are available in the platform
"""

try:
    from vendor_sonic_platform.devcfg import Devcfg
    from sonic_platform_base.sonic_eeprom.eeprom_tlvinfo import TlvInfoDecoder
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")


class Eeprom(TlvInfoDecoder):
    """Platform-specific EEPROM class"""

    def __init__(self):
        super(Eeprom, self).__init__(Devcfg.EEPROM_DATA_DIR, 0, '', True)
