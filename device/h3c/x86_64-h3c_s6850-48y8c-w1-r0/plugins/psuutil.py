#!/usr/bin/env python

#############################################################################
# Accton
#
# Module contains an implementation of SONiC PSU Base API and
# provides the PSUs status which are available in the platform
#
#############################################################################

import os.path

try:
    from sonic_psu.psu_base import PsuBase
except ImportError as e:
    raise ImportError (str(e) + "- required module not found")

class PsuUtil(PsuBase):
    """Platform-specific PSUutil class"""

    def __init__(self):
        PsuBase.__init__(self)

        self.psu_path = "/sys/switch/psu/"
        self.psu_presence = "/status"
        self.psu_oper_status = "/status"
        self.psu_mapping = {
            1: "psu1",
            2: "psu2",
        }

    def get_num_psus(self):
        return len(self.psu_mapping)

    def get_psu_status(self, index):
        if index is None:
            return False

        status = 0
        node = self.psu_path + self.psu_mapping[index]+self.psu_oper_status
        try:
            with open(node, 'r') as power_status:
                status = int(power_status.read().strip())
        except IOError:
            return False

        if status == 1:
            return 1
        else:
            return 0


    def get_psu_presence(self, index):
        if index is None:
            return False

        status = 0
        node = self.psu_path + self.psu_mapping[index] + self.psu_presence
        try:
            with open(node, 'r') as presence_status:
                status = int(presence_status.read().strip())
        except IOError:
            return False

        if status == 0:
            return 0
        else:
            return 1
