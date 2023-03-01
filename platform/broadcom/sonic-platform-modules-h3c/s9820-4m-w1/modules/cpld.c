/*公有文件引入*/
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>

/*私有文件*/
#include "pub.h"
#include "bsp_base.h"

//extern long simple_strtol(const char *cp, char **endp, unsigned int base);

#define MODULE_NAME "cpld"


/*********************************************/
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system cpld driver");
MODULE_LICENSE("Dual BSD/GPL");
/*********************************************/


#define CPLD_OPER_WARN_MSG0  "\n** !!!! cpld operation is dangerous, which may cause reboot/halt !!!!"
#define CPLD_OPER_WARN_MSG1  "\n** use 'echo offset:value' to set register value"
#define CPLD_OPER_WARN_MSG2  "\n** offset & value must be hex value, example: 'echo 0x0:0x17 > board_cpld'\n"


#define CPU_CPLD_NAME     cpu_cpld
#define BOARD_CPLD_NAME   board_cpld
#define SLOT_CPLD_NAME    "slot%d_cpld"
#define BUFF_NAME         buffer


enum CPLD_SENSOR_ATTR
{
    NUM_CPLDS,
    ALIAS,      
    TYPE,    
    HW_VERSION,
    BROAD_VERSION,
    REG_RESET_CPU,
    REG_MAC_INIT_OK,
    HW_REBOOT,
    HW_CLR_RST,
    I2C_WDT_CTRL,
    CPU_INIT_OK,
    I2C_WDT_FEED,
    MAC_INNER_TEMP
};

static void release_kobj(void);


