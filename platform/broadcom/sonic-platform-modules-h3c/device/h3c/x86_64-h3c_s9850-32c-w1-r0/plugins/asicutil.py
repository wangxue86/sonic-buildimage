import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base


class AsicUtil(Util_Base):
    """Platform-specific Component class"""

    def __init__(self):
        self.model = "BCM56870_A0"
        self.vendor = "Broadcom"

    def get_model(self):
        return self.model

    def get_vendor(self):
        return self.vendor

    def get_sdk_version(self):
        #output = self.exec_cmd("sudo bcmcmd \"version\" | grep \"Release:\"")
        output = self.read_attr("/etc/sonic/.plugins/asic_sdk_version")
        if not output:
            return "NA"
        sdk_version = self.parse_output("sdk-(\d+\.(?:\d+\.)*\d+)", output)
        return sdk_version

    def get_phy_version(self):
        return "NA"

    def get_pcie_firmware_version(self):
        #output = self.exec_cmd("sudo bcmcmd \"pciephy fw version\"")
        output = self.read_attr("/etc/sonic/.plugins/asic_pcie_fw_version")
        if not output:
            return "NA"
        pattern = "PCIe FW loader version:\s*(\d+\.(?:\d+\.)*\d+)"
        pcie_version = self.parse_output(pattern, output)
        return pcie_version

    def get_info(self):

        asic_dict_keys = [
            "model",
            "sdk_version",
            "vendor",
            "phy_version",
            "pcie_firmware_version"
        ]
        asic_info = dict.fromkeys(asic_dict_keys, "NA")
        asic_info["model"] = self.get_model()
        asic_info["sdk_version"] = self.get_sdk_version()
        asic_info["vendor"] = self.get_vendor()
        asic_info["phy_version"] = self.get_phy_version()
        asic_info["pcie_firmware_version"] = self.get_pcie_firmware_version()
        return json.dumps(asic_info, ensure_ascii = False,indent=4)
