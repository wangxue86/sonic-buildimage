

/*公有文件引入*/
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "linux/kthread.h"
#include <linux/delay.h>




#include "pub.h"
#include "bsp_base.h"
//#include "static_ktype.h"
#include "i2c_dev_reg.h"


/*********************************************/
MODULE_AUTHOR("Wan Huan <wan.huan@h3c.com>");
MODULE_DESCRIPTION("h3c system led driver");
MODULE_LICENSE("Dual BSD/GPL");
/*********************************************/


#define MODULE_NAME "sysled"


int sysled_debug_level = DEBUG_INFO|DEBUG_ERR;

#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(sysled_debug_level, level, fmt,##args)

static struct kobject *kobj_sysled = NULL;
static struct kobject *kobj_led_debug = NULL;

extern int bsp_fan_get_status(int fan_index);
extern int bsp_psu_get_status(int psu_index);


//extern int bsp_cpld_write_part(IN u8 value, IN u16 offset, IN u8 mask, IN u8 shift_bits);


struct task_struct *sysled_monitor_task = NULL;
int sysled_mon_interval = 1000;
int sysled_monitor_task_sleep = 1;

enum SYSLED_ATTR
{
    SYSLED_LOC_FRONT,
    SYSLED_LOC_REAR,
    BMCLED_LOC_FRONT,
    BMCLED_LOC_REAR,
    FANLED_LOC_FRONT,
    FANLED_LOC_REAR,
    PSULED_LOC_FRONT,
    PSULED_LOC_REAR,
    IDLED_LOC_FRONT,
    IDLED_LOC_REAR,
    DIS_LED_MON
};

struct led_color_status
{
  int sys_led_front_color;
  int sys_led_rear_color;
  int bmc_led_front_color;
  int bmc_led_rear_color;
  int fan_led_front_color;
  int fan_led_rear_color;
  int psu_led_front_color;
  int psu_led_rear_color;
  int id_led_front_color;
  int id_led_rear_color;
};

struct led_color_status led_color_status_info = {0};

static int bsp_sysled_set_led_color(int led_location, int color_code)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();

    int led_green = 0, led_red = 0;
    u8 cpld_addr_green = 0, cpld_offs_green = 0;   
    u8 cpld_addr_red = 0, cpld_offs_red = 0;
    u16 cpld_value = 0;
    u16 cpld_addr_sysled = 0;

    switch(color_code)
    {
    case LED_COLOR_GREEN:
        {
            led_green = CODE_LED_ON;
            led_red = CODE_LED_OFF;
            break;
        }
        case LED_COLOR_RED:
        {
            led_green = CODE_LED_OFF;
            led_red = CODE_LED_ON;
            break;
        }
        case LED_COLOR_YELLOW:
        {
            led_green = CODE_LED_ON;
            led_red = CODE_LED_ON;
            break;
        }
        case LED_COLOR_DARK:
        {
            led_green = CODE_LED_OFF;
            led_red = CODE_LED_OFF;
            break;
        }
        default:
        {
            if ((led_location == IDLED_LOC_FRONT) || (led_location == IDLED_LOC_REAR))
                break;

            CHECK_IF_ERROR_GOTO_EXIT(ret = ERROR_FAILED, "unknown color_code=%d", color_code);
            goto exit;
            break;
        }
    }

    switch(led_location)
    {
        case SYSLED_LOC_FRONT:
        case SYSLED_LOC_REAR:
        {
            cpld_addr_sysled = bd->cpld_addr_pannel_sys_led_ctrl;

            switch (color_code)
            {
                case LED_COLOR_GREEN:
                {
                    cpld_value = bd->cpld_value_sys_led_code_green;
                    break;
                }
                case LED_COLOR_RED:
                {
                    cpld_value = bd->cpld_value_sys_led_code_red;
                    break;
                }
                case LED_COLOR_YELLOW:
                {
                    cpld_value = bd->cpld_value_sys_led_code_yellow;
                    break;
                }
                case LED_COLOR_DARK:
                {
                    cpld_value = bd->cpld_value_sys_led_code_dark;
                    break;
                }
            }

            ret = bsp_cpld_write_byte(cpld_value, cpld_addr_sysled);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");

            break;
        }
        case BMCLED_LOC_FRONT:
        case BMCLED_LOC_REAR:
        {
            cpld_addr_green = bd->cpld_addr_pannel_bmc_led_green;
            cpld_offs_green = bd->cpld_offs_pannel_bmc_led_green;
            cpld_addr_red = bd->cpld_addr_pannel_bmc_led_red;
            cpld_offs_red = bd->cpld_offs_pannel_bmc_led_red;
            ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
            ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
            break;
        }
        case FANLED_LOC_FRONT:
        case FANLED_LOC_REAR:
        {
            cpld_addr_green = bd->cpld_addr_pannel_fan_led_green;
            cpld_offs_green = bd->cpld_offs_pannel_fan_led_green;
            cpld_addr_red = bd->cpld_addr_pannel_fan_led_red;
            cpld_offs_red = bd->cpld_offs_pannel_fan_led_red;
            ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
            ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
            break;
        }
        case PSULED_LOC_FRONT:
        case PSULED_LOC_REAR:
        {
            cpld_addr_green = bd->cpld_addr_pannel_psu_led_green;
            cpld_offs_green = bd->cpld_offs_pannel_psu_led_green;
            cpld_addr_red = bd->cpld_addr_pannel_psu_led_red;
            cpld_offs_red = bd->cpld_offs_pannel_psu_led_red;
            ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
            ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
            break;
        }
        case IDLED_LOC_FRONT:
        case IDLED_LOC_REAR:
        {
            led_red =   (color_code != LED_COLOR_DARK) ? CODE_LED_ON : CODE_LED_OFF;
            led_green = (color_code != LED_COLOR_DARK) ? CODE_LED_ON : CODE_LED_OFF;
            cpld_addr_green = bd->cpld_addr_pannel_id_led_blue;
            cpld_offs_green = bd->cpld_offs_pannel_id_led_blue;
            cpld_addr_red = bd->cpld_addr_pannel_id_led_blue;
            cpld_offs_red = bd->cpld_offs_pannel_id_led_blue;
            ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
            ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
            break;
        }
        default:
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret = ERROR_FAILED, "unknown led_location=%d", led_location);
            goto exit;
            break;
        }
    }

    
exit:
    
    return ret;
}

static ssize_t bsp_sysled_sysfs_get_led_status(struct device *kobj, struct device_attribute *da, char *buf)
{

    ssize_t index = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);

    switch(attr->index)
    {
        case SYSLED_LOC_FRONT:
        {
            index = sprintf(buf, "%d", led_color_status_info.sys_led_front_color);
            break;
        }
        case SYSLED_LOC_REAR:
        {
            index = sprintf(buf, "%d", led_color_status_info.sys_led_rear_color);
            break;
        }
        case BMCLED_LOC_FRONT:
        {
            index = sprintf(buf, "%d", led_color_status_info.bmc_led_front_color);
            break;
        }
        case BMCLED_LOC_REAR:
        {
            index = sprintf(buf, "%d", led_color_status_info.bmc_led_rear_color);
            break;
        }
        case FANLED_LOC_FRONT:
        {
            index = sprintf(buf, "%d", led_color_status_info.fan_led_front_color);
            break;
        }
        case FANLED_LOC_REAR:
        {
            index = sprintf(buf, "%d", led_color_status_info.fan_led_rear_color);
            break;
        }
        case PSULED_LOC_FRONT:
        {
            index = sprintf(buf, "%d", led_color_status_info.psu_led_front_color);
            break;
        }
        case PSULED_LOC_REAR:
        {
            index = sprintf(buf, "%d", led_color_status_info.psu_led_rear_color);
            break;
        }
        case IDLED_LOC_FRONT:
        {
            index = sprintf(buf, "%d", led_color_status_info.id_led_front_color);
            break;
        }
        case IDLED_LOC_REAR:
        {
            index = sprintf(buf, "%d", led_color_status_info.id_led_rear_color);
            break;
        }
        default:
        {
            index = sprintf(buf, "Not support\n");
            break;
        }
    }
    return index;
}

static ssize_t bsp_sysled_sysfs_set_led_status(struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    switch(attr->index)
    {
    case IDLED_LOC_FRONT:
    case IDLED_LOC_REAR:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
            }
            else
            {
                bsp_sysled_set_led_color(attr->index, temp);
                led_color_status_info.id_led_front_color = temp;
                led_color_status_info.id_led_rear_color = temp;
            }
            break;
        }
        case SYSLED_LOC_FRONT:
        case SYSLED_LOC_REAR:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
            }
            else
            {
                bsp_sysled_set_led_color(attr->index, temp);
                led_color_status_info.sys_led_front_color = temp;
                led_color_status_info.sys_led_rear_color = temp;
            }
            break;
        }
        case BMCLED_LOC_FRONT:
        case BMCLED_LOC_REAR:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
            }
            else
            {
                bsp_sysled_set_led_color(attr->index, temp);
                led_color_status_info.bmc_led_front_color = temp;
                led_color_status_info.bmc_led_front_color = temp;
            }
            break;
        }
        case FANLED_LOC_FRONT:
        case FANLED_LOC_REAR:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
            }
            else
            {
                bsp_sysled_set_led_color(attr->index, temp);
                led_color_status_info.fan_led_front_color = temp;
                led_color_status_info.fan_led_rear_color = temp;
            }
            break;
        }
        case PSULED_LOC_FRONT:
        case PSULED_LOC_REAR:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
            }
            else
            {
                bsp_sysled_set_led_color(attr->index, temp);
                led_color_status_info.psu_led_front_color = temp;
                led_color_status_info.psu_led_rear_color = temp;
            }
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_INFO, "Not support attribute %d ", attr->index);
            break;
        }
    }
    return count;
}


static ssize_t bsp_sysled_sysfs_set_led_debug(struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    switch(attr->index)
    {
        case DIS_LED_MON:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
            }
            else
            {
                sysled_monitor_task_sleep = (temp != 0);
            }
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "Not support attribute index %d", attr->index);
            break;
        }
    }
    return count;
}


static ssize_t bsp_sysled_sysfs_get_led_debug(struct device *kobj, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;

    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    switch(attr->index)
    {
        case DIS_LED_MON:
        {
            len = sprintf(buf, "%d", sysled_monitor_task_sleep);
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "Not support attribute index %d", attr->index);
            len = sprintf(buf, "Not support.");
            break;
        }
    }

    return len;
}



SENSOR_DEVICE_ATTR(sys_led_status_front, S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, SYSLED_LOC_FRONT);
SENSOR_DEVICE_ATTR(sys_led_status_rear,  S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, SYSLED_LOC_REAR);
SENSOR_DEVICE_ATTR(bmc_led_status_front, S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, BMCLED_LOC_FRONT);
SENSOR_DEVICE_ATTR(bmc_led_status_rear,  S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, BMCLED_LOC_REAR);
SENSOR_DEVICE_ATTR(fan_led_status_front, S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, FANLED_LOC_FRONT);
SENSOR_DEVICE_ATTR(fan_led_status_rear,  S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, FANLED_LOC_REAR);
SENSOR_DEVICE_ATTR(psu_led_status_front, S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, PSULED_LOC_FRONT);
SENSOR_DEVICE_ATTR(psu_led_status_rear,  S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, PSULED_LOC_REAR);
SENSOR_DEVICE_ATTR(id_led_status_front,  S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, IDLED_LOC_FRONT);
SENSOR_DEVICE_ATTR(id_led_status_rear,   S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status, IDLED_LOC_REAR);


static SENSOR_DEVICE_ATTR(disable_led_mon , S_IRUGO|S_IWUSR, bsp_sysled_sysfs_get_led_debug, bsp_sysled_sysfs_set_led_debug, DIS_LED_MON);

static struct attribute *sysled_attributes[] =
{
    &sensor_dev_attr_sys_led_status_front.dev_attr.attr,
    &sensor_dev_attr_sys_led_status_rear.dev_attr.attr,
    &sensor_dev_attr_bmc_led_status_front.dev_attr.attr,
    &sensor_dev_attr_bmc_led_status_rear.dev_attr.attr,
    &sensor_dev_attr_fan_led_status_front.dev_attr.attr,
    &sensor_dev_attr_fan_led_status_rear.dev_attr.attr,
    &sensor_dev_attr_psu_led_status_front.dev_attr.attr,
    &sensor_dev_attr_psu_led_status_rear.dev_attr.attr,
    &sensor_dev_attr_id_led_status_front.dev_attr.attr,
    &sensor_dev_attr_id_led_status_rear.dev_attr.attr,
    NULL
};

static const struct attribute_group sysled_group =
{
    .attrs = sysled_attributes,
};

static struct attribute *sysled_debug_attributes[] =
{
    &sensor_dev_attr_disable_led_mon.dev_attr.attr,
    NULL
};

static const struct attribute_group sysled_debug_group =
{
    .attrs = sysled_debug_attributes,
};



