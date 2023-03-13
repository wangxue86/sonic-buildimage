#include "bsp_base.h"

#define MODULE_NAME "cpld"
#define CPLD_OPER_WARN_MSG0  "\n** !!!! cpld operation is dangerous, which may cause reboot/halt !!!!"
#define CPLD_OPER_WARN_MSG1  "\n** use 'echo offset:value' to set register value"
#define CPLD_OPER_WARN_MSG2  "\n** offset & value must be hex value, example: 'echo 0x0:0x17 > board_cpld'\n"
#define CPU_CPLD_NAME     cpu_cpld
#define BOARD_CPLD_NAME   board_cpld
#define SLOT_CPLD_NAME    "slot%d_cpld"
#define BUFF_NAME         buffer

enum CPLD_SENSOR_ATTR {
    NUM_CPLDS,
    ALIAS,      
    TYPE,    
    HW_VERSION,
    BROAD_VERSION,
    REG_RESET_CPU,
    REG_MAC_INIT_OK,
    HW_REBOOT,
    I2C_WDT_CTRL,
    CPU_INIT_OK,
    I2C_WDT_FEED
};

struct reboot_reason {
    int reason_code;
    int is_cold_reset;
    int is_cpu_thermal;
    int is_power;
    int is_watchdog;
    int is_boot_software;
    int is_soft_reset;
    int is_mainboard_reset;
};

u8 buffer_char = 0;
static int loglevel = DEBUG_INFO | DEBUG_ERR;
static struct kobject *kobj_cpld_debug = NULL;
static struct kobject *kobj_cpld_root = NULL;
static struct kobject *kobj_cpld_sub[MAX_CPLD_NUM] = {0};
static struct reboot_reason last_reboot_reason = {0};

static void release_kobj(void);
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt,##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)

int bsp_cpld_get_cpld_version(int cpld_index, u8 *cpld_version_hex)
{
    board_static_data *bd = bsp_get_board_data();

    if (!bd) 
        return ERROR_FAILED;
    else 
        return bsp_cpld_read_part(cpld_version_hex, bd->cpld_addr_cpld_ver[cpld_index], bd->cpld_mask_cpld_ver[cpld_index], bd->cpld_offs_cpld_ver[cpld_index]);
}

int bsp_cpld_get_board_version(u8 * board_version)
{
    board_static_data *bd = bsp_get_board_data();

    if (!bd) 
        return ERROR_FAILED;
    else 
        return bsp_cpld_read_part(board_version, bd->cpld_addr_pcb_ver, bd->cpld_mask_pcb_ver, bd->cpld_offs_pcb_ver);
}

int bsp_set_mac_init_ok(u8 bit)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();

    if (!bd) 
        return ERROR_FAILED;
    
    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_mac_init_ok, "mainboard mac_init_ok reg is not defined!");
    ret = bsp_cpld_set_bit(bd->cpld_addr_mac_init_ok, bd->cpld_offs_mac_init_ok, bit);

exit:
    return ret;
}

ssize_t bsp_cpld_sysfs_show_cpld(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    u16 i,j;
	size_t index = 0;
    u16 cpld_size = 0;
	u8 temp_value = 0;
	int slot_index = -1;
	int ret = ERROR_SUCCESS;
	int start_addr = 0;

	int (* cpld_read_byte_func)(u8 *value, u16 offset) = NULL;

	if (strcmp(attr->attr.name, __stringify(CPU_CPLD_NAME)) == 0) {
	    cpld_size = (u16) bsp_get_cpu_cpld_size();
		cpld_read_byte_func = bsp_cpu_cpld_read_byte;
		start_addr = bsp_get_board_data()->cpld_hw_addr_cpu;
	} else if (strcmp(attr->attr.name, __stringify(BOARD_CPLD_NAME)) == 0) {
	    cpld_size = (u16) bsp_cpld_get_size();
		cpld_read_byte_func = bsp_cpld_read_byte;
		start_addr = bsp_get_board_data()->cpld_hw_addr_board;
	} else if (sscanf(attr->attr.name, SLOT_CPLD_NAME, &slot_index) <= 0) {
	    DBG_ECHO(DEBUG_ERR, "Invalid attrbute '%s'\n", attr->attr.name);
        return sprintf(buf, "Invalid attrbute '%s'\n", attr->attr.name);
	} else if (slot_index > MAX_SLOT_NUM) {
	    DBG_ECHO(DEBUG_ERR, "Invalid slot index %d\n", slot_index);
		return sprintf(buf, "Invalid slot index %d\n", slot_index);
	} else {
	    slot_index = slot_index - 1;
        cpld_size = (u16) bsp_cpld_get_slot_size(slot_index);
		cpld_read_byte_func = NULL;
		start_addr = bsp_get_board_data()->cpld_hw_addr_slot[slot_index];
	}

    index += sprintf(buf + index, CPLD_OPER_WARN_MSG0);
	index += sprintf(buf + index, CPLD_OPER_WARN_MSG1);
	index += sprintf(buf + index, CPLD_OPER_WARN_MSG2);
    index += sprintf(buf + index, "\nhw address start: 0x%4x\n", start_addr);

    for (i = 0; i < cpld_size; i+=16) {
        if (index >= PAGE_SIZE - 200) {
            DBG_ECHO(DEBUG_INFO, "buf size reached %d, break to avoid overflow.", (int)index);
            break;
        }
        
        index += sprintf(buf + index, "0x%04x: ", i);
        for (j = 0; j < 16; j++) {
            ret = cpld_read_byte_func == NULL ? bsp_slot_cpld_read_byte(slot_index, &temp_value, i+j): cpld_read_byte_func(&temp_value, i + j);
            if (ret == ERROR_SUCCESS) {
                index += sprintf(buf + index, "%02x %s", temp_value, j == 7 ? " ": "");
            } else {
                index += sprintf(buf + index, "XX ");
		    }
        }
        
		index += sprintf(buf + index, "\n");
    }
   
	return index;
}

