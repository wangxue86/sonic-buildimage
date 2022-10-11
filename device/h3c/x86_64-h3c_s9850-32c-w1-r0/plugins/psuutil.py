import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base

STATUS_LED_COLOR_GREEN = "green"
STATUS_LED_COLOR_RED = "red"
STATUS_LED_COLOR_OFF = "off"


class PsuUtil(Util_Base):
    led_status_value = {
        '0': STATUS_LED_COLOR_OFF,
        '1' : STATUS_LED_COLOR_GREEN,
        '3' : STATUS_LED_COLOR_RED
    }

    def __init__(self):
        self.psu_path = "/sys/switch/psu/"

    def get_num_psus(self):
        path = os.path.join(self.psu_path, "num_psus")
        num = self.read_attr(path)
        try:
            num = int(num)
        except (ValueError, TypeError):
            return 0

        return num

    def get_psu_status(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "status")
        status = self.read_attr(path)

        return status == '1'

    def get_psu_presence(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "status")
        presence = self.read_attr(path)
        if not presence:
            return False

        return presence != '0'

    def get_psu_sn(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "sn")
        sn = self.read_attr(path)
        return sn

    def get_psu_type(self, index):
        psu_sn = self.get_psu_sn(index)
        psu_keywords = ['0231AG7U','0231ABV7','9803A00R','9803A05U']
        if any(k in psu_sn for k in psu_keywords):
            return "PSR1600B-12A-B"
        
        path = os.path.join(self.psu_path, "psu{}".format(index), "product_name")
        type = self.read_attr(path)

        return type

    def get_in_type(self, index):
        """
        not support
        """
        path = os.path.join(self.psu_path, "psu{}".format(index), "in_vol_type")
        type = self.read_attr(path)

        return type

    def get_fan_speed(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "fan")
        fan_speed = self.read_attr(path)
        try:
            fan_speed = int(float(fan_speed))
        except (ValueError, TypeError):
            return "NA"
        return fan_speed

    def get_temp(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "temp_input")
        psu_temp = self.read_attr(path)
        try:
            temp = round(float(psu_temp), 2)
        except (ValueError, TypeError):
            return "NA"
        return temp

    def get_in_vol(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "in_vol")
        in_vol = self.read_attr(path)
        try:
            in_vol = round(float(in_vol), 2)
        except (ValueError, TypeError):
            return "NA"
        return in_vol

    def get_in_power(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "in_power")
        in_power = self.read_attr(path)
        try:
            in_power = round(float(in_power), 2)
        except (ValueError, TypeError):
            return "NA"
        return in_power

    def get_in_curr(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "in_curr")
        in_curr = self.read_attr(path)
        try:
            in_curr = round(float(in_curr), 2)
        except (ValueError, TypeError):
            return "NA"
        return in_curr

    def get_out_vol(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "out_vol")
        out_vol = self.read_attr(path)
        try:
            out_vol = round(float(out_vol), 2)
        except (ValueError, TypeError):
            return "NA"
        return out_vol

    def get_out_power(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "out_power")
        out_power = self.read_attr(path)
        try:
            out_power = round(float(out_power), 2)
        except (ValueError, TypeError):
            return "NA"
        return out_power

    def get_out_curr(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "out_curr")
        out_curr = self.read_attr(path)
        try:
            out_curr = round(float(out_curr), 2)
        except (ValueError, TypeError):
            return "NA"
        return out_curr

    def get_led_status(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "led_status")
        led_status =  self.read_attr(path)
        if led_status in self.led_status_value:
            return self.led_status_value[led_status]
        return "NA"

    def get_firmware_version(self, index):
        path = os.path.join(self.psu_path, "psu{}".format(index), "hw_version")
        firware_version = self.read_attr(path).strip('\x03')
        return firware_version


    def get_info(self):
        """
        Returns:
            A dict: {objs:[psu1_info_dict, psu2_info_dict,...], num: psu_nums}
        """
        psu_info_dict = {"objs":list(), "num": 0}

        psu_dict_keys = [
            "presence",
            "model",
            "serial",
            "status",
            "in_type",
            "fan_speed",
            "temp",
            "in_vol",
            "in_power",
            "in_curr",
            "out_vol",
            "out_power",
            "out_curr",
            "led_status",
            "firmware_version"
        ]

        psu_num = self.get_num_psus()
        if not psu_num:
            return psu_info_dict

        psu_list = []
        for index in range(1, psu_num + 1):
            psu_info = dict.fromkeys(psu_dict_keys, "NA")
            present = self.get_psu_presence(index)
            if not present:
                psu_info['presence'] = '0'
                psu_info["status"] = '0'
                psu_info["led_status"] = 'off'
                psu_list.append(psu_info)
                continue
            psu_info["presence"] = '1'
            psu_info["model"] = self.get_psu_type(index)
            psu_info["serial"] = self.get_psu_sn(index)
            psu_status = self.get_psu_status(index)
            if psu_status:
                psu_info["status"] = '1'
            else:
                psu_info["status"] = '0'
            psu_info["in_type"] = self.get_in_type(index)
            psu_info["fan_speed"] = str(self.get_fan_speed(index))
            psu_info["temp"] = str(self.get_temp(index))
            psu_info["in_vol"] = str(self.get_in_vol(index))
            psu_info["in_power"] = str(self.get_in_power(index))
            psu_info["in_curr"] = str(self.get_in_curr(index))
            psu_info["out_vol"] = str(self.get_out_vol(index))
            psu_info["out_power"] = str(self.get_out_power(index))
            psu_info["out_curr"] = str(self.get_out_curr(index))
            psu_info["led_status"] = self.get_led_status(index)
            psu_info["firmware_version"] = self.get_firmware_version(index)
            psu_list.append(psu_info)
        psu_info_dict["objs"] = psu_list
        psu_info_dict["num"] = psu_num
        return json.dumps(psu_info_dict, ensure_ascii = False, indent = 4)