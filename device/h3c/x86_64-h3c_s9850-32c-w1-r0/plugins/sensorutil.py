import os.path
import sys
import json

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from common import Util_Base

class SensorUtil(Util_Base):
    def __init__(self):
        self.sensor_path = "/sys/switch/sensor/"
        self.temp_sensor_path = "/sys/switch/sensor/temp{}"
        self.vol_sensor_path = "/sys/switch/sensor/in0"
        self.temp_sensor_num = self._get_temp_num()
        self.temp_vol_num = self._get_temp_num()

    def _get_temp_num(self):
        path = os.path.join(self.sensor_path, "num_temp_sensors")
        temp_num = self.read_attr(path)
        return int(temp_num)

    def _get_vol_num(self):
        path = os.path.join(self.sensor_path, "in_num_sensors")
        temp_num = self.read_attr(path)
        return int(temp_num)

    def get_alias(self, index):
        if index <= self.temp_sensor_num:
            path = os.path.join(self.temp_sensor_path.format(index), "temp_alias")
            alias = self.read_attr(path)
        else:
            path = os.path.join(self.vol_sensor_path, "in_alias")
            alias = self.read_attr(path)
        return alias

    def get_type(self, index):
        if index <= self.temp_sensor_num:
            path = os.path.join(self.temp_sensor_path.format(index), "temp_type")
            type = self.read_attr(path)
        else:
            path = os.path.join(self.vol_sensor_path, "in_type")
            type = self.read_attr(path)
        return type

    def get_high_threshold(self, index):
        if index <= self.temp_sensor_num:
            path = os.path.join(self.temp_sensor_path.format(index), "temp_max")
            high_threshold  = self.read_attr(path)
        else:
            path = os.path.join(self.vol_sensor_path, "in_max")
            high_threshold = self.read_attr(path)
        try:
            high_threshold = round(float(high_threshold), 2)
        except (ValueError, TypeError):
            return "NA"
        return high_threshold

    def get_low_threshold(self, index):
        if index <= self.temp_sensor_num:
            path = os.path.join(self.temp_sensor_path.format(index), "temp_min")
            low_threshold  = self.read_attr(path)
        else:
            path = os.path.join(self.vol_sensor_path, "in_min")
            low_threshold = self.read_attr(path)
        try:
            low_threshold = round(float(low_threshold), 2)
        except (ValueError, TypeError):
            return "NA"
        return low_threshold

    def get_value(self, index):
        if index <= self.temp_sensor_num:
            path = os.path.join(self.temp_sensor_path.format(index), "temp_input")
            value  = self.read_attr(path)
        else:
            path = os.path.join(self.vol_sensor_path, "in_input")
            value = self.read_attr(path)
        try:
            value = round(float(value), 2)
        except (ValueError, TypeError):
            return "NA"
        return value

    def get_info(self):
        device_fileds = [
            "model",
            "alias",
            "type",
            "max",
            "min",
            "value"
        ]
        sensor_info_dict = {"objs": list(), "num": 0}
        sensor_num = self.temp_sensor_num + self.temp_vol_num
        sensor_list = []
        for index in range(1, sensor_num + 1):
            sensor_info = dict.fromkeys(device_fileds, "NA")
            sensor_info["model"] = self.get_alias(index)
            sensor_info["alias"] = self.get_alias(index)
            sensor_info["type"] = self.get_type(index)
            sensor_info["max"] = str(self.get_high_threshold(index))
            sensor_info["min"] = str(self.get_low_threshold(index))
            sensor_info["value"] = str(self.get_value(index))
            sensor_list.append(sensor_info)
        sensor_info_dict["objs"] = sensor_list
        sensor_info_dict["num"] = sensor_num
        return json.dumps(sensor_info_dict, ensure_ascii = False,indent=4)

