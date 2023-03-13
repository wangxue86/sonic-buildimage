try:
    import os
    from sonic_platform_base.fan_drawer_base import FanDrawerBase
    from sonic_platform.fan import Fan
    from vendor_sonic_platform.devcfg import Devcfg
    from sonic_platform import log_wrapper
except ImportError as e:
    raise ImportError(str(e) + "- required module not found")

#
# fan_drawer.py
#
# platform-specific class with which
# to interact with a fan drawer module in SONiC
#


class FanDrawer(FanDrawerBase):
    """
    fan drawer class
    """

    def __init__(self, index):
        self._fan_list = []
        self._index = index 
        log_wrapper.log_init(self)
        for fan_index in range(0, Devcfg.FAN_NUM):
            self._fan_list.append(Fan(fan_index))
            self.logger.log_debug('fan{} inited.'.format(fan_index))

    def get_name(self):
        return 'drawer{}'.format(self._index)
        
    def get_num_fans(self):
        """
        Retrieves the number of fans available on this fan drawer

        Returns:
            An integer, the number of fan modules available on this fan drawer
        """
        return len(self._fan_list)

    def get_all_fans(self):
        """
        Retrieves all fan modules available on this fan drawer

        Returns:
            A list of objects derived from FanBase representing all fan
            modules available on this fan drawer
        """
        return self._fan_list

    def get_fan(self, index):
        """
        Retrieves fan module represented by (0-based) index <index>

        Args:
            index: An integer, the index (0-based) of the fan module to
            retrieve

        Returns:
            An object dervied from FanBase representing the specified fan
            module
        """
        fan = None

        try:
            fan = self._fan_list[index]
        except IndexError:
            sys.stderr.write("Fan index {} out of range (0-{})\n".format(
                             index, len(self._fan_list) - 1))

        return fan

    def set_status_led(self, color):
        """
        Sets the state of the fan drawer status LED

        Args:
            color: A string representing the color with which to set the
                   fan drawer status LED

        Returns:
            bool: True if status LED state is set successfully, False if not
        """
        #raise NotImplementedError
        self.status_led = color
        return True

    def get_status_led(self, color):
        """
        Gets the state of the fan drawer LED

        Returns:
            A string, one of the predefined STATUS_LED_COLOR_* strings above
        """
        #raise NotImplementedError
        return self.status_led

    def get_maximum_consumed_power(self):
        """
        Retrives the maximum power drawn by Fan Drawer

        Returns:
            A float, with value of the maximum consumable power of the
            component.
        """
        #raise NotImplementedError
        return 1.0
