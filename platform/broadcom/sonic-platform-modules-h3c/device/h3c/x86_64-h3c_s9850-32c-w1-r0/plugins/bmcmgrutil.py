#
# bmcmgrutil.py
# Platform-specific BMC config interface for SONiC
#


try:
    from sonic_bmcmgr.bmcmgr_base import BmcmgrBase
except ImportError as e:
    raise ImportError(str(e) + "- required module not found")

class BmcmgrUtil(BmcmgrBase):
    """Platform-specific BMCmgrutil class"""
    USER_ID = "2"
    USER_NAME = "root"
    DEFAULT_CHANNEL = "1"

    def __init__(self):
        BmcmgrBase.__init__(self)

    def init_unboxing(self):
        rv = self.set_password(self.DEFAULT_PASSWORD)
        return rv[0]

    def ip_string_to_hex_list(self, wip):
        ip = wip.split(".")
        return [hex(int(ip[0])), hex(int(ip[1])), hex(int(ip[2])), hex(int(ip[3]))]

    def add_whitelist(self, wip):
        iphex = self.ip_string_to_hex_list(wip)
        return self.run_command("ipmitool raw 0x32 0x76 0x00 0x01 {} {} {} {}".format(iphex[0], iphex[1], iphex[2], iphex[3]))

    def remove_whitelist(self, wip):
        iphex = self.ip_string_to_hex_list(wip)
        return self.run_command("ipmitool raw 0x32 0x76 0x04 0x01 {} {} {} {}".format(iphex[0], iphex[1], iphex[2], iphex[3]))

    def remove_all_whitelist(self):
        return self.run_command("ipmitool raw 0x32 0x76 0x08")

    def enable_whitelist(self):
        return [0, "Not support", ""]

    def disable_whitelist(self):
        return [0, "Not support", ""]

    def set_password(self, password):
        return self.run_command("ipmitool user set password {} {}".format(self.USER_ID, password))