static ssize_t bsp_cpld_sysfs_set_cpld(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    unsigned long temp_offset = 0x0;
	unsigned long temp_value = 0x0;
	int slot_index = -1;
	int ret = ERROR_SUCCESS;

    if (sscanf(buf, "0x%lx:0x%lx", &temp_offset, &temp_value) < 2) {
        DBG_ECHO(DEBUG_ERR, "Invalid format '%s'\n", buf);
		return count;
    }

	if (strcmp(attr->attr.name, __stringify(CPU_CPLD_NAME)) == 0) {
	    ret = bsp_cpu_cpld_write_byte((u8)temp_value, (u16)temp_offset);
	} else if (strcmp(attr->attr.name, __stringify(BOARD_CPLD_NAME)) == 0) {
		ret = bsp_cpld_write_byte((u8)temp_value, (u16)temp_offset);
	} else if (sscanf(attr->attr.name, SLOT_CPLD_NAME, &slot_index) <= 0) {
	    DBG_ECHO(DEBUG_ERR, "Invalid attrbute '%s'\n", attr->attr.name);
        return count;
	} else if (slot_index > MAX_SLOT_NUM) {
	    DBG_ECHO(DEBUG_ERR, "Invalid slot index %d\n", slot_index);
		return count;
	} else {
	
	    slot_index = slot_index - 1;
		ret = bsp_slot_cpld_write_byte(slot_index, (u8)temp_value, (u16)temp_offset);
	}

	if (ret != ERROR_SUCCESS) {
	    DBG_ECHO(DEBUG_ERR, "slot(%d) write 0x%lx to offset 0x%lx failed!" , slot_index,  temp_value, temp_offset);
	}

	return count;
}

static ssize_t bsp_cpld_sysfs_buffer_show(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%c", buffer_char);
}

static ssize_t bsp_cpld_sysfs_buffer_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int temp_value = 0;
    int slot_index = 0;
    int ret = ERROR_SUCCESS;

    if (strstr(buf, "cpu_read") != NULL) {
        if (sscanf(buf, "cpu_read_0x%x", &temp_value) == 1) {
	        ret = bsp_cpu_cpld_read_byte(&buffer_char, (u16)temp_value);
        } else {
            DBG_ECHO(DEBUG_ERR, "'%s' format error! 'cpu_read_0xaa'", buf);
        }
	} else if (strstr(buf, "brd_read") != NULL) {
        if (sscanf(buf, "brd_read_0x%x", &temp_value) == 1) {
	        ret = bsp_cpld_read_byte(&buffer_char, (u16)temp_value);
        }
        else {
            DBG_ECHO(DEBUG_ERR, "'%s' format error! 'brd_read_0xaa'", buf);
        }
    }
    else if (strstr(buf, "slot_read") != NULL) {
        if (sscanf(buf, "slot_read_%d_0x%x", &slot_index, &temp_value) == 2) {
	        ret = bsp_slot_cpld_read_byte(slot_index, &buffer_char, (u16)temp_value);
        } else {
            DBG_ECHO(DEBUG_ERR, "'%s' format error! 'slot_read_n_0xaa'", buf);
        }
    } else {
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_ERR, "'%s' format error!", buf);
    }

    if (ret != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "failed for %s", buf);
    }
	
    return count;
}

