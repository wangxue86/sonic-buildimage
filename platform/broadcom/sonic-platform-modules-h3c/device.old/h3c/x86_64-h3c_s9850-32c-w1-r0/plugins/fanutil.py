import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base


STATUS_LED_COLOR_GREEN = "green"
STATUS_LED_COLOR_RED = "red"
STATUS_LED_COLOR_OFF = "off"

class FanUtil(Util_Base):
    """Platform-specific FanUtil class"""
    STATUS_REMOVED = 'unpresent'
    STATUS_INSERTED = 'present'
    STATUS_OK = 'ok'
    STATUS_SPEED_LOW = 'speed low'
    STATUS_SPEED_HIGH = 'speed high'
    led_status_value = {
        '0': STATUS_LED_COLOR_OFF,
        '1' : STATUS_LED_COLOR_GREEN,
        '3' : STATUS_LED_COLOR_RED
    }

    def __init__(self):
        self.fan_path = "/sys/switch/fan/"

    def get_fan_name(self, index):
        return "fan{}".format(index)

    def get_fan_num(self):
        path = os.path.join(self.fan_path, "num_fans")
        num = self.read_attr(path)
        return int(num)

    def get_fan_presence(self, index):
        path = os.path.join(self.fan_path, "fan{}".format(index), "status")
        status = self.read_attr(path)
        if status == '0':
            return False
        else:
            return True

    def get_fan_sn(self, index):
        path = os.path.join(self.fan_path, "fan{}".format(index), "sn")
        sn = self.read_attr(path)
        return sn

    def get_fan_type(self, index):
        path = os.path.join(self.fan_path, "fan{}".format(index), "product_name")
        model = self.read_attr(path)
        return model

    def get_fan_status(self, index):
        if not self.get_fan_presence(index):
            return self.STATUS_REMOVED
        for motor_index in range(0, 2):
            try:
                target_speed = int(self.get_fan_speed_target(index, motor_index))
                now_speed = int(self.get_fan_speed(index, motor_index))
                speed_tolerance = int(self.get_fan_speed_tolerance(index, motor_index))
            except (ValueError, TypeError):
                return self.STATUS_INSERTED
            if abs(target_speed - now_speed) >  speed_tolerance:
                if target_speed > now_speed:
                    self.old_status = self.STATUS_SPEED_LOW
                    return self.STATUS_SPEED_LOW
                return self.STATUS_SPEED_HIGH
        return self.STATUS_OK


    def get_fan_direction(self, index):
        path = os.path.join(self.fan_path, "fan{}".format(index),
                                "motor1/direction")
        direction = self.read_attr(path)
        return direction

    def get_fan_speed_ratio(self, index):
        motor0_path = os.path.join(self.fan_path, "fan{}".format(index),
                                "motor0", "ratio")
        motor1_path = os.path.join(self.fan_path, "fan{}".format(index),
                                "motor1", "ratio")
        try:
            ratio0 = int(self.read_attr(motor0_path))
            ratio1 = int(self.read_attr(motor1_path))
            ratio = (ratio0 + ratio1) / 2
        except (ValueError, TypeError):
            return "NA"
        return ratio

    def get_fan_speed(self, index, motor_index):
        path = os.path.join(self.fan_path, "fan{}".format(index),
                                "motor{}".format(motor_index), "speed")
        speed = self.read_attr(path)
        try:
            speed = int(speed)
        except (ValueError, TypeError):
            return "NA"
        return speed

    def get_fan_speed_tolerance(self, index, motor_index):
        path = os.path.join(self.fan_path, "fan{}".format(index),
                                "motor{}".format(motor_index), "speed_tolerance")
        speed_tolerance = self.read_attr(path)
        try:
            speed_tolerance = int(speed_tolerance)
        except (ValueError, TypeError):
            return "NA"
        return speed_tolerance

    def get_fan_speed_target(self, index, motor_index):
        path = os.path.join(self.fan_path, "fan{}".format(index),
                                    "motor{}".format(motor_index), "speed_target")
        speed_target = self.read_attr(path)
        try:
            speed_target = int(speed_target)
        except (ValueError, TypeError):
            return "NA"
        return speed_target

    def get_fan_led_status(self, index):
        path = os.path.join(self.fan_path, "fan{}".format(index), "led_status")
        led_status = self.read_attr(path)
        if led_status in self.led_status_value:
            return self.led_status_value[led_status]
        return "NA"

    def get_fan_firmware_version(self, index):
        path = os.path.join(self.fan_path, "fan{}".format(index), "hw_version")
        firmware_version = self.read_attr(path)
        return firmware_version

    def get_fan_motor_num(self, index):
        path = os.path.join(self.fan_path, "fan{}",format(index), "num_motors")
        motor_num = self.read_attr(path)
        try:
            motor_num = int(motor_num)
        except (ValueError, TypeError):
            return 0
        return motor_num

    def get_info(self):
        """
        Returns:
            A dict: {objs:[fan1_info_dict, fan2_info_dict,...], num: fan_nums}
        """
        fan_info_dict = {"objs":list(), "num": 0}

        fan_dict_keys = [
            "presence",
            "model",
            "serial",
            "status",
            "direction",
            "speed_ratio",
            "speed1",
            "speed_tolerance1",
            "speed_target1",
            "speed2",
            "speed_tolerance2",
            "speed_target2",
            "led_status",
            "firmware_version"
        ]

        fan_num = self.get_fan_num()
        if not fan_num:
            return fan_info_dict

        fan_list = []
        for index in range(1, fan_num + 1):
            fan_info = dict.fromkeys(fan_dict_keys, "NA")
            present = self.get_fan_presence(index)
            if not present:
                fan_info['presence'] = '0'
                fan_info['status'] = '0'
                fan_info['led_status'] = 'off'
                fan_list.append(fan_info)
                continue
            fan_info["presence"] = '1'
            fan_info["model"] = self.get_fan_type(index)
            fan_info["serial"] = self.get_fan_sn(index)
            fan_status = self.get_fan_status(index)
            if self.STATUS_OK == fan_status:
                fan_info["status"] = '1'
            else:
                fan_info["status"] = '0'
            fan_info["direction"] = self.get_fan_direction(index)
            fan_info["speed_ratio"] = str(self.get_fan_speed_ratio(index))
            fan_info["speed1"] = str(self.get_fan_speed(index, 0))
            fan_info["speed_tolerance1"] = str(self.get_fan_speed_tolerance(index, 0))
            fan_info["speed_target1"] = str(self.get_fan_speed_target(index, 0))
            fan_info["speed2"] = str(self.get_fan_speed(index, 1))
            fan_info["speed_tolerance2"] = str(self.get_fan_speed_tolerance(index, 1))
            fan_info["speed_target2"] = str(self.get_fan_speed_target(index, 1))
            fan_info["led_status"] = self.get_fan_led_status(index)
            fan_info["firmware_version"] = self.get_fan_firmware_version(index)
            fan_list.append(fan_info)
        fan_info_dict["objs"] = fan_list
        fan_info_dict["num"] = len(fan_list)
        return json.dumps(fan_info_dict, ensure_ascii = False, indent = 4)


