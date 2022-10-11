import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base

class FpgaUtil(Util_Base):

    def get_model(self, index):
        return "NA"

    def get_alias(self, index):
        return "NA"

    def get_hardware_version(self, index):
        return "NA"

    def get_firmware_version(self, index):
        return "NA"

    def get_num(self):
        return 0

    def get_info(self):
        """
        Returns
            a dict: {objs:[fpga_obj1, fpga_obj1...], num:..}
        """
        fpga_dict_keys = [
            "model",
            "vendor",
            "alias",
            "hardware_version",
            "firmware_version"
        ]
        fpga_info_dict = {"objs":list(), "num":0}
        fpga_num = self.get_num()
        if not fpga_num:
            fpga_info_dict["num"] = fpga_num
            return json.dumps(fpga_info_dict, ensure_ascii = False, indent = 4)
        fpga_list = []
        for index in range(fpga_num):
            fpga_info = dict.fromkeys(fpga_dict_keys, "NA")
            fpga_info["model"] = self.get_model(index)
            fpga_info["alias"] = self.get_alias(index)
            fpga_info["hardware_version"] = self.get_hardware_version(index)
            fpga_info["firmware_version"] = self.get_firmware_version(index)
            fpga_list.apped(fpga_info)
        fpga_info_dict["objs"] = fpga_list
        fpga_info_dict["num"] = fpga_num
        return json.dumps(fpga_info_dict, ensure_ascii = False,indent=4)