static ssize_t bsp_cpld_custom_sysfs_read(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;
    u32 cpld_index = 0;
    u8 temp = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    board_static_data *bd = bsp_get_board_data();

    sscanf(((struct kobject*)kobjs)->name,"cpld%d", &cpld_index);

    switch(attr->index) {
    case ALIAS:
        if (cpld_index < bd->cpld_num && bd->cpld_location_describe[cpld_index] != NULL) {
            len = sprintf(buf, "%s\n", bd->cpld_location_describe[cpld_index]);
        } else {
            len = sprintf(buf, "%s\n", "Unknown");
        }
        break;
    case TYPE:
        if (cpld_index < bd->cpld_num && bd->cpld_type_describe[cpld_index] != NULL) {
            len = sprintf(buf, "%s\n", bd->cpld_type_describe[cpld_index]);
        } else {
            len = sprintf(buf, "%s\n", "Unknown");
        }
        break;
    case HW_VERSION:
        if (bsp_cpld_get_cpld_version (cpld_index, &temp) == ERROR_SUCCESS) {
            len = sprintf(buf, "%d\n", temp);
        } else {
            DBG_ECHO(DEBUG_ERR, "get cpld %d version failed", cpld_index);
            len = sprintf(buf, "%s\n", "N/A");
        }
        break;
    case BROAD_VERSION:
        if (bsp_cpld_get_board_version(&temp) == ERROR_SUCCESS) {
            len = sprintf(buf, "%d\n", temp);
        } else {
            DBG_ECHO(DEBUG_ERR, "get pcb version failed");
            len = sprintf(buf, "%s\n", "N/A");
        }
        break;
    case REG_RESET_CPU:
        if (bsp_cpld_get_bit(bd->cpld_addr_cpu_rst, bd->cpld_offs_cpu_rst, &temp) != ERROR_SUCCESS) {
            len = sprintf(buf, "%s\n", "N/A");
        } else {
            len = sprintf(buf, "%d\n", (int)temp);
        }
        break;
    case I2C_WDT_CTRL:
        if (bsp_cpld_read_byte(&temp, bd->cpld_addr_i2c_wdt_ctrl) != ERROR_SUCCESS) {
            len = sprintf(buf, "%s\n", "N/A");
        }
        else {
            len = sprintf(buf, "%d\n", (int)temp);
        }
        break;
    case CPU_INIT_OK:
        if (bsp_cpld_read_byte(&temp, bd->cpld_addr_cpu_init_ok) != ERROR_SUCCESS) {
            len = sprintf(buf, "%s\n", "N/A");
        } else {
            len = sprintf(buf, "%d\n", (int)temp);
        }
        break;
    case I2C_WDT_FEED:
        if (bsp_cpld_read_byte(&temp, bd->cpld_addr_i2c_wdt_feed) != ERROR_SUCCESS) {
            len = sprintf(buf, "%s\n", "N/A");
        } else {
            len = sprintf(buf, "%d\n", (int)temp);
        }
        break;
    case REG_MAC_INIT_OK:
        if (bsp_cpld_get_bit(bd->cpld_addr_mac_init_ok, bd->cpld_offs_mac_init_ok, &temp) != ERROR_SUCCESS) {
            len = sprintf(buf, "%s\n", "N/A");
        } else {
            len = sprintf(buf, "%d\n", (int)temp);
        }
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "no attribute index %d", attr->index);
    }
    
    return len;
}

static ssize_t bsp_cpld_custom_root_sysfs_read (struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;
    board_static_data *bd = bsp_get_board_data();
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    
    switch(attr->index) {
    case NUM_CPLDS:
        len = sprintf(buf, "%d\n", (int)bd->cpld_num);
        break;
    case HW_REBOOT:
        len += sprintf(buf + len, "reason code=0x%x\n", last_reboot_reason.reason_code);
        len += sprintf(buf + len, "RESET_TYPE_COLD=%d\n", last_reboot_reason.is_cold_reset);
        len += sprintf(buf + len, "RESET_TYPE_CPU_THERMAL=%d\n", last_reboot_reason.is_cpu_thermal);
        len += sprintf(buf + len, "RESET_TYPE_POWER=%d\n", last_reboot_reason.is_power);
        len += sprintf(buf + len, "RESET_TYPE_WDT=%d\n", last_reboot_reason.is_watchdog);
        len += sprintf(buf + len, "RESET_TYPE_BOOT_SW=%d\n", last_reboot_reason.is_boot_software);
        len += sprintf(buf + len, "RESET_TYPE_SOFT=%d\n", last_reboot_reason.is_soft_reset);
        len += sprintf(buf + len, "RESET_TYPE_MAINBOARD_REST=%d\n", last_reboot_reason.is_mainboard_reset);
        break;	    
    default:
        DBG_ECHO(DEBUG_ERR, "no attribute index %d", attr->index);
    }

    return len;
}


