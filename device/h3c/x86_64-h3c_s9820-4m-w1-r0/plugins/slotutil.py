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
    from sonic_slot.slot_base import SlotBase
except ImportError as e:
    raise ImportError (str(e) + "- required module not found")

class SlotUtil(SlotBase):
    """Platform-specific PSUutil class"""

    def __init__(self):
        SlotBase.__init__(self)

        self.slot_path = "/sys/switch/slot/"
        self.slot_presence = "/status"
        self.slot_oper_status = "/status"
        self.slot_mapping = {
            1: "slot1",
            2: "slot2",
            3: "slot3",
            4: "slot4"
        }
        self.slot_sn = "/sn"
        self.slot_pn = "/pn"
        self.slot_model = "/product_name"
        self.slot_hw_version = "/hw_version"

    def get_slot_num(self):
        return len(self.slot_mapping)

    def get_slot_status(self, index):
        if index is None:
            return False

        status = 0
        node = self.slot_path + self.slot_mapping[index]+self.slot_oper_status
        try:
            with open(node, 'r') as power_status:
                status = int(power_status.read().strip())
        except IOError:
            return False

        if status == 1:
            return 1
        else:
            return 0

    def get_slot_presence(self, index):
        if index is None:
            return False

        status = 0
        node = self.slot_path + self.slot_mapping[index] + self.slot_presence
        try:
            with open(node, 'r') as presence_status:
                status = int(presence_status.read().strip())
        except IOError:
            return False

        if status == 0:
            return 0
        else:
            return 1

    def get_slot_sn(self, index):
        if index is None:
            return "index is None"
        
        sn = ""
        node = self.slot_path + self.slot_mapping[index] + self.slot_sn
        try:
            with open(node, 'r') as f:
                sn = f.read().strip()
        except IOError:
            return "Slot {} sn read failed IOError".format(index)
        
        return sn

    def get_slot_pn(self, index):
        if index is None:
            return "index is None"
        
        pn = ""
        node = self.slot_path + self.slot_mapping[index] + self.slot_pn
        try:
            with open(node, 'r') as f:
                pn = f.read().strip()
        except IOError:
            return "Slot {} pn read failed IOError".format(index)
        
        return pn

    def get_slot_hw_version(self, index):
        if index is None:
            return "index is None"
        
        hw_version = ""
        node = self.slot_path + self.slot_mapping[index] + self.slot_hw_version
        try:
            with open(node, 'r') as f:
                hw_version = f.read().strip()
        except IOError:
            return "Slot {} hw_version read failed IOError".format(index)
        
        return hw_version

    def get_slot_model(self, index):
        if index is None:
            return "index is None"
        
        model = ""
        node = self.slot_path + self.slot_mapping[index] + self.slot_model
        try:
            with open(node, 'r') as f:
                model = f.read().strip()
        except IOError:
            return "Slot {} model read failed IOError".format(index)
        
        return model