int cpld_debug_level = DEBUG_INFO|DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(cpld_debug_level, level, fmt,##args)

static struct kobject *kobj_cpld_debug = NULL;
static struct kobject *kobj_cpld_root = NULL;
static struct kobject *kobj_cpld_sub[MAX_CPLD_NUM] = {0};

u8 buffer_char = 0;


int bsp_cpld_get_cpld_version(int cpld_index, u8 * cpld_version_hex)
{
    board_static_data *bd = bsp_get_board_data();
    return bsp_cpld_read_part(cpld_version_hex, bd->cpld_addr_cpld_ver[cpld_index], bd->cpld_mask_cpld_ver[cpld_index], bd->cpld_offs_cpld_ver[cpld_index]);
}

int bsp_cpld_get_board_version(u8 * board_version)
{
    board_static_data *bd = bsp_get_board_data();
    return bsp_cpld_read_part(board_version, bd->cpld_addr_pcb_ver, bd->cpld_mask_pcb_ver, bd->cpld_offs_pcb_ver);
}

int bsp_set_mac_init_ok(u8 bit)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
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
    //u8 cpld_mem_buf[256] = {0};
	int ret = ERROR_SUCCESS;
	int start_addr = 0;

	
	int (* cpld_read_byte_func)(u8 *value, u16 offset) = NULL;

	if (strcmp(attr->attr.name, __stringify(CPU_CPLD_NAME)) == 0)
	{
	    cpld_size = (u16) bsp_get_cpu_cpld_size();
		cpld_read_byte_func = bsp_cpu_cpld_read_byte;
		start_addr = bsp_get_board_data()->cpld_hw_addr_cpu;
	} 
	else if (strcmp(attr->attr.name, __stringify(BOARD_CPLD_NAME)) == 0)
	{
	    cpld_size = (u16) bsp_cpld_get_size();
		cpld_read_byte_func = bsp_cpld_read_byte;
		start_addr = bsp_get_board_data()->cpld_hw_addr_board;
	}
	else if (sscanf(attr->attr.name, SLOT_CPLD_NAME, &slot_index) <= 0)
	{
	    DBG_ECHO(DEBUG_ERR, "Invalid attrbute '%s'\n", attr->attr.name);
        return sprintf(buf, "Invalid attrbute '%s'\n", attr->attr.name);
	} 
	else if (slot_index > MAX_SLOT_NUM)
	{
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

    for (i = 0; i < cpld_size; i+=16)
    {
        //avoid overflow
        if (index >= (PAGE_SIZE - 200))
        {
            DBG_ECHO(DEBUG_INFO, "buf size reached %d, break to avoid overflow.", (int)index);
            break;
        }
        index += sprintf(buf + index, "0x%04x: ", i);
		
        for (j = 0; j < 16; j++)
        {
            ret = cpld_read_byte_func == NULL ? bsp_slot_cpld_read_byte(slot_index, &temp_value, i+j): cpld_read_byte_func(&temp_value, i + j);
            
            if (ret == ERROR_SUCCESS)
            {
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
    //int (* cpld_write_byte_func)(u8 value, u16 offset) = NULL;

    if (sscanf(buf, "0x%lx:0x%lx", &temp_offset, &temp_value) < 2)
    {
        DBG_ECHO(DEBUG_ERR, "Invalid format '%s'\n", buf);
		return count;
    }

	if (strcmp(attr->attr.name, __stringify(CPU_CPLD_NAME)) == 0)
	{
	    ret = bsp_cpu_cpld_write_byte((u8)temp_value, (u16)temp_offset);
	} 
	else if (strcmp(attr->attr.name, __stringify(BOARD_CPLD_NAME)) == 0)
	{
		ret = bsp_cpld_write_byte((u8)temp_value, (u16)temp_offset);
	}
	else if (sscanf(attr->attr.name, SLOT_CPLD_NAME, &slot_index) <= 0)
	{
	    DBG_ECHO(DEBUG_ERR, "Invalid attrbute '%s'\n", attr->attr.name);
        return count;
	} 
	else if (slot_index > MAX_SLOT_NUM)
	{
	    DBG_ECHO(DEBUG_ERR, "Invalid slot index %d\n", slot_index);
		return count;
	} else {
	
	    slot_index = slot_index - 1;
		ret = bsp_slot_cpld_write_byte(slot_index, (u8)temp_value, (u16)temp_offset);
	}

	if (ret != ERROR_SUCCESS)
	{
	    DBG_ECHO(DEBUG_ERR, "slot(%d) write 0x%lx to offset 0x%lx failed!" , slot_index,  temp_value, temp_offset);
	}

	return count;
}

static ssize_t bsp_cpld_sysfs_set_cpld_clr_rst(struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    //ssize_t len = 0;
    struct kobject * okobjs = (struct kobject*)kobjs;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    board_static_data *bd = bsp_get_board_data();
    int temp = 0;
    u8 get_value = 0;
    switch(attr->index)
    {
        case HW_CLR_RST:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_ERR, "HW_CLR_RST format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
            }
            else if(ERROR_SUCCESS == bsp_cpu_cpld_read_byte(&get_value, bd->cpld_addr_clear_reset_flag))
            {
                if (ERROR_SUCCESS != bsp_cpu_cpld_write_byte(get_value | ((1<< bd->cpld_offs_clear_reset_flag) & bd->cpld_mask_clear_reset_flag), bd->cpld_addr_clear_reset_flag))
                {
                    DBG_ECHO(DEBUG_ERR, "HW_CLR_RST set failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
                }
            }
            else
            {
                DBG_ECHO(DEBUG_ERR, "clr_rst failed");
            }
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_INFO, "not support write attribute %s -> %s", okobjs->name, attr->dev_attr.attr.name);
            break;
        }
    }

    return count;
}

//读cpu特定地址, 返回值放到buffer里。仅用于cpld升级
static ssize_t bsp_cpld_sysfs_buffer_show(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%c", buffer_char);
}

static ssize_t bsp_cpld_sysfs_buffer_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    //cpu_read_0xaa
    //brd_read_0xaa
    //slot_read_n_0xaa

    int temp_value = 0;
    int slot_index = 0;
    int ret = ERROR_SUCCESS;

    if (strstr(buf, "cpu_read") != NULL)
	{
        if (sscanf(buf, "cpu_read_0x%x", &temp_value) == 1)
        {
	        ret = bsp_cpu_cpld_read_byte(&buffer_char, (u16)temp_value);
        }
        else
        {
            DBG_ECHO(DEBUG_ERR, "'%s' format error! 'cpu_read_0xaa'", buf);
        }
	}
    else if (strstr(buf, "brd_read") != NULL)
    {
        if (sscanf(buf, "brd_read_0x%x", &temp_value) == 1)
        {
	        ret = bsp_cpld_read_byte(&buffer_char, (u16)temp_value);
        }
        else
        {
            DBG_ECHO(DEBUG_ERR, "'%s' format error! 'brd_read_0xaa'", buf);
        }
    }
    else if (strstr(buf, "slot_read") != NULL)
    {
        if (sscanf(buf, "slot_read_%d_0x%x", &slot_index, &temp_value) == 2)
        {
	        ret = bsp_slot_cpld_read_byte(slot_index, &buffer_char, (u16)temp_value);
        }
        else
        {
            DBG_ECHO(DEBUG_ERR, "'%s' format error! 'slot_read_n_0xaa'", buf);
        }
    }
    else
    {
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_ERR, "'%s' format error!", buf);
    }

    if (ret != ERROR_SUCCESS)
    {
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

    switch(attr->index)
    {
    case ALIAS:
        if (cpld_index < bd->cpld_num && bd->cpld_location_describe[cpld_index] != NULL)
        {
            len = sprintf(buf, "%s", bd->cpld_location_describe[cpld_index]);
        }
        else
        {
            len = sprintf(buf, "error cpld index %d or alias unknown", cpld_index);
        }

        break;
    case TYPE:
        if (cpld_index < bd->cpld_num && bd->cpld_type_describe[cpld_index] != NULL)
        {
            len = sprintf(buf, "%s", bd->cpld_type_describe[cpld_index]);
        }
        else
        {
            len = sprintf(buf, "error cpld index %d or type unknown", cpld_index);
        }

        break;
    case HW_VERSION:
        if (bsp_cpld_get_cpld_version(cpld_index, &temp) == ERROR_SUCCESS)
        {
            len = sprintf(buf, "%d", temp);
        }
        else
        {
            DBG_ECHO(DEBUG_ERR, "get cpld %d version failed", cpld_index);
            len = sprintf(buf, "failed");
        }
        break;
    case BROAD_VERSION:
        if (bsp_cpld_get_board_version(&temp) == ERROR_SUCCESS)
        {
            len = sprintf(buf, "%d", temp);
        }
        else
        {
            DBG_ECHO(DEBUG_ERR, "get pcb version failed");
            len = sprintf(buf, "failed");
        }
        break;
    case REG_RESET_CPU:
        if (bsp_cpld_get_bit(bd->cpld_addr_cpu_rst, bd->cpld_offs_cpu_rst, &temp) != ERROR_SUCCESS)
        {
            len = sprintf(buf, "get cpu reset reg failed");
        }
        else
        {
            len = sprintf(buf, "%d", (int)temp);
        }
        break;
    case I2C_WDT_CTRL:
        if (bsp_cpld_read_byte(&temp, bd->cpld_addr_i2c_wdt_ctrl) != ERROR_SUCCESS)
        {
            len = sprintf(buf, "get i2c_wdt_ctrl reg failed");
        }
        else
        {
            len = sprintf(buf, "%d", (int)temp);
        }
        break;
    case CPU_INIT_OK:
        //if (bsp_cpld_read_byte(&temp, bd->cpld_addr_cpu_init_ok) != ERROR_SUCCESS)
        if (bsp_cpld_get_bit(bd->cpld_addr_cpu_init_ok ,bd->cpld_offs_cpu_init_ok,&temp) != ERROR_SUCCESS)
        {
            len = sprintf(buf, "get cpu_init_ok reg failed");
        }
        else
        {
            len = sprintf(buf, "%d", (int)temp);
        }
        break;
    case I2C_WDT_FEED:
        if (bsp_cpld_read_byte(&temp, bd->cpld_addr_i2c_wdt_feed) != ERROR_SUCCESS)
        {
            len = sprintf(buf, "get i2c_wdt_feed reg failed");
        }
        else
        {
            len = sprintf(buf, "%d", (int)temp);
        }
        break;
    case REG_MAC_INIT_OK:
        if (bsp_cpld_get_bit(bd->cpld_addr_mac_init_ok, bd->cpld_offs_mac_init_ok, &temp) != ERROR_SUCCESS)
        {
            len = sprintf(buf, "get mac_init_ok reg failed");
        }
        else
        {
            len = sprintf(buf, "%d", (int)temp);
        }
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "no attribute index %d", attr->index);
    }
    
    return len;
}