static ssize_t bsp_cpld_custom_sysfs_write(struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    board_static_data *bd = bsp_get_board_data();

    if (!bd) {
        return -EINVAL;
    }

    switch(attr->index) {
    case REG_RESET_CPU:
        if (sscanf(buf, "%d", &temp) == 1) {
            temp = (temp != 0) ? 1 : 0;
            if (bsp_cpld_set_bit(bd->cpld_addr_cpu_rst, bd->cpld_offs_cpu_rst, (u8)temp) != ERROR_SUCCESS) {
                DBG_ECHO(DEBUG_ERR, "cpu reset set bit %d failed", temp);
            }
        }
        break;
    case I2C_WDT_CTRL:
        if (sscanf(buf, "%d", &temp) == 1) {
            if (bsp_cpld_write_byte(temp, bd->cpld_addr_i2c_wdt_ctrl) != ERROR_SUCCESS) {
                DBG_ECHO(DEBUG_ERR, "set cpu_init_ok %d failed", temp);
            }
        } else {
            count = -EINVAL;
        }
        break;
    case CPU_INIT_OK:
        if (sscanf(buf, "%d", &temp) == 1)
        {
            if (bsp_cpld_write_byte(temp, bd->cpld_addr_cpu_init_ok) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "set cpu_init_ok %d failed", temp);
            }
        }
        else
        {
            count = -EINVAL;
        }
        break;
    case I2C_WDT_FEED:
        if (sscanf(buf, "%d", &temp) == 1) {
            if (bsp_cpld_write_byte(temp, bd->cpld_addr_i2c_wdt_feed) != ERROR_SUCCESS) {
                DBG_ECHO(DEBUG_ERR, "set i2c_wdt_feed %d failed", temp);
            }
        } else {
            count = -EINVAL;
        }
        break;
    case REG_MAC_INIT_OK:
        if (sscanf(buf, "%d", &temp) == 1) {
            temp = (temp != 0) ? 1 : 0;
            if (bsp_set_mac_init_ok((u8)temp) != ERROR_SUCCESS) {
                DBG_ECHO(DEBUG_ERR, "set mac_init_ok %d failed", temp);
            }
        } else {
            count = -EINVAL;
        }
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "no support attribute index %d", attr->index);
    }

    return count;
}

