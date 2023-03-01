#!/usr/bin/env python
"""
Module contains an implementation of SONiC Platform Base API and
provides the platform information
"""
try:
    from sonic_platform_base.platform_base import PlatformBase
    from sonic_platform.chassis import Chassis
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")


class Platform(PlatformBase):
    """
    Platform-specific Platform class
    """

    dev_list_default = ['component', 'fan', 'fan_drawer', 'psu', 'thermal', 'sfp', 'watchdog', 'syseeprom']

    def __init__(self, dev_list=dev_list_default):
        chassis = Chassis(dev_list)
        self._chassis = chassis
        #super(Platform, self).__init__(chassis)
