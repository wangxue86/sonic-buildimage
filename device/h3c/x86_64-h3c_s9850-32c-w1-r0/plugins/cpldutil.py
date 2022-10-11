import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base

class CpldUtil(Util_Base):
    def __init__(self):
        self.cpld_path = "/sys/switch/cpld/"

    def get_num(self):
        path = os.path.join(self.cpld_path, "num_cplds")
        num = self.read_attr(path)
        try:
            num = int(num)
        except (ValueError, TypeError):
            return 0
        return num

    def get_model(self, index):
        path =  os.path.join(self.cpld_path, "cpld{}".format(index),"type")
        model = self.read_attr(path)
        return model

    def get_vendor(self, index):
        return "NA"

    def get_alias(self, index):
        path = os.path.join(self.cpld_path, "cpld{}".format(index), "alias")
        alias = self.read_attr(path)
        return alias

    def get_hardware_version(self, index):
        path = os.path.join(self.cpld_path, "cpld{}".format(index), "board_version")
        hardware_version = self.read_attr(path)
        return hardware_version

    def get_firmware_version(self, index):
        path = os.path.join(self.cpld_path, "cpld{}".format(index), "hw_version")
        firmware_version = self.read_attr(path)
        return firmware_version

    def get_info(self):
        cpld_dict_keys = [
            "model",
            "vendor",
            "alias",
            "hardware_version",
            "firmware_version"
        ]
        cpld_info_dict = {"objs":list(), "num": 0}
        cpld_num = self.get_num()
        if not cpld_num:
            return json.dumps(cpld_info_dict, ensure_ascii = False, indent = 4)
        cpld_list = []
        for index in range(0, cpld_num):
            cpld_info = dict.fromkeys(cpld_dict_keys, "NA")
            cpld_info["model"] = self.get_model(index)
            cpld_info["vendor"] = self.get_vendor(index)
            cpld_info["alias"] = self.get_alias(index)
            cpld_info["hardware_version"] = self.get_hardware_version(index)
            cpld_info["firmware_version"] = self.get_firmware_version(index)
            cpld_list.append(cpld_info)
        cpld_info_dict["objs"] = cpld_list
        cpld_info_dict["num"] = cpld_num
        return json.dumps(cpld_info_dict, ensure_ascii = False, indent = 4)