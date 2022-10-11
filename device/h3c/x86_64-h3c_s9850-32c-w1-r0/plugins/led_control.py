try:
    from sonic_led.led_control_base import LedControlBase
except ImportError, e:
    raise ImportError(str(e) + " - required module not found")

class LedControl(LedControlBase):
    def __init__(self):
        self.led_path = "/sys/switch/sysled/"

    def port_link_state_change(self, portname, state):
        return
