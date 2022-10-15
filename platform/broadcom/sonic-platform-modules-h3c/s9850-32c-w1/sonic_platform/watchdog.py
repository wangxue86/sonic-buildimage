#!/usr/bin/env python

########################################################################
#MODULE_AUTHOR("Qianchaoyang <qian.chaoyang@h3c.com><li.tingA@h3c.com>");
#MODULE_DESCRIPTION("h3c watchdog management");
#
########################################################################

try:
    import time
    import ctypes
    from sonic_platform_base.watchdog_base import WatchdogBase
except ImportError as _e:
    raise ImportError(str(_e) + "- required module not found")


class _timespec(ctypes.Structure):
    _fields_ = [
        ('tv_sec', ctypes.c_long),
        ('tv_nsec', ctypes.c_long)
    ]

class Watchdog(WatchdogBase):
    """
    Abstract base class for interfacing with a hardware watchdog module
    """
    watchdog_path = '/sys/switch/watchdog/'

    armed_time = 0
    timeout = 0
    CLOCK_MONOTONIC = 1

    def __init__(self):
        self._librt = ctypes.CDLL('librt.so.1', use_errno=True)
        self._clock_gettime = self._librt.clock_gettime
        self._clock_gettime.argtypes = [ctypes.c_int, ctypes.POINTER(_timespec)]

        #super(Watchdog, self).__init__()

    def _get_time(self):
        """
        To get clock monotonic time
        """
        _ts = _timespec()
        if self._clock_gettime(self.CLOCK_MONOTONIC, ctypes.pointer(_ts)) != 0:
            return 0
        return _ts.tv_sec + _ts.tv_nsec * 1e-9

    def arm(self, seconds):
        w_file = None
        file_path = self.watchdog_path+"arm"

        if seconds > 255 * 1.6:
            seconds = 255 * 1.6
        try:
            with open(file_path, "w") as w_file:
                w_file.write(str(seconds))
        except IOError as _e:
            print("Error: unable to open file: %s" %(str(_e)))
            return False
        # actual_value = (seconds*10/16)*1.6
        time.sleep(0.1)
        self.timeout = seconds
        self.armed_time = self._get_time()
        return seconds

    def disarm(self):
        """
        Disarm the hardware watchdog

        Returns:
        A boolean, True if watchdog is disarmed successfully, False
        if not
        """

        file_path = self.watchdog_path+"disarm"
        try:
            with open(file_path, "w") as val_file:
                val_file.write("1")
        except IOError as _e:
            print("Error: unable to open file: %s" %(str(_e)))
            return False
        time.sleep(0.01)
        self.armed_time = 0
        self.timeout = 0
        return True

    def is_armed(self):
        """
        Retrieves the armed state of the hardware watchdog.

        Returns:
        A boolean, True if watchdog is armed, False if not
        """
        armd = 0
        file_path = self.watchdog_path + "is_armed"
        try:
            with open(file_path, 'r') as armed_prs:
                armd = int(armed_prs.read())
        except IOError:
            return False
        return bool(armd)

    def get_remaining_time(self):
        """
        If the watchdog is armed, retrieve the number of seconds
        remaining on the watchdog timer

        Returns:
        An integer specifying the number of seconds remaining on
        their watchdog timer. If the watchdog is not armed, returns
        -1.
        """
        if not self.is_armed():
            return -1

        if self.armed_time > 0 and self.timeout != 0:
            cur_time = self._get_time()
            if cur_time <= 0:
                return 0
        diff_time = int(cur_time - self.armed_time)
        if diff_time > self.timeout:
            return self.timeout
        return self.timeout - diff_time