static ssize_t bsp_cpld_custom_root_sysfs_read(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;

    board_static_data *bd = bsp_get_board_data();
    u8 temp = 0;
    u8 mac_temp_lo = 0;
    u8 mac_temp_hi = 0;
    u16 mac_temp = 0;
    u8 retry = 0;
    int ret = 0;
    bool result = 0;

    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    if ((bd->product_type == PDT_TYPE_TCS83_120F_4U)&&(attr->index==MAC_INNER_TEMP))
    {
        len = sprintf(buf, "N/A\n");
        return len;
    }
    switch(attr->index)
    {
        case NUM_CPLDS:
        {
            len = sprintf(buf, "%d", (int)bd->cpld_num);
            break;
        }
        case HW_REBOOT:
        {
            ret = bsp_cpu_cpld_read_byte(&temp, bd->cpld_addr_reset_type_cold);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld read HW REBOOT failed!");
            if((temp & bd->cpld_mask_reset_type_cold) >> bd->cpld_offs_reset_type_cold)
            {
                len = sprintf(buf, "%s", "RESET_TYPE_COLD\n");
            }
            else if((temp & bd->cpld_mask_reset_type_wdt) >> bd->cpld_offs_reset_type_wdt)
            {
                len = sprintf(buf, "%s", "RESET_TYPE_WDT\n");
            }
            else if((temp & bd->cpld_mask_reset_type_boot_sw) >> bd->cpld_offs_reset_type_boot_sw)
            {
                len = sprintf(buf, "%s", "RESET_TYPE_BOOT_SW\n");
            }
            else if((temp & bd->cpld_mask_reset_type_soft) >> bd->cpld_offs_reset_type_soft)
            {
                len = sprintf(buf, "%s", "RESET_TYPE_SOFT\n");
            }
            else if((temp & (bd->cpld_mask_reset_type_cold | bd->cpld_mask_reset_type_wdt | bd->cpld_mask_reset_type_boot_sw | bd->cpld_mask_reset_type_soft)) != 0)
            {
                if((temp & bd->cpld_mask_reset_type_cold) >> bd->cpld_offs_reset_type_cold)
                {
                    len = sprintf(buf + strlen(buf), "%s", "RESET_TYPE_COLD\n");
                }
                else if((temp & bd->cpld_mask_reset_type_wdt) >> bd->cpld_offs_reset_type_wdt)
                {
                    len = sprintf(buf + strlen(buf), "%s", "RESET_TYPE_WDT\n");
                }
                else if((temp & bd->cpld_mask_reset_type_boot_sw) >> bd->cpld_offs_reset_type_boot_sw)
                {
                    len = sprintf(buf + strlen(buf), "%s", "RESET_TYPE_BOOT_SW\n");
                }
                else if((temp & bd->cpld_mask_reset_type_soft) >> bd->cpld_offs_reset_type_soft)
                {
                    len = sprintf(buf + strlen(buf), "%s", "RESET_TYPE_SOFT\n");
                }
                len = sprintf(buf, "%s", "HW has moer than one restart records,unable to determine the reson for the HW last restart\n");
                len = strlen(buf);
            }
            else
            {
                len = sprintf(buf, "%s", "No_HW_restart\n");
            }
	        break;	    
        }
        case HW_CLR_RST:
        {
            ret = bsp_cpu_cpld_read_byte(&temp, bd->cpld_addr_clear_reset_flag);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld read HW REBOOT failed!");
            len = sprintf(buf, "%d", (temp & bd->cpld_mask_clear_reset_flag) >> bd->cpld_offs_clear_reset_flag);
            break;
        }
        case MAC_INNER_TEMP:
        {
            #define MAC_TEMP_OK_MASK 0x03
            #define MAC_TEMP_MAX_RETRIES 10
            mutex_lock(&bsp_mac_inner_temp_lock);
            ret = bsp_cpld_write_byte(0x01, 0x63);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "write mac inner temp flag failed!");
            do {
                ret = bsp_cpld_read_byte(&temp, 0x63);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "mac inner temp read phase 0 failed!");
                result = (MAC_TEMP_OK_MASK != (temp&MAC_TEMP_OK_MASK));
                if (result)
                    msleep(10);
                else
                    break;
            }while ( retry++ < MAC_TEMP_MAX_RETRIES );
            CHECK_IF_ERROR_GOTO_EXIT(result, "mac inner temp read phase 1 failed with %#x!\n", temp);
            ret = bsp_cpld_read_byte(&mac_temp_lo, 0x64);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "mac inner temp read phase 2 failed!");
            ret = bsp_cpld_read_byte(&mac_temp_hi, 0x65);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "mac inner temp read phase 3 failed!");
            mac_temp = mac_temp_hi;
            mac_temp <<= 8;
            mac_temp |= mac_temp_lo;
            len = sprintf(buf, "%d", mac_temp);
            break;
        }
	    default:
        {
	        DBG_ECHO(DEBUG_ERR, "no attribute index %d", attr->index);
            break;
        }
    }

exit:
    if (MAC_INNER_TEMP == attr->index)
    {
        bsp_cpld_write_byte(0x00, 0x63);
        if(!len)
            len = sprintf(buf, "get mac inner temp error");
        mutex_unlock(&bsp_mac_inner_temp_lock);
    }
    return len;

}


static ssize_t bsp_cpld_custom_sysfs_write(struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    board_static_data *bd = bsp_get_board_data();

    switch(attr->index)
    {
    case REG_RESET_CPU:
        if (sscanf(buf, "%d", &temp) == 1)
        {
            temp = (temp != 0) ? 1 : 0;
            if (bsp_cpld_set_bit(bd->cpld_addr_cpu_rst, bd->cpld_offs_cpu_rst, (u8)temp) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "cpu reset set bit %d failed", temp);
            }
        }
        break;
    case I2C_WDT_CTRL:
        if (sscanf(buf, "%d", &temp) == 1)
        {
            if (bsp_cpld_write_byte(temp, bd->cpld_addr_i2c_wdt_ctrl) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "set cpu_init_ok %d failed", temp);
            }
        }
        else
        {
            count = -EINVAL;
        }
        break;
    case CPU_INIT_OK:
        if (sscanf(buf, "%d", &temp) == 1)
        {
                if (bsp_cpld_set_bit(bd->cpld_addr_cpu_init_ok, bd->cpld_offs_cpu_init_ok, (u8)temp) != ERROR_SUCCESS)
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
        if (sscanf(buf, "%d", &temp) == 1)
        {
            if (bsp_cpld_write_byte(temp, bd->cpld_addr_i2c_wdt_feed) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "set i2c_wdt_feed %d failed", temp);
            }
        }
        else
        {
            count = -EINVAL;
        }
        break;
    case REG_MAC_INIT_OK:
        if (sscanf(buf, "%d", &temp) == 1)
        {
            temp = (temp != 0) ? 1 : 0;
            if (bsp_set_mac_init_ok((u8)temp) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "set mac_init_ok %d failed", temp);
            }
        }
        else
        {
            count = -EINVAL;
        }
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "no support attribute index %d", attr->index);
    }

    return count;

}



SYSFS_RW_ATTR_DEF(CPU_CPLD_NAME,   bsp_cpld_sysfs_show_cpld, bsp_cpld_sysfs_set_cpld);
SYSFS_RW_ATTR_DEF(BOARD_CPLD_NAME, bsp_cpld_sysfs_show_cpld, bsp_cpld_sysfs_set_cpld);
SYSFS_RW_ATTR_DEF(BUFF_NAME,       bsp_cpld_sysfs_buffer_show, bsp_cpld_sysfs_buffer_set);



SENSOR_DEVICE_ATTR(num_cplds,    S_IRUGO,         bsp_cpld_custom_root_sysfs_read, NULL, NUM_CPLDS);
SENSOR_DEVICE_ATTR(alias,        S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, ALIAS);
SENSOR_DEVICE_ATTR(type,         S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, TYPE);
SENSOR_DEVICE_ATTR(hw_version,   S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, HW_VERSION);
SENSOR_DEVICE_ATTR(board_version,S_IRUGO,         bsp_cpld_custom_sysfs_read, NULL, BROAD_VERSION);
SENSOR_DEVICE_ATTR(hw_reboot,    S_IRUGO,         bsp_cpld_custom_root_sysfs_read, NULL, HW_REBOOT);
SENSOR_DEVICE_ATTR(reg_reset_cpu, S_IRUGO|S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, REG_RESET_CPU);
SENSOR_DEVICE_ATTR(reg_i2c_wdt_ctrl, S_IRUGO|S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, I2C_WDT_CTRL);
SENSOR_DEVICE_ATTR(reg_cpu_init_ok,  S_IRUGO|S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, CPU_INIT_OK);
SENSOR_DEVICE_ATTR(reg_i2c_wdt_feed, S_IRUGO|S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, I2C_WDT_FEED);
SENSOR_DEVICE_ATTR(reg_mac_init_ok,  S_IRUGO|S_IWUSR, bsp_cpld_custom_sysfs_read, bsp_cpld_custom_sysfs_write, REG_MAC_INIT_OK);
SENSOR_DEVICE_ATTR(hw_clr_rst, S_IRUGO|S_IWUSR, bsp_cpld_custom_root_sysfs_read, bsp_cpld_sysfs_set_cpld_clr_rst, HW_CLR_RST);
SENSOR_DEVICE_ATTR(mac_inner_temp, S_IRUGO, bsp_cpld_custom_root_sysfs_read, NULL, MAC_INNER_TEMP);

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

