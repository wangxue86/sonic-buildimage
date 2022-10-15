#!/usr/bin/env python2

#
#Version: 1.0
########################################################################
#
# provides the fan status which are available in the platform
#MODULE_AUTHOR("wangxue <wang.xue@h3c.com>");
#MODULE_DESCRIPTION("h3c fan drawer management");
#
#############################################################################

from sonic_platform_base.fan_drawer_base import FanDrawerBase

#currently, we have only one fan for each drawer

class FanDrawer(FanDrawerBase):

    def __init__(self, fan_drawer_index, fan_list):
        FanDrawerBase.__init__(self)
        self._fan_list = fan_list
        self._fan_drawer_index = fan_drawer_index
        self._fan_drawer_name = str(fan_drawer_index + 1)

    def get_name(self):
        return self._fan_drawer_name

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
                             index, len(self._fan_list)-1))

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
        ret = True
        for fan in self._fan_list:
            ret = ret & fan.set_status_led(color)

        return ret
        #raise NotImplementedError

    def get_status_led(self):
        """
        Gets the state of the fan drawer LED

        Returns:
            A string, one of the predefined STATUS_LED_COLOR_* strings above
        """
        color = self._fan_list[0].get_status_led()
        return color

        #raise NotImplementedError

    def get_maximum_consumed_power(self):
        """
        Retrives the maximum power drawn by Fan Drawer

        Returns:
            A float, with value of the maximum consumable power of the
            component.
        """
        raise NotImplementedError