int h3c_sysled_kernel_monitor_thread(void *arg)
{
    int psu_number, psu_index;
    int fan_number, fan_index;
    int fan_led_status;
    int psu_led_status;
    int sys_led_status;

    int fan_status;
    int psu_status;
    
    board_static_data *bd = bsp_get_board_data();
    fan_number = bd->fan_num;
    psu_number = bd->psu_num;


    led_color_status_info.sys_led_front_color = LED_COLOR_DARK;
    led_color_status_info.sys_led_rear_color  = LED_COLOR_DARK;
    led_color_status_info.psu_led_front_color = LED_COLOR_DARK;
    led_color_status_info.fan_led_front_color = LED_COLOR_DARK;
    led_color_status_info.bmc_led_front_color = LED_COLOR_DARK;
        
    while(1)
    {   
        
        msleep(sysled_mon_interval);
        if (kthread_should_stop())
        {
            break;
        }
        if (sysled_monitor_task_sleep == 1)
        {
            continue;
        }
        fan_led_status = LED_COLOR_GREEN;
        psu_led_status = LED_COLOR_GREEN;
        sys_led_status = LED_COLOR_GREEN;

        for (fan_index=0; fan_index < fan_number; fan_index++)
        {
            fan_status = bsp_fan_get_status(fan_index);
            if (fan_status == FAN_STATUS_FAULT)
            {
                fan_led_status = LED_COLOR_RED;
                break;
            }
            if (fan_status == FAN_STATUS_ABSENT)
            {
                fan_led_status = LED_COLOR_YELLOW;
            }
        }

        for (psu_index=0; psu_index < psu_number; psu_index++)
        {
            psu_status = bsp_psu_get_status(psu_index);
            if (psu_status == PSU_STATUS_FAULT)
            {
                psu_led_status = LED_COLOR_RED;
                break;
            }
            if (psu_status == PSU_STATUS_ABSENT)
            {
                psu_led_status = LED_COLOR_YELLOW;
            }
        }

        
 
        if (led_color_status_info.fan_led_front_color != fan_led_status)
        {
            DBG_ECHO(DEBUG_DBG, "set fan status %d", fan_led_status);
            led_color_status_info.fan_led_front_color = fan_led_status;
            led_color_status_info.fan_led_rear_color = fan_led_status;
            
            bsp_sysled_set_led_color(FANLED_LOC_FRONT, fan_led_status);
            bsp_sysled_set_led_color(FANLED_LOC_REAR,  fan_led_status);
        }
        
        if (led_color_status_info.psu_led_front_color != psu_led_status)
        {
            DBG_ECHO(DEBUG_DBG, "set psu status %d", psu_led_status);
            led_color_status_info.psu_led_front_color = psu_led_status;
            led_color_status_info.psu_led_rear_color = psu_led_status;

            
            bsp_sysled_set_led_color(PSULED_LOC_FRONT, psu_led_status);
            bsp_sysled_set_led_color(PSULED_LOC_REAR,  psu_led_status);
        }
        
        if (led_color_status_info.sys_led_front_color != sys_led_status)
        {
            DBG_ECHO(DEBUG_DBG, "set sys status %d", sys_led_status);
            led_color_status_info.sys_led_front_color = sys_led_status;
            led_color_status_info.sys_led_rear_color = sys_led_status;
            
            bsp_sysled_set_led_color(SYSLED_LOC_FRONT, sys_led_status);
            bsp_sysled_set_led_color(SYSLED_LOC_REAR,  sys_led_status);
        }
   
    }
    return ERROR_SUCCESS;

}


static int __init sysled_init(void)
{

    int ret = ERROR_SUCCESS;

    memset(&led_color_status_info, 0 ,sizeof(led_color_status_info));
    //create node for sysled
    kobj_sysled= kobject_create_and_add("sysled", kobj_switch);

    if (kobj_sysled == NULL)
    {
        DBG_ECHO(DEBUG_ERR, "kobj_switch create falled!\n");          
        ret = -ENOMEM;       
        goto exit;     
    }

    DBG_ECHO(DEBUG_INFO, "sysled monitor task creating...");
    sysled_monitor_task = kthread_run(h3c_sysled_kernel_monitor_thread, NULL, "h3c_sysled_mon");
    CHECK_IF_NULL_GOTO_EXIT(ret, sysled_monitor_task, "sysled monitor task failed creating!");
    
    ret = sysfs_create_group(kobj_sysled, &sysled_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create group failed");

    //create led debug node
    kobj_led_debug = kobject_create_and_add("led", kobj_debug);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_led_debug, "fan debug kobject created failed!");
    
    ret = sysfs_create_group(kobj_led_debug, &sysled_debug_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "fan debug attribute group created failed!");
    
exit:
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "module init failed! result=%d\n", ret);
        if (kobj_sysled != NULL)
            kobject_put(kobj_sysled);
    }
    else
    {
        INIT_PRINT("module finished and success!");
    }

    
    return ret;
}


static void __exit sysled_exit(void)
{
    if (sysled_monitor_task != NULL)
    {
        kthread_stop(sysled_monitor_task);
    }

    if (kobj_sysled != NULL)
    {
        sysfs_remove_group(kobj_sysled, &sysled_group);
        kobject_put(kobj_sysled);
    }

    if (kobj_led_debug != NULL)
    {
        sysfs_remove_group(kobj_led_debug, &sysled_debug_group);
        kobject_put(kobj_led_debug);
    }
    INIT_PRINT("module sysled uninstalled !\n");
    return;
}

/***************************************************************************************************/


module_init(sysled_init);
module_exit(sysled_exit);
