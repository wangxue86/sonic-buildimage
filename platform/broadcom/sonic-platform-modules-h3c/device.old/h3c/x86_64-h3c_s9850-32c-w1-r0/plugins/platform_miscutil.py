import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base

class Platform_MiscUtil(Util_Base):
    def __init__(self):
        self.bmc_model = "AST2500"
        self.nic_model = "I350"
        self.nic_vendor = "Intel"

    def get_bios_vendor(self):
        #cmd = "sudo dmidecode -s bios-vendor"
        #output = self.exec_cmd(cmd)
        output = self.read_attr("/etc/sonic/.plugins/misc_bios_vendor")
        if not output:
            return "NA"
        bios_vendor = output.strip()
        return bios_vendor

    def get_bios_version(self):
        #cmd  = "sudo dmidecode -s bios-version"
        #output = self.exec_cmd(cmd)
        output = self.read_attr("/etc/sonic/.plugins/misc_bios_version")
        if not output:
            return "NA" 
        bios_version = output.strip()
        return bios_version

    def get_bmc_version(self):
        cmd  = "ipmitool mc info | grep Firmware"
        output = self.exec_cmd(cmd)
        if not output:
            return "NA"
        pattern = "Firmware Revision\s*:\s*(\d+\.(?:\d+\.)*\d+)"
        bmc_version = self.parse_output(pattern, output)
        return bmc_version

    def get_bmc_model(self):
        return self.bmc_model

    def get_onie_version(self):
        #cmd = "cat /host/machine.conf | grep onie_version"
        #output = self.exec_cmd(cmd)
        output = self.read_attr("/etc/sonic/.plugins/misc_onie_version")
        if not output:
            return "NA"
        pattern = "onie_version=(.+)"
        onie_version = self.parse_output(pattern, output)
        return onie_version

    def get_cpu_model(self):
        cmd = "grep \"model name\" /proc/cpuinfo|uniq"
        output = self.exec_cmd(cmd)
        if not output:
            return "NA"
        pattern = "model name\s*:\s*(.+)"
        cpu_model = self.parse_output(pattern, output)
        return cpu_model

    def get_cpu_core(self):
        cmd = "grep \"cpu cores\" /proc/cpuinfo|uniq"
        output = self.exec_cmd(cmd)
        if not output:
            return "NA"
        pattern = "cpu cores\s*:\s(\d+)"
        cpu_core = self.parse_output(pattern, output)
        try:
            cpu_core = int(cpu_core)
        except (ValueError, TypeError):
            return "NA"
        return cpu_core

    def get_nic_model(self):
        return self.nic_model

    def get_nic_vendor(self):
        return self.nic_vendor

    def get_nic_firmware_version(self):
        #cmd = "sudo ip netns exec mgmt ethtool -i eth0"
        #output = self.exec_cmd(cmd)
        output = self.read_attr("/etc/sonic/.plugins/misc_nic_fw_version")
        if not output:
            return "NA"
        pattern = "firmware-version:\s*(.+)"
        nic_firmware_version = self.parse_output(pattern, output)
        return nic_firmware_version

    def get_cpu_bmc_switch_model(self):
        return "NA"

    def get_cpu_bmc_switch_version(self):
        return "NA"

    def get_info(self):
        """
        Returns:
            a dict: {platform_dict}
        """
        platform_dickt_keys = [
            "bios_vendor",
            "bios_version",
            "bmc_model",
            "bmc_version",
            "onie_version",
            "cpu_model",
            "cpu_core",
            "nic_model",
            "nic_vendor",
            "nic_firmware_version",
            "cpu_bmc_switch_model",
            "cpu_bmc_switch_version"
        ]
        platform_info_dict = dict.fromkeys(platform_dickt_keys, "NA")
        platform_info_dict["bios_vendor"]  = self.get_bios_vendor()
        platform_info_dict["bios_version"] = self.get_bios_version()
        platform_info_dict["bmc_model"] = self.get_bmc_model()
        platform_info_dict["bmc_version"] = self.get_bmc_version()
        platform_info_dict["onie_version"] = self.get_onie_version()
        platform_info_dict["cpu_model"] = self.get_cpu_model()
        platform_info_dict["cpu_core"] = str(self.get_cpu_core())
        platform_info_dict["nic_model"] = self.get_nic_model()
        platform_info_dict["nic_vendor"] = self.get_nic_vendor()
        platform_info_dict["nic_firmware_version"] = self.get_nic_firmware_version()
        platform_info_dict["cpu_bmc_switch_model"] = self.get_cpu_bmc_switch_model()
        platform_info_dict["cpu_bmc_switch_version"] = self.get_cpu_bmc_switch_version()
        return json.dumps(platform_info_dict, ensure_ascii = False,indent=4)