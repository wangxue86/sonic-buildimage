def log_init(provider):
    from sonic_platform_base.device_base import DeviceBase
    if 'log_debug' in dir(DeviceBase):
        provider.logger = provider
    else:
        from sonic_py_common.logger import Logger
        provider.logger = Logger()
