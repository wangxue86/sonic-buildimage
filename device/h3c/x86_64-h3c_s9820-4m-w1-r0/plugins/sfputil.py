# sfputil.py
#
# Platform-specific SFP transceiver interface for SONiC
#

try:
    import time
    import string
    from ctypes import create_string_buffer
    from sonic_sfp.sfputilbase import SfpUtilBase
except ImportError as e:
    raise ImportError("%s - required module not found" % str(e))



#from xcvrd
SFP_STATUS_REMOVED = '0'
SFP_STATUS_INSERTED = '1'

class SfpUtil(SfpUtilBase):
    """Platform-specific SfpUtil class"""

    PORT_START = 0
    PORT_END = 127
    PORTS_IN_BLOCK = 127
    QSFP_PORT_START = 0
    QSFP_PORT_END = 127
    PORT_NUM_PER_SLOT = 32

    #BASE_VAL_PATH = "/sys/class/i2c-adapter/i2c-{0}/{1}-0050/"
    BASE_VAL_PATH = "/sys/switch/xcvr/Eth{0}GE{1}-{2}/"

    _port_to_is_present = {}
    _port_to_lp_mode = {}

    _port_base_path = {}
    _port_to_eeprom_mapping = {}
    _port_to_eeprom_dom_mapping = {}
    _last_bitmap = 0

    @property
    def last_bitmap(self):
        return self._last_bitmap
    @last_bitmap.setter
    def last_bitmap(self, value):
        self._last_bitmap = value
    @property
    def port_start(self):
        return self.PORT_START

    @property
    def port_end(self):
        return self.PORT_END

    @property
    def qsfp_port_start(self):
        return self.QSFP_PORT_START

    @property
    def qsfp_port_end(self):
        return self.QSFP_PORT_END

    @property
    def qsfp_ports(self):
        return list(range(self.QSFP_PORT_START, self.PORTS_IN_BLOCK + 1))

    @property
    def port_to_eeprom_mapping(self):
        #print(traceback.extract_stack()[-2])
        return self._port_to_eeprom_mapping

    def __init__(self):
    
        #eeprom_path = '/sys/bus/i2c/devices/{0}-0050/eeprom'
        #eeprom_path = '/sys/switch/xcvr/Eth{0}G{1}/eeprom/raw'
        for x in range(self.port_start, self.port_end+1):
            speed = 25 if x < self.QSFP_PORT_START else 100
            slot  = int((x) / self.PORT_NUM_PER_SLOT + 1)
            port = (x) % self.PORT_NUM_PER_SLOT + 1
            
            self._port_base_path[x] = self.BASE_VAL_PATH.format(speed, slot, port)
            self.port_to_eeprom_mapping[x] = self._port_base_path[x] + "eeprom/raw"
            self._port_to_eeprom_dom_mapping[x] = self._port_base_path[x] + "eeprom/dom/dom_raw"
        self.last_bitmap = 0
        SfpUtilBase.__init__(self)


    def get_presence(self, port_num):
        # Check for invalid port_num
        if port_num < self.port_start or port_num > self.port_end:
            return False

        path = self._port_base_path[port_num] + ("pre_n" if port_num < self.QSFP_PORT_START else "module_present")
        
        try:
            val_file = open(path)
        except IOError as e:
            print("Error: unable to open file: %s" % str(e))
            return False

        content = val_file.readline().rstrip()
        val_file.close()

        # content is a string, either "0" or "1"
        if content == "1":
            return True

        return False

    def get_low_power_mode(self, port_num):
        # Check for invalid port_num
        if port_num < self.qsfp_port_start or port_num > self.qsfp_port_end:
            return False

        if not self.get_presence(port_num):
            return False
        try:
            val_file = open(self._port_base_path[port_num] + "lpmode", "r")
        except IOError as e:
            print("Error: unable to open file: %s" % str(e))
            return False
            
        content = val_file.readline().rstrip()
        val_file.close()
            
        # content is a string, either "0" or "1"
        if content == "1":
            return True
        else:
            return False

    def set_low_power_mode(self, port_num, lpmode):
        
        # Check for invalid port_num
        if port_num < self.qsfp_port_start or port_num > self.qsfp_port_end:
            print("Only qsfp supports lpmode!")
            return False
        set_val = "1" if lpmode == True else "0"
        val_file = None
        try:
            val_file = open(self._port_base_path[port_num] + "lpmode", "w")
            val_file.write(set_val)
            
        except IOError as e:
            print("Error: unable to open file: %s" % str(e))
            return False
        finally:
            if val_file is not None:
                val_file.close()
                time.sleep(0.01)
        return True
        
        '''
        try:
            eeprom = None

            if not self.get_presence(port_num):
                return False # Port is not present, unable to set the eeprom

            # Fill in write buffer
            regval = 0x3 if lpmode else 0x1 # 0x3:Low Power Mode, 0x1:High Power Mode
            buffer = create_string_buffer(1)
            buffer[0] = chr(regval)

            # Write to eeprom
            eeprom = open(self.port_to_eeprom_mapping[port_num], "r+b")
            eeprom.seek(93)
            eeprom.write(buffer[0])
            return True
        except IOError as e:
            print "Error: unable to open file: %s" % str(e)
            return False
        finally:
            if eeprom is not None:
                eeprom.close()
                time.sleep(0.01)
        '''
    def reset(self, port_num):
        
        if port_num < self.qsfp_port_start or port_num > self.qsfp_port_end:
            print("only qsfp supports reset!")
            return False
        
        reset_file = self._port_base_path[port_num] + "reset"
        val_file = None
        try:
            val_file = open(reset_file, "w")
            val_file.write("0")
            val_file.close()
            time.sleep(0.01)
            val_file = open(reset_file, "w")
            val_file.write("1")
            val_file.close()
            
        except IOError as e:
            print("Error: unable to open file: %s" % str(e))
            return False
        finally:
            if val_file is not None:
                val_file.close()
                time.sleep(0.01)
        return True
        
        '''
        cpld_i = self.get_cpld_num(port_num)
        cpld_ps = self._cpld_mapping[cpld_i]
        path = "/sys/bus/i2c/devices/{0}/module_reset_{1}"
        port_ps = path.format(cpld_ps, port_num)
        
        self.__port_to_mod_rst = port_ps
        try:
            reg_file = open(self.__port_to_mod_rst, 'r+', buffering=0)
        except IOError as e:
            print "Error: unable to open file: %s" % str(e)
            return False

        #toggle reset
        reg_file.seek(0)
        reg_file.write('1')
        time.sleep(1)
        reg_file.seek(0)
        reg_file.write('0')
        reg_file.close()
        
        return True
        '''
    @property
    def _get_present_bitmap(self):
        '''
        nodes = []
        rev = []
        port_num = [30,26]

        path = "/sys/bus/i2c/devices/{0}/module_present_all"
        cpld_i = self.get_cpld_num(self.port_start)
        cpld_ps = self._cpld_mapping[cpld_i]
        nodes.append((path.format(cpld_ps), port_num[0]))
        cpld_i = self.get_cpld_num(self.port_end)
        cpld_ps = self._cpld_mapping[cpld_i]
        nodes.append((path.format(cpld_ps), port_num[1]))

        bitmaps = ""
        for node in nodes:
            try:
                reg_file = open(node[0])
            except IOError as e:
                print "Error: unable to open file: %s" % str(e)
                return False
            bitmap = reg_file.readline().rstrip()
            bitmap = bin(int(bitmap, 16))[2:].zfill(node[1])
            rev.append(bitmap)
            reg_file.close()

        bitmaps = "".join(rev[::-1])
        bitmaps = hex(int(bitmaps, 2))
        '''
        bitmap = 0
        for port_num in range(self.PORT_START, self.PORT_END + 1):
            present = self.get_presence(port_num)
            bitmap += present << (port_num - 1);
        
        return bitmap

    def get_transceiver_change_event(self, timeout=0):
        forever = False
        now = time.time()
        port_dict = {}
        port = 0    
        if timeout == 0:
            forever = True
        elif timeout > 0:
            timeout = timeout / float(1000) # Convert to secs
        else:
            print("get_transceiver_change_event:Invalid timeout value", timeout)
            return False, {}
        while forever or timeout > 0:
            reg_value = self._get_present_bitmap
            
            changed_ports = self.last_bitmap ^ reg_value
            if changed_ports:
                for port in range (self.port_start, self.port_end+1):
                    # Mask off the bit corresponding to our port
                    mask = (1 << (port))
                    if changed_ports & mask:
                        if (reg_value & mask) == 0:
                           port_dict[port] = SFP_STATUS_REMOVED
                        else:
                           port_dict[port] = SFP_STATUS_INSERTED
                # Update cache
                self.last_bitmap = reg_value
                return True, port_dict      
            if timeout :
                timeout -= 1
            time.sleep(1)
        return True, {}
    #override the function in sfputilbase.py to fix sfp dom(0x51) eeprom path read
    def _get_port_eeprom_path(self, port_num, devid):   
        if port_num in list(self.port_to_eeprom_mapping.keys()):
            sysfs_sfp_i2c_client_eeprom_path = self._port_to_eeprom_dom_mapping[port_num] if devid == self.DOM_EEPROM_ADDR else self.port_to_eeprom_mapping[port_num]

        #print(sysfs_sfp_i2c_client_eeprom_path);
        return sysfs_sfp_i2c_client_eeprom_path

