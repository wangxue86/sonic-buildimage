import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base

class SlotUtil(Util_Base):
    def __init__(self):
        self.slot_path = "/sys/switch/slot/"
        self.slot_num = self._get_slot_num()

    def _get_slot_num(self):
        try:
            slot_num = int(self.read_attr(self.slot_path + 'num_slot'))
        except (ValueError, TypeError):
            return 0
        return slot_num

    def get_type(self, index):
        slot_type = self.read_attr(os.path.join(self.slot_path, "slot{}".format(index), "product_name"))
        return slot_type

    def get_sn(self, index):
        slot_sn = self.read_attr(os.path.join(self.slot_path, "slot{}".format(index), "sn"))
        return slot_sn

    def get_presence(self, index):
        present = self.read_attr(os.path.join(self.slot_path, "slot{}".format(index), "status"))
        if not present:
            return "NA"
        try:
            present = int(present)
        except (ValueError, TypeError):
            return "NA"
        return present

    def get_status(self, index):
        status = self.read_attr(os.path.join(self.slot_path, "slot{}".format(index), "status"))
        if not status:
            return "NA"
        try:
            status = int(status)
        except (ValueError, TypeError):
            return "NA"
        return status

    def get_part_number(self, index):
        part_number = self.read_attr(os.path.join(self.slot_path, "slot{}".format(index), "pn"))
        return part_number

    def get_hw_version(self, index):
        hw_version = self.read_attr(os.path.join(self.slot_path, "slot{}".format(index), "hw_version"))
        return hw_version

    def get_info(self):
        slot_dict_kesy = [
            "type",
            "sn",
            "presence",
            "status",
            "part_number",
            "hw_version"
        ]
        slot_info_dict = {"objs":list(), "num":0}
        if not self.slot_num:
            slot_info_dict["num"] = 0
            return slot_info_dict

        slot_list  = []
        for index in range(1, self.slot_num + 1):
            slot_info = dict.fromkeys(slot_dict_kesy, "NA")
            slot_info["presence"] = str(self.get_presence(index))
            if slot_info["presence"] == '0':
                slot_list.append(slot_info)
                continue
            slot_info["type"] = self.get_type(index)
            slot_info["sn"] = self.get_sn(index)
            slot_info["status"] = str(self.get_status(index))
            slot_info["part_number"] = self.get_part_number(index)
            slot_info["hw_version"] = self.get_hw_version(index)
            slot_list.append(slot_info)
        slot_info_dict["objs"] = slot_list
        slot_info_dict["num"] = self.slot_num
        return json.dumps(slot_info_dict, ensure_ascii = False,indent = 4)
