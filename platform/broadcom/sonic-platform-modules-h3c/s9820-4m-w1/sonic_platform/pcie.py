#!/usr/bin/env python
"""
Module contains an implementation of SONiC Platform Base API and
provides the 'Pcie' information which are available in the platform
"""

try:
    import abc
    from sonic_platform_base.sonic_pcie.pcie_common import PcieUtil
except ImportError as e:
    raise ImportError (str(e) + " - required module not found")

class Pcie(PcieUtil):
    def __init__(self, path):
        self.pcie_util = PcieUtil(path)


    @abc.abstractmethod
    def get_pcie_device(self):
        """
         get current device pcie info

          Returns:
            A list including pcie device info
         """
        return self.pcie_util.get_pcie_device()


    @abc.abstractmethod
    def get_pcie_check(self):
        """
         Check Pcie device with config file
         Returns:
            A list including pcie device and test result info
        """
        return self.pcie_util.get_pcie_check()
