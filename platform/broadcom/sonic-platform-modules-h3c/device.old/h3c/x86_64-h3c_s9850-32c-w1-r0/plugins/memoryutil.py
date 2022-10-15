import re
import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base

class MemoryUtil(Util_Base):
    """Platform-specific Component class"""

    def __init__(self):
        self.model_list = []
        self.vendor_list = []
        self.size_list = []
        self.factor_list = []
        self.detail_list = []
        self.speed_list = []
        try:
            #info = self.exec_cmd("sudo dmidecode -t memory")
            info = self.read_attr("/etc/sonic/.plugins/dmidecode_memory")

            pattern = "Type: *([^\n]+)"
            match_obj_model = re.findall(pattern, info, re.M)
            self.model_list = [model.strip() for model in match_obj_model[1:] if model]

            pattern = "Manufacturer: *([^\n]+)"
            match_obj_vendor = re.findall(pattern, info, re.M)
            self.vendor_list = [vendor.strip() for vendor in match_obj_vendor if vendor]

            pattern = "Size: *([^\n]+)"
            match_obj_size = re.findall(pattern, info, re.M)
            self.size_list = [size.strip() for size in match_obj_size if size]

            pattern = "Factor: *([^\n]+)"
            match_obj_factor = re.findall(pattern, info, re.M)
            self.factor_list = [factor.strip() for factor in match_obj_factor if factor]

            pattern = "Detail: *([^\n]+)"
            match_obj_detail = re.findall(pattern, info, re.M)
            self.detail_list = [detail.strip() for detail in match_obj_detail if detail]

            self.speed = "2667 MHz"
            
        except Exception:
            self.model_list = []
            self.vendor_list = []
            self.size_list = []
            self.factor_list = []
            self.detail_list = []
            self.speed = "Unknown"

    def get_model(self, index):
        return self.model_list[index]

    def get_vendor(self, index):
        return self.vendor_list[index]

    def get_size(self, index):
        return self.size_list[index]

    def get_description(self, index):
        if not self.size_list[index]:
            return "NA"

        if self.size_list[index] in ["No Module Installed", "Unknown"]:
            return self.size_list[index]
        return " ".join([self.factor_list[index],  self.model_list[index], \
                            self.detail_list[index],  self.speed])

    def get_info(self):
        memory_dict_keys = [
            "model",
            "vendor",
            "size",
            "description"
        ]
        memory_num = len(self.model_list)
        memory_info_dict = {"objs":list(), "num":0}
        memory_list = []
        for index in range(memory_num):
            memory_info = dict.fromkeys(memory_dict_keys, "NA")
            memory_info["model"] = self.get_model(index)
            memory_info["vendor"] = self.get_vendor(index)
            memory_info["size"] = self.get_size(index)
            memory_info["description"] = self.get_description(index)
            memory_list.append(memory_info)
        memory_info_dict["objs"] = memory_list
        memory_info_dict["num"] = memory_num
        return json.dumps(memory_info_dict, ensure_ascii = False,indent=4)
