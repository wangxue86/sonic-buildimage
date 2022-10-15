#!/usr/bin/env python

########################################################################
#
# Module contains an implementation of SONiC Platform Base API and
# provides the Components' (e.g., BIOS, CPLD, FPGA, etc.) available inde
# provides the Components' (e.g., BIOS, CPLD, FPGA, etc.) available in
# the platform
#
########################################################################

try:
    import re
    import subprocess
    from sonic_platform_base.component_base import ComponentBase
except ImportError as _e:
    raise ImportError(str(_e) + "- required module not found")

BIOS_QUERY_VERSION_COMMAND = "sudo dmidecode -t 11"


class Component(ComponentBase):
    """Platform-specific Component class"""

    CPLD_DIR = "/sys/switch/cpld/"
    DEBUG_CPLD_DIR = "/sys/switch/debug/cpld/"
    CHASSIS_COMPONENTS = [
        ["BIOS", ("Performs initialization of hardware components during "
                  "booting")],
        ["CPU-CPLD", "Used for managing CPU board devices and power"],
        ["MAIN_BOARD-CPLD", ("Used for managing Fan, PSU, system LEDs, QSFP "
                             "modules (1-16)")]
    ]

    def __init__(self, component_index):
        self.index = component_index
        self.name = self.CHASSIS_COMPONENTS[self.index][0]
        self.description = self.CHASSIS_COMPONENTS[self.index][1]

        super(Component, self).__init__()

    @classmethod
    def _get_file_path(cls, main_dir, sub_dir):
        _dir = main_dir+sub_dir
        temp_value = 0
        try:
            with open(_dir, 'r') as temp_read:
                temp_value = temp_read.read()
        except IOError:
            temp_value = "io error"
        return temp_value

    @classmethod
    def _get_command_result(cls, cmdline):
        try:
            proc = subprocess.Popen(cmdline.split(), stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            stdout = proc.communicate()[0]
            proc.wait()
            result = stdout.decode().rstrip('\n')
        except OSError:
            result = None

        return result


    def get_name(self):
        """
        Retrieves the name of the component

        Returns:
            A string containing the name of the component
        """
        return self.name

    def get_description(self):
        """
        Retrieves the description of the component

        Returns:
            A string containing the description of the component
        """
        return self.description

    def get_firmware_version(self):
        """
        Retrieves the firmware version of the component

        Returns:
            A string containing the firmware version of the component
        """
        cpld_version = ""
        if self.index == 0:
            bios_ver = self._get_command_result(BIOS_QUERY_VERSION_COMMAND)
            if not bios_ver:
                return 'NA'
            pattern = r"H3C BIOS Version *([^\n]+)"
            matchobj = re.search(pattern, bios_ver)
            ver_str = matchobj.group(1) if matchobj != None else "NA"
            return ver_str

        elif self.index == 1:
            cpu_cpld_content = self._get_file_path(self.DEBUG_CPLD_DIR, "cpu_cpld")
            exp = '0x0000: (\\w{2}) (\\w{2}) \\w{2} (\\w{2})'
            ver_arr = re.findall(exp, cpu_cpld_content)[0]
            firmware_version = "%s" % (int(ver_arr[2], 16) & 0xf)
            return firmware_version

        elif self.index == 2:
            cpld_num = self._get_file_path(self.CPLD_DIR, "num_cplds")
            for i in range(0, int(cpld_num)):
                fw_version = self._get_file_path(self.CPLD_DIR + "/cpld" + str(i) + "/",
                                                 "hw_version")
                cpld_version += "%s" % fw_version
            return cpld_version
        else:
            return None

    def install_firmware(self, image_path):
        """
        Installs firmware to the component

        Args:
            image_path: A string, path to firmware image

        Returns:
            A boolean, True if install was successful, False if not
        """
        return False
