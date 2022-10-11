import os.path
import sys
import re
import json

from sonic_platform_base.sonic_ssd.ssd_generic import SsdUtil

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base


DISKFILE = "/proc/partitions"

class DiskUtil(Util_Base):
    def __init__(self):
        self.dev_list = self._get_disk_list()
        self.ssd = None
        self.model_list = []
        self.vendor_list = []
        self.firmware_version_list = []
        try:
            for dev in self.dev_list:
                ssdutil = SsdUtil('/dev/{}'.format(dev))
                self.model_list.append(ssdutil.get_model())
                self.vendor_list.append(ssdutil.get_vendor_output())
                self.firmware_version_list.append(ssdutil.get_firmware())
        except Exception:
            self.model_list = []
            self.vendor_list = []
            self.firmware_version_list = []

    def _get_disk_list(self):
        dev_list = ['sda']
        '''
        with open(DISKFILE, 'r') as fp:
            lines = fp.readlines()
        for line in lines[2:]:
            dev_name = line.split()[3]
            if (re.match("sda\d+", dev_name)):
                dev_list.append(dev_name)
                '''
        return dev_list

    def get_size(self, index):
        return "240G"
        '''
        cmd = "lsblk /dev/{} -nl --output SIZE --bytes".format(self.dev_list[index])
        size = self.exec_cmd(cmd).strip()
        if not size:
            return "NA"
        return int(size)
        '''

    def get_model(self, index):
        return self.model_list[index] if self.model_list else "NA"

    def get_firmware_version(self, index):
        return self.firmware_version_list[index] if self.firmware_version_list \
            else "NA"

    def get_vendor(self, index):
        return self.vendor_list[index] if self.vendor_list else "NA"

    def get_info(self):
        disk_dict_keys = [
            "model",
            "vendor",
            "size",
            "firmware_version"
        ]
        disk_info_dict = {"objs":list, "num":0}
        disk_num = len(self.dev_list)
        disk_list = []

        for index in range(disk_num):
            disk_info = dict.fromkeys(disk_dict_keys, "NA")
            disk_info["model"] = self.get_model(index)
            disk_info["vendor"] = self.get_vendor(index)
            disk_info["size"] = str(self.get_size(index))
            disk_info["firmware_version"] = self.get_firmware_version(index)
            disk_list.append(disk_info)
        disk_info_dict["objs"] = disk_list
        disk_info_dict["num"] = disk_num
        return json.dumps(disk_info_dict, ensure_ascii = False,indent=4)