static struct attribute *cpld_custom_reg_attributes[] = {

    NULL
};

static struct attribute *cpld_custom_root_attributes[] = {
    &sensor_dev_attr_num_cplds.dev_attr.attr,
    &sensor_dev_attr_hw_reboot.dev_attr.attr,
    &sensor_dev_attr_hw_clr_rst.dev_attr.attr,
    &sensor_dev_attr_mac_inner_temp.dev_attr.attr,
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


static struct kobj_attribute slot_cpld_attr[MAX_SLOT_NUM] = {{{0}}};
static char attr_name[MAX_SLOT_NUM][20];


//设置初始化入口函数
static int __init cpld_init(void)
{
    int ret = ERROR_SUCCESS;
	int slot_index;
    int cpld_index = 0;
    char temp_str[20] = {0};
    board_static_data * bd = bsp_get_board_data();

    memset(attr_name, 0, sizeof(attr_name));
	DBG_ECHO(DEBUG_INFO, "cpld module init started.");
	//create node for cpld debug
    kobj_cpld_debug = kobject_create_and_add("cpld", kobj_debug);
	
	if (kobj_cpld_debug == NULL)
	{
		DBG_ECHO(DEBUG_ERR, "kobj_cpld_debug create falled!\n");		  
		ret = -ENOMEM;	   
		goto exit;	 
	}

	//add the attribute files and check the result, exit if failed.
	CHECK_CREATE_SYSFS_FILE(kobj_cpld_debug, BOARD_CPLD_NAME, ret);
	CHECK_CREATE_SYSFS_FILE(kobj_cpld_debug, CPU_CPLD_NAME, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_cpld_debug, BUFF_NAME, ret);

    //add subslot cpld
	for (slot_index = 0; slot_index < bd->slot_num; slot_index ++)
	{
	    if (bd->cpld_size_slot[slot_index] != 0)
	    {
	        sprintf(attr_name[slot_index], SLOT_CPLD_NAME, slot_index + 1);
            slot_cpld_attr[slot_index].attr.name = attr_name[slot_index];
			slot_cpld_attr[slot_index].attr.mode = (S_IRUGO|S_IWUSR);
			slot_cpld_attr[slot_index].show = bsp_cpld_sysfs_show_cpld;
			slot_cpld_attr[slot_index].store = bsp_cpld_sysfs_set_cpld;
            ret = sysfs_create_file(kobj_cpld_debug, &(slot_cpld_attr[slot_index].attr));
            if (ERROR_SUCCESS != ret)
            {
                DBG_ECHO(DEBUG_ERR, "sysfs create attribute %s failed!", slot_cpld_attr[slot_index].attr.name);
	            goto exit;
		    }
		}
	}


    kobj_cpld_root = kobject_create_and_add("cpld", kobj_switch);
	if (kobj_cpld_root == NULL)
	{
		DBG_ECHO(DEBUG_ERR, "kobj_switch create falled!\n");		  
		ret = -ENOMEM;	   
		goto exit;	 
	}

    ret = sysfs_create_group(kobj_cpld_root, &cpld_custom_root_attr_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create cpld root group failed");

    for (cpld_index = 0; cpld_index < bd->cpld_num; cpld_index ++)
    {
        sprintf(temp_str, "cpld%d", cpld_index);
        kobj_cpld_sub[cpld_index] = kobject_create_and_add(temp_str, kobj_cpld_root);
        if (kobj_cpld_sub[cpld_index] == NULL)
        {
            DBG_ECHO(DEBUG_ERR, "kobj cpld sub[%d] create falled!\n", cpld_index);		  
		    ret = -ENOMEM;	   
		    goto exit;	 
        }
        ret = sysfs_create_group(kobj_cpld_sub[cpld_index], &cpld_custom_attr_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "create cpld%d group failed", cpld_index);
        ret = sysfs_create_group(kobj_cpld_sub[cpld_index], &cpld_custom_reg_attr_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "create cpld%d custom reg group failed", cpld_index);
    }

exit:
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "cpld module init failed! result=%d\n", ret);
        release_kobj();
    }
	else
	{
	    INIT_PRINT("cpld module finished and success!");
	}
    
    return ret;
}


static void release_kobj(void)
{
    int cpld_index = 0;
    board_static_data *bd = bsp_get_board_data();
    
    for (cpld_index = 0; cpld_index < bd->cpld_num; cpld_index ++)
    {
        if (kobj_cpld_sub[cpld_index] != NULL)
            kobject_put(kobj_cpld_sub[cpld_index]);
    }
    
    if (kobj_cpld_root != NULL)
        kobject_put(kobj_cpld_root);
    if (kobj_cpld_debug != NULL)
        kobject_put(kobj_cpld_debug);

    DBG_ECHO(DEBUG_INFO, "module cpld uninstalled !\n");
    return;
}


//设置出口函数
static void __exit cpld_exit(void)
{
    release_kobj();
    
    INIT_PRINT("module cpld uninstalled !\n");
    return;
}

module_init(cpld_init);
module_exit(cpld_exit);


