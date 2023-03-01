#!/usr/bin/env python
"""
Module contains an implementation of SONiC Platform Base API and
provides the Components' (e.g., BIOS, CPLD, FPGA, etc.) available
the platform
"""
try:
    import re
    import os
    import time
    from vendor_sonic_platform.devcfg import Devcfg
    from vendor_sonic_platform.utils import UtilsHelper
    from sonic_platform_base.component_base import ComponentBase
    from sonic_platform import log_wrapper
except ImportError as error:
    raise ImportError(str(error) + "- required module not found")

try:
    from pathlib2 import Path
except BaseException:
    from pathlib import Path

BIOS_QUERY_VERSION_COMMAND = "dmidecode -t 11"


class Component(ComponentBase):
    """Platform-specific Component class"""

    def __init__(self, component_index):
        self.index = component_index
        self.name = Devcfg.CHASSIS_COMPONENTS[self.index][0]
        self.description = Devcfg.CHASSIS_COMPONENTS[self.index][1]
        super(Component, self).__init__()
        log_wrapper.log_init(self)

    @staticmethod
    def _get_file_path(main_dir, sub_dir):
        return UtilsHelper.read_data_from_file(main_dir, sub_dir)

    @staticmethod
    def _get_command_result(cmdline):
        return UtilsHelper.exec_cmd(cmdline)

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
            ver_str = 'N/A'
            status,bios_ver = self._get_command_result(BIOS_QUERY_VERSION_COMMAND)
            if status == 0:
                if bios_ver:
                    matchobj = re.search(Devcfg.BIOS_VERSION_PATTERN, bios_ver)
                    ver_str = matchobj.group(1) if matchobj is not None else "N/A"
                return ver_str

        elif self.index >= 1 and self.index < Devcfg.COMPONENT_NUM:
            fw_version = self._get_file_path(Devcfg.CPLD_DIR + "/cpld" + str(self.index-1)
                                             + "/", "hw_version")
            cpld_version += "%s" % fw_version
            return cpld_version
        return 'N/A'

    @staticmethod
    def _update_bios(image_path):
        '''
        BIOS update
        '''
        upgrade_file_name = ''
        upgrade_file_list = list()

        efi_mount_dir = Devcfg.EFI_MOUNT_DIR    #/mnt/bios_update
        efi_boot_dir = Devcfg.EFI_BOOT_DIR      #/mnt/bios_update/EFI/boot
        efi_dir = Devcfg.EFI_DIR                #/mnt/bios_update/EFI

        mount_dir = Path(efi_mount_dir)

        if not mount_dir.is_dir():
            os.system("mkdir -p {}".format(efi_mount_dir))

        if not mount_dir.is_dir():
            return False

        if not Path(efi_dir).is_dir():
            # mount /dev/sda1 /mnt/bios_update/
            os.system("mount {} {}".format(Devcfg.EFI_PARTITION, efi_mount_dir))
            time.sleep(0.1)
            if not Path(efi_dir).is_dir():
                return False

        if not Path(efi_boot_dir).is_dir():
            os.system("mkdir -p {}".format(efi_boot_dir))
            time.sleep(0.1)
            if not Path(efi_boot_dir).is_dir():
                return False
        else:
            os.system("rm -rf {}/*".format(efi_boot_dir))

        os.system("cp -rfa %s {}".format(efi_boot_dir) % image_path)
        upgrade_file_list = os.listdir(efi_boot_dir)

        if upgrade_file_list:
            upgrade_file_name = upgrade_file_list[0]
        else:
            return False

        os.system("mv {}/%s {}/bdeplatcome.btw".format(efi_boot_dir, efi_boot_dir)
                  % upgrade_file_name)

        if not os.path.exists("{}/bdeplatcome.btw".format(efi_boot_dir)):
            return False

        os.system("reboot")

        return True

    def _update_cpld(self, image_path):
        '''
        CPLD update
        '''
        kernel_ver_name = os.popen('uname -a').readlines()[0].split()[2]
        drv_exist = os.popen('lsmod | grep drv_cpld').readlines()
        if not drv_exist:
            os.system('insmod /lib/modules/%s/extra/drv_cpld.ko' % kernel_ver_name)

        if self.index == 1:
            os.system('chmod 777 {}'.format(Devcfg.VMECPU_DIR))
            time.sleep(0.1)
            os.system('vmecpu %s' % image_path)
        elif self.index == 2:
            os.system('chmod 777 {}'.format(Devcfg.VME_DIR))
            time.sleep(0.1)
            os.system('vme %s' % image_path)
            time.sleep(5)
            try:
                with open(Devcfg.DEBUG_CPLD_DIR, 'rb+') as power_ctl:
                    cpld_value = power_ctl.readlines()
                    for value in cpld_value:
                        if re.search('0x0030', value):
                            reg_value = int(value.split()[8], 16)
                            set_value = reg_value | 0x40
                            power_ctl.write("0x37:0x%x" % set_value)
            except Exception as err:
                self.logger.log_error(str(err) + 'need to power down manually')
                return False
        return True

    def install_firmware(self, image_path):
        """
        Installs firmware to the component
        Args:
            image_path: A string, path to firmware image
        Returns:
            A boolean, True if install was successful, False if not
        """
        result = False
        if self.index == 0:
            result = self._update_bios(image_path)
        elif self.index == 1 or self.index == 2:
            result = self._update_cpld(image_path)

        return result