static int bsp_cpld_record_reboot_reason(void)
{
    u8 temp = 0;
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpu_cpld_read_byte(&temp, bd->cpld_addr_reset_type_cold);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld read HW REBOOT failed!");
    
    last_reboot_reason.reason_code = temp;
    last_reboot_reason.is_cold_reset      = (int)((temp & bd->cpld_mask_reset_type_cold) >> bd->cpld_offs_reset_type_cold);
    last_reboot_reason.is_cpu_thermal     = (int)((temp & bd->cpld_mask_reset_type_cpu_thermal) >> bd->cpld_offs_reset_type_cpu_thermal);
    last_reboot_reason.is_power           = (int)((temp & bd->cpld_mask_reset_type_power_en) >> bd->cpld_offs_reset_type_power_en);
    last_reboot_reason.is_watchdog        = (int)((temp & bd->cpld_mask_reset_type_wdt) >> bd->cpld_offs_reset_type_wdt);
    last_reboot_reason.is_boot_software   = (int)((temp & bd->cpld_mask_reset_type_boot_sw) >> bd->cpld_offs_reset_type_boot_sw);
    last_reboot_reason.is_soft_reset      = (int)((temp & bd->cpld_mask_reset_type_soft) >> bd->cpld_offs_reset_type_soft);
    last_reboot_reason.is_mainboard_reset = (int)((temp & bd->cpld_mask_reset_type_mlb) >> bd->cpld_offs_reset_type_mlb);

    bsp_cpu_cpld_write_byte(bd->cpld_mask_clear_reset_flag, bd->cpld_addr_clear_reset_flag);  
exit:
    return ret;   
}

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static ssize_t bsp_default_debug_help (char *buf)
{
    board_static_data *bd = bsp_get_board_data();
    ssize_t index = 0;

    if (PDT_TYPE_S6850_48Y8C_W1 == bd->product_type) {
        index += sprintf (buf + index, "%s", "  Board cpld register list:\n");
        index += sprintf (buf + index, "%s", "    0x00 ----- pcb version\n");
        index += sprintf (buf + index, "%s", "    0x01 ----- cpld0 firmware version\n");
        index += sprintf (buf + index, "%s", "    0x02 ----- board type\n");
        index += sprintf (buf + index, "%s", "    0x03 ----- cpld1 firmware version\n");
        index += sprintf (buf + index, "%s", "    0x07 ----- cpld2 firmware version\n");
        index += sprintf (buf + index, "%s", "    0x34 ----- psu good\n");
        index += sprintf (buf + index, "%s", "    0x35 ----- psu present\n");
        index += sprintf (buf + index, "%s", "    0x35 ----- psu present\n");
        index += sprintf (buf + index, "%s", "    0x6a ----- bit0: system id panel led set register\n");
        index += sprintf (buf + index, "%s", "    0x6b ----- bit6&bit7: psu panel led set register\n");
        index += sprintf (buf + index, "%s", "    0x6b ----- bit2&bit3: fan panel led set register\n");
        index += sprintf (buf + index, "%s", "    0x6c ----- bit0 ~ bit3: system panel led set register\n");
        index += sprintf (buf + index, "%s", "    0x71 ----- fan pwm set register\n");
        index += sprintf (buf + index, "%s", "    0x74 ----- fan dir register\n");
        index += sprintf (buf + index, "%s", "    0x75 ----- fan power set register\n");
        index += sprintf (buf + index, "%s", "    0x76 ----- fan red led set register\n");
        index += sprintf (buf + index, "%s", "    0x78 ----- fan0 ~ fan2 good status\n");
        index += sprintf (buf + index, "%s", "    0x79 ----- fan3 ~ fan4 good status\n");
        index += sprintf (buf + index, "%s", "    0x7d ----- fan green led set register\n\n");
        index += sprintf (buf + index, "%s", "  Cpu cpld register list:\n");
        index += sprintf (buf + index, "%s", "    0x00 ----- pcb version\n");
        index += sprintf (buf + index, "%s", "    0x02 ----- cpld version\n\n");
        index += sprintf (buf + index, "%s", "  Cage power on register list:\n");
        index += sprintf (buf + index, "%s", "    0x37 ---- bit5: cage power on\n\n");
        index += sprintf (buf + index, "%s", "  Sfp present register list:\n");
        index += sprintf (buf + index, "%s", "    0x89 ---- port00 ~ port07 optic present\n");
        index += sprintf (buf + index, "%s", "    0x8a ---- port08 ~ port15 optic present\n");
        index += sprintf (buf + index, "%s", "    0x8b ---- port16 ~ port23 optic present\n");
        index += sprintf (buf + index, "%s", "    0x8c ---- port24 ~ port31 optic present\n");
        index += sprintf (buf + index, "%s", "    0x8d ---- port32 ~ port39 optic present\n");
        index += sprintf (buf + index, "%s", "    0x8e ---- port40 ~ port47 optic present\n\n");
        index += sprintf (buf + index, "%s", "  Sfp tx dis register list:\n");
        index += sprintf (buf + index, "%s", "    0x91 ---- port00 ~ port07 tx dis\n");
        index += sprintf (buf + index, "%s", "    0x92 ---- port08 ~ port15 tx dis\n");
        index += sprintf (buf + index, "%s", "    0x93 ---- port16 ~ port23 tx dis\n");
        index += sprintf (buf + index, "%s", "    0x94 ---- port24 ~ port31 tx dis\n");
        index += sprintf (buf + index, "%s", "    0x95 ---- port32 ~ port39 tx dis\n");
        index += sprintf (buf + index, "%s", "    0x96 ---- port40 ~ port47 tx dis\n\n");
        index += sprintf (buf + index, "%s", "  Sfp rx los register list:\n");
        index += sprintf (buf + index, "%s", "    0xa1 ---- port00 ~ port07 rx los\n");
        index += sprintf (buf + index, "%s", "    0xa2 ---- port08 ~ port15 rx los\n");
        index += sprintf (buf + index, "%s", "    0xa3 ---- port16 ~ port23 rx los\n");
        index += sprintf (buf + index, "%s", "    0xa4 ---- port24 ~ port31 rx los\n");
        index += sprintf (buf + index, "%s", "    0xa5 ---- port32 ~ port39 rx los\n");
        index += sprintf (buf + index, "%s", "    0xa6 ---- port40 ~ port47 rx los\n\n");
        index += sprintf (buf + index, "%s", "  Sfp tx fault register list:\n");
        index += sprintf (buf + index, "%s", "    0x99 ---- port00 ~ port07 tx fault\n");
        index += sprintf (buf + index, "%s", "    0x9a ---- port08 ~ port15 tx fault\n");
        index += sprintf (buf + index, "%s", "    0x9b ---- port16 ~ port23 tx fault\n");
        index += sprintf (buf + index, "%s", "    0x9c ---- port24 ~ port31 tx fault\n");
        index += sprintf (buf + index, "%s", "    0x9d ---- port32 ~ port39 tx fault\n");
        index += sprintf (buf + index, "%s", "    0x9e ---- port40 ~ port47 tx fault\n\n");
        index += sprintf (buf + index, "%s", "  Qsfp present register list:\n");
        index += sprintf (buf + index, "%s", "    0xb1 ---- port48 ~ port55 optic present\n\n");
        index += sprintf (buf + index, "%s", "  Qsfp lpmod register list:\n");
        index += sprintf (buf + index, "%s", "    0xb9 ---- port49 ~ port56 qsfp lpmod\n\n");
        index += sprintf (buf + index, "%s", "  Qsfp reset register list:\n");
        index += sprintf (buf + index, "%s", "    0xc1 ---- port49 ~ port56 qsfp reset\n");
        index += sprintf (buf + index, "%s", "  Qsfp interrupt register list:\n");
        index += sprintf (buf + index, "%s", "    0xc9 ---- port49 ~ port56 qsfp interrupt\n\n");
        index += sprintf (buf + index, "%s", "  Read cpld register:\n");
        index += sprintf (buf + index, "%s", "    you can 'cat /sys/switch/debug/xxx_cpld' get cpld data\n\n");
        index += sprintf (buf + index, "%s", "    eg:\n\n");
        index += sprintf (buf + index, "%s", "      ** !!!! cpld operation is dangerous, which may cause reboot/halt !!!!\n");
        index += sprintf (buf + index, "%s", "      ** use 'echo offset:value' to set register value\n");
        index += sprintf (buf + index, "%s", "      ** offset & value must be hex value, example: 'echo 0x0:0x17 > board_cpld'\n\n");
        index += sprintf (buf + index, "%s", "      hw address start: 0x2200\n");
        index += sprintf (buf + index, "%s", "      0x0000: 08 14 01 d3 10 05 00 14  70 01 01 ff 55 55 55 55\n");
        index += sprintf (buf + index, "%s", "      0x0010: 55 11 05 05 05 ff ff ff  ff ff ff ff ff ff ff ff\n");
        index += sprintf (buf + index, "%s", "      0x0020: ff ff ff 10 10 01 01 ff  ff 0d 0d 0d 0d 0d 0d 0d\n");
        index += sprintf (buf + index, "%s", "      0x0030: 0d 0d 05 01 ff fc fd 2f  01 01 01 01 7e 7f 7f 7f\n");
        index += sprintf (buf + index, "%s", "      0x0040: 7f 7f 7f 7f 7e 7e 7e 7e  01 f8 2f fe 80 81 81 81\n");
        index += sprintf (buf + index, "%s", "      0x0050: 81 81 81 81 17 fe ff ff  ff ff ff ff ff ff ff ff\n");
        index += sprintf (buf + index, "%s", "      0x0060: fe fe fe fe ee 40 06 0f  f0 f1 ff bb f4 f5 f5 f5\n");
        index += sprintf (buf + index, "%s", "      0x0070: 13 c2 a1 02 1f 1f 1f 00  ff ff ff 0d e0 e0 f5 05\n");
        index += sprintf (buf + index, "%s", "      0x0080: 05 05 05 05 05 05 05 05  05 f4 ff ff ff ff ff 0d\n");
        index += sprintf (buf + index, "%s", "      0x0090: 0d 00 00 00 00 00 00 05  05 f4 ff ff ff ff ff 0d\n");
        index += sprintf (buf + index, "%s", "      0x00a0: 0d f4 ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
        index += sprintf (buf + index, "%s", "      0x00b0: ff ff ff ff ff ff ff ff  ff 00 01 01 01 01 01 01\n");
        index += sprintf (buf + index, "%s", "      0x00c0: 00 ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
        index += sprintf (buf + index, "%s", "      0x00d0: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
        index += sprintf (buf + index, "%s", "      0x00e0: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
        index += sprintf (buf + index, "%s", "      0x00f0: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n\n");
        index += sprintf (buf + index, "%s", "  write cpld register:\n");
        index += sprintf (buf + index, "%s", "    you can 'echo offset:value > xxx_cpld' set cpld register\n\n");
        index += sprintf (buf + index, "%s", "    eg:\n");
        index += sprintf (buf + index, "%s", "      echo 0x02:0xff > board_cpld\n\n");
        index += sprintf (buf + index, "%s", "  WARNING: cpld operation is dangerous, which may cause reboot/halt.\n\n");
    } else {
        index += sprintf (buf + index, "%s", "N/A");
    }
    
    return index;
}

static ssize_t bsp_default_loglevel_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "the log level is: 0x%02x\n"
            "    DEBUG_ERR        (0x01) : %s\n"
            "    DEBUG_WAR        (0x02) : %s\n"
            "    DEBUG_INFO       (0x04) : %s\n"
            "    DEBUG_DBG        (0x08) : %s\n"
            "please set Dec or Hex(0x..) data.\n",
            loglevel, 
            loglevel & DEBUG_ERR      ? "on" : "off",
            loglevel & DEBUG_WAR      ? "on" : "off",
            loglevel & DEBUG_INFO     ? "on" : "off",
            loglevel & DEBUG_DBG      ? "on" : "off");
}

static ssize_t bsp_default_loglevel_store (struct kobject *kobj, struct kobj_attribute *attr, const char* _buf, size_t count)
{
    if (_buf[1] == 'x') {
        sscanf(_buf, "%x", &loglevel);
    } else {
        sscanf(_buf, "%d", &loglevel);
    }

    DBG_ECHO(DEBUG_INFO, "set loglevel %x ...\n", loglevel);
    return count;
}

static ssize_t bsp_default_debug_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return bsp_default_debug_help(buf);
}

static ssize_t bsp_default_debug_store (struct kobject *kobj, struct kobj_attribute *attr, const char* buf, size_t count)
{
    //debug();
	return count;
}
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

SYSFS_RW_ATTR_DEF(CPU_CPLD_NAME,   bsp_cpld_sysfs_show_cpld, bsp_cpld_sysfs_set_cpld);
SYSFS_RW_ATTR_DEF(BOARD_CPLD_NAME, bsp_cpld_sysfs_show_cpld, bsp_cpld_sysfs_set_cpld);
SYSFS_RW_ATTR_DEF(BUFF_NAME,       bsp_cpld_sysfs_buffer_show, bsp_cpld_sysfs_buffer_set);
SENSOR_DEVICE_ATTR(num_cplds,    S_IRUGO,         bsp_cpld_custom_root_sysfs_read, NULL, NUM_CPLDS);
SENSOR_DEVICE_ATTR(alias,        S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, ALIAS);
SENSOR_DEVICE_ATTR(type,         S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, TYPE);
SENSOR_DEVICE_ATTR(hw_version,   S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, HW_VERSION);
SENSOR_DEVICE_ATTR(board_version,S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, BROAD_VERSION);
SENSOR_DEVICE_ATTR(hw_reboot,    S_IRUGO,         bsp_cpld_custom_root_sysfs_read, NULL, HW_REBOOT);
SENSOR_DEVICE_ATTR(reg_reset_cpu, S_IRUGO | S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, REG_RESET_CPU);
SENSOR_DEVICE_ATTR(reg_i2c_wdt_ctrl, S_IRUGO | S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, I2C_WDT_CTRL);
SENSOR_DEVICE_ATTR(reg_cpu_init_ok,  S_IRUGO | S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, CPU_INIT_OK);
SENSOR_DEVICE_ATTR(reg_i2c_wdt_feed, S_IRUGO | S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, I2C_WDT_FEED);
SENSOR_DEVICE_ATTR(reg_mac_init_ok,  S_IRUGO | S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, REG_MAC_INIT_OK);

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct kobj_attribute loglevel_att =
    __ATTR(loglevel, S_IRUGO | S_IWUSR, bsp_default_loglevel_show, bsp_default_loglevel_store);

static struct kobj_attribute debug_att =
    __ATTR(debug, S_IRUGO | S_IWUSR, bsp_default_debug_show, bsp_default_debug_store);
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *cpld_custom_attributes[] = {
    &sensor_dev_attr_alias.dev_attr.attr,
    &sensor_dev_attr_type.dev_attr.attr,
    &sensor_dev_attr_hw_version.dev_attr.attr,
    &sensor_dev_attr_board_version.dev_attr.attr,
    &sensor_dev_attr_reg_reset_cpu.dev_attr.attr,
    &sensor_dev_attr_reg_i2c_wdt_ctrl.dev_attr.attr,
    &sensor_dev_attr_reg_cpu_init_ok.dev_attr.attr,
    &sensor_dev_attr_reg_i2c_wdt_feed.dev_attr.attr,
    &sensor_dev_attr_reg_mac_init_ok.dev_attr.attr,
    NULL
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute *def_attrs[] = {
    &loglevel_att.attr,
    &debug_att.attr,
    NULL,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *cpld_custom_reg_attributes[] = {
    &sensor_dev_attr_hw_reboot.dev_attr.attr,
    NULL
};

static struct attribute *cpld_custom_root_attributes[] = {
    &sensor_dev_attr_num_cplds.dev_attr.attr,
    NULL
};

static const struct attribute_group cpld_custom_attr_group = {
    .attrs = cpld_custom_attributes,
};

static const struct attribute_group cpld_custom_reg_attr_group = {
    .attrs = cpld_custom_reg_attributes,
};

static const struct attribute_group cpld_custom_root_attr_group = {
    .attrs = cpld_custom_root_attributes,
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute_group def_attr_group = {
    .attrs = def_attrs,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct kobj_attribute slot_cpld_attr[MAX_SLOT_NUM] = {{{0}}};
static char attr_name[MAX_SLOT_NUM][20];

static int __init cpld_init(void)
{
    int ret = ERROR_SUCCESS;
	int slot_index;
    int cpld_index = 0;
    char temp_str[20] = {0};
    board_static_data * bd = bsp_get_board_data();

    memset(attr_name, 0, sizeof(attr_name));
	DBG_ECHO(DEBUG_INFO, "cpld module init started.");
    kobj_cpld_debug = kobject_create_and_add("cpld", kobj_debug);
	
	if (kobj_cpld_debug == NULL) {
		DBG_ECHO(DEBUG_ERR, "kobj_cpld_debug create falled!\n");		  
		ret = -ENOMEM;	   
		goto exit;	 
	}

	//add the attribute files and check the result, exit if failed.
	CHECK_CREATE_SYSFS_FILE(kobj_cpld_debug, BOARD_CPLD_NAME, ret);
	CHECK_CREATE_SYSFS_FILE(kobj_cpld_debug, CPU_CPLD_NAME, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_cpld_debug, BUFF_NAME, ret);

    //add subslot cpld
	for (slot_index = 0; slot_index < bd->slot_num; slot_index ++) {
	    if (bd->cpld_size_slot[slot_index] != 0) {
	        sprintf(attr_name[slot_index], SLOT_CPLD_NAME, slot_index + 1);
            slot_cpld_attr[slot_index].attr.name = attr_name[slot_index];
			slot_cpld_attr[slot_index].attr.mode = (S_IRUGO|S_IWUSR);
			slot_cpld_attr[slot_index].show = bsp_cpld_sysfs_show_cpld;
			slot_cpld_attr[slot_index].store = bsp_cpld_sysfs_set_cpld;
            ret = sysfs_create_file(kobj_cpld_debug, &(slot_cpld_attr[slot_index].attr));
            if (ERROR_SUCCESS != ret) {
                DBG_ECHO(DEBUG_ERR, "sysfs create attribute %s failed!", slot_cpld_attr[slot_index].attr.name);
	            goto exit;
		    }
		}
	}

    kobj_cpld_root = kobject_create_and_add("cpld", kobj_switch);
	if (!kobj_cpld_root) {
		DBG_ECHO(DEBUG_ERR, "kobj_switch create falled!\n");		  
		ret = -ENOMEM;	   
		goto exit;	 
	}

    /*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
    if (sysfs_create_group(kobj_cpld_root, &def_attr_group) != 0) {
        DBG_ECHO(DEBUG_INFO, "create fan default attr faild.\n");
        ret = -ENOSYS;
        goto exit;	
    }
    /*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

    ret = sysfs_create_group(kobj_cpld_root, &cpld_custom_root_attr_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create cpld root group failed");

    for (cpld_index = 0; cpld_index < bd->cpld_num; cpld_index ++) {
        sprintf(temp_str, "cpld%d", cpld_index);
        kobj_cpld_sub[cpld_index] = kobject_create_and_add(temp_str, kobj_cpld_root);
        if (!kobj_cpld_sub[cpld_index]) {
            DBG_ECHO(DEBUG_ERR, "kobj cpld sub[%d] create falled!\n", cpld_index);		  
		    ret = -ENOMEM;	   
		    goto exit;	 
        }
        
        ret = sysfs_create_group(kobj_cpld_sub[cpld_index], &cpld_custom_attr_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "create cpld%d group failed", cpld_index);
        ret = sysfs_create_group(kobj_cpld_sub[cpld_index], &cpld_custom_reg_attr_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "create cpld%d custom reg group failed", cpld_index);
    }

    if (bsp_cpld_record_reboot_reason() != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "get reboot reason failed!\n");
    }

exit:
    if (ret != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "cpld module init failed! result=%d\n", ret);
        release_kobj();
    } else {
	    INIT_PRINT("cpld module finished and success!");
	}
    
    return ret;
}

static void release_kobj(void)
{
    int cpld_index = 0;
    board_static_data *bd = bsp_get_board_data();
    
    for (cpld_index = 0; cpld_index < bd->cpld_num; cpld_index ++) {
        if (kobj_cpld_sub[cpld_index]) {
            kobject_put (kobj_cpld_sub[cpld_index]);
            kobj_cpld_sub[cpld_index] = NULL;
        }
    }
   
    if (kobj_cpld_root != NULL) {
        sysfs_remove_group (kobj_cpld_root, &def_attr_group);
        kobject_put(kobj_cpld_root);
        kobj_cpld_root = NULL;
    }
    
    if (kobj_cpld_debug != NULL) {
        kobject_put(kobj_cpld_debug);
    }

    DBG_ECHO(DEBUG_INFO, "module cpld uninstalled !\n");
}

static void __exit cpld_exit(void)
{
    release_kobj();
    INIT_PRINT("module cpld uninstalled !\n");
}

module_init(cpld_init);
module_exit(cpld_exit);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system cpld driver");
MODULE_LICENSE("Dual BSD/GPL");
