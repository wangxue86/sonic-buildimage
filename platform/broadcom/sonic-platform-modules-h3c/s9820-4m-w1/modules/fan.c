/*公有文件引入*/
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "linux/kthread.h"
#include <linux/delay.h>
/*私有文件*/
#include "pub.h"
#include "bsp_base.h"
#include "static_ktype.h"
#define MODULE_NAME "fan"
void release_all_fan_kobj(void);

/*********************************************/
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system eeprom driver");
MODULE_LICENSE("Dual BSD/GPL");
/*********************************************/

#define FAN_EEPROM_STRING_MAX_LENGTH 128
enum fan_hwmon_sysfs_attributes
{
    SENSOR_NAME,
    FAN1_MIN,
    FAN1_MAX,
    FAN1_INPUT,
    FAN1_PULSES,
    FAN1_SPEED,
    FAN1_LABEL,
    FAN1_ENABLE,
    FAN1_ALARM,
    FAN2_MIN,
    FAN2_MAX,
    FAN2_INPUT,
    FAN2_PULSES,
    FAN2_SPEED,
    FAN2_LABEL,
    FAN2_ENABLE,
    FAN2_ALARM,
    FAN_HWMON_ATTR_BUTT,
};

enum fan_sensor_sysfs_attributes
{
    DIS_FAN_MON = FAN_HWMON_ATTR_BUTT + 1,
    DIS_ADJUST_SPEED,
    NUM_FANS,
    VENDOR_NAME,
    PRODUCT_NAME,
    SN,
    PN,
    HW_VERSION,
    NUM_MOTORS,
    MOTOR_SPEED,
    MOTOR_TOLERANCE,
    MOTOR_TARGET,
    MOTOR_RATIO,
    MOTOR_DIRECTION,
    MOTOR_STATUS,
    STATUS,
    LED_STATUS,
    FAN_SPEED_PWM,
};

enum fan_led_status
{
    FAN_LED_OFF = 0,
    FAN_LED_GREEN,
    FAN_LED_RED,
};

typedef struct
{
    int motor_index;
    int speed;
    int fan_index;
    int motor_status;
    struct kobject kobj_motor;
} fan_motor_st;

typedef struct
{
    int status;
    int fan_index;
    int pwm;
    struct device *parent_hwmon;
    struct kobject kobj_fan;
    fan_motor_st fan_motor[MAX_FAN_MOTER_NUM];
} fan_info_st;

int fan_debug_level = DEBUG_INFO | DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(fan_debug_level, level, fmt,##args)

static fan_info_st fan_info[MAX_FAN_NUM];

static struct kobject *kobj_fan_root = NULL;
static struct kobject *kobj_fan_debug = NULL;    //风扇debug节点

struct task_struct *fan_monitor_task[MAX_FAN_NUM] = {NULL};
int fan_mon_interval = 100;                   //任务间隔ms
int fan_monitor_task_sleep = 0;       //任务开关
int fan_mon_not_adjust_speed = 1;     //是否调速
char *fan_status_string[] = {"Absent", "OK", "NOT OK", "Unknown"};
extern int bsp_sensor_get_max6696_temp(int max6696_index, int spot_index, s8 *value);

int bsp_fan_get_status(int fan_index)
{
    return fan_info[fan_index].status;
}
EXPORT_SYMBOL(bsp_fan_get_status);

char *bsp_fan_get_status_string(int status)
{
    if ((status >= FAN_STATUS_ABSENT) && (status <= FAN_STATUS_FAULT))
    {
        return fan_status_string[status];
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "fan unknown status %d", status);
        return fan_status_string[FAN_STATUS_UNKNOWN];
    }
}

int bsp_fan_get_status_color(int status)
{
    int color;
    switch (status)
    {
        case FAN_STATUS_NORMAL:
        {
            color = LED_COLOR_GREEN;
            break;
        }
        case FAN_STATUS_FAULT:
        {
            color = LED_COLOR_RED;
            break;
        }
        default:
        {
            color = LED_COLOR_DARK;
            break;
        }
    }
    return color;
}

int bsp_fan_get_fan_number(int *fan_number)
{
    *fan_number = bsp_get_board_data()->fan_num;
    return ERROR_SUCCESS;
}

int bsp_fan_get_info_from_eeprom(int fan_index, int info_type, OUT u8 *info_data)
{
    u16 start_offset = 0;
    u16 read_length = 0;
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    if ((fan_index < 0) || (fan_index > bd->fan_num))
    {
        DBG_ECHO(DEBUG_ERR, "fan index %d is invalied!", fan_index);
        ret = ERROR_FAILED;
        goto exit;
    }

    if (bsp_fan_get_status(fan_index) == FAN_STATUS_ABSENT)
    {
        *info_data = '\0';
        ret = ERROR_SUCCESS;
        goto exit;
    }

    switch (info_type)
    {
        case VENDOR_NAME:
        {
            start_offset = REG_ADDR_FAN_VENDOR_NAME;
            read_length = FAN_VENDOR_NAME_BYTE_COUNT;
            break;
        }
        case PRODUCT_NAME:
        {
            start_offset = REG_ADDR_FAN_PDT_NAME;
            read_length = FAN_PDT_NAME_BYTE_COUNT;
            break;
        }
        case SN:
        {
            start_offset = REG_ADDR_FAN_SN;
            read_length = FAN_SN_BYTE_COUNT;
            break;
        }
        case HW_VERSION:
        {
            start_offset = REG_ADDR_FAN_HW_VER;
            read_length = FAN_HW_VER_BYTE_COUNT;
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "not support info type %d", info_type);
            ret = ERROR_FAILED;
            goto exit;
            break;
        }
    }
    if (lock_i2c_path(I2C_DEV_FAN + fan_index) == ERROR_SUCCESS)
    {
        ret = bsp_i2c_24LC128_eeprom_read_bytes(bd->i2c_addr_fan[fan_index], start_offset, read_length, info_data);
    }
    unlock_i2c_path();

exit:
    return ret;
}

#if 0

ssize_t bsp_fan_sysfs_show(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    return 0;
}

ssize_t  bsp_fan_sysfs_store(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    return count;
}

#endif



//找到dev对应的fan index
int bsp_sysfs_fan_get_index_from_dev(struct device *dev)
{
    int i;
    int fan_num = bsp_get_board_data()->fan_num;
    for (i = 0; i < fan_num; i++)
    {
        if (fan_info[i].parent_hwmon == dev)
            return i;
    }
    DBG_ECHO(DEBUG_ERR, "Not found matched fan hwmon, dev=%p", dev);
    return -1;
}


static ssize_t bsp_sysfs_fan_custom_set_attr(struct device *kobject, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    fan_info_st *fan_ptr = container_of((struct kobject *)kobject, fan_info_st, kobj_fan);
    int fan_index = fan_ptr->fan_index;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    switch (attr->index)
    {
        case DIS_FAN_MON:
        case DIS_ADJUST_SPEED:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
            }
            else
            {
                attr->index == DIS_FAN_MON ? (fan_monitor_task_sleep = (temp != 0)) : (fan_mon_not_adjust_speed = (temp != 0));
            }
            break;
        }
        case FAN_SPEED_PWM:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
            }
            else if (bsp_cpld_set_fan_pwm_reg((u8)temp) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "set fan pwm cpld failed");
            }
            break;
        }
        case LED_STATUS:
            //设置风扇led
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
            }
            else
            {
                if (temp == FAN_LED_OFF)
                {
                    bsp_cpld_set_fan_led_red(CODE_LED_OFF, fan_index);
                    bsp_cpld_set_fan_led_green(CODE_LED_OFF, fan_index);
                }

                else if (temp == FAN_LED_GREEN)
                {
                    bsp_cpld_set_fan_led_green(CODE_LED_ON, fan_index);
                    bsp_cpld_set_fan_led_red(CODE_LED_OFF, fan_index);
                }
                else if (temp == FAN_LED_RED)
                {
                    bsp_cpld_set_fan_led_green(CODE_LED_OFF, fan_index);
                    bsp_cpld_set_fan_led_red(CODE_LED_ON, fan_index);
                }
            }
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "Not found attribte %d", attr->index);
            break;
        }
    }

    return count;
}


static ssize_t bsp_sysfs_fan_custom_motor_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    ssize_t index = 0;
    int temp = 0;
    u8 tempu8;
    board_static_data *bd = bsp_get_board_data();
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    fan_motor_st *motor = container_of((struct kobject *)kobj, fan_motor_st, kobj_motor);

    int motor_index = motor->motor_index;
    int fan_index = motor->fan_index;
    int fan_pwm_ratio = 0;

    switch (attr->index)
    {
        case MOTOR_STATUS:
        {
            index = sprintf(buf, "%d", fan_info[fan_index].fan_motor[motor_index].motor_status);
            break;
        }
        case MOTOR_SPEED:
        {
            index = sprintf(buf, "%d", fan_info[fan_index].fan_motor[motor_index].speed);
            break;
        }
        case MOTOR_RATIO:
        {
            temp = (fan_info[fan_index].fan_motor[motor_index].speed) * 100 / (bd->fan_max_speed);
            index = sprintf(buf, "%d", temp);
            break;
        }
        case MOTOR_TOLERANCE:
        {
            index = sprintf(buf, "%d", 15);
            break;
        }
        case MOTOR_TARGET:
        {
            if (bsp_cpld_get_fan_pwm_reg(&tempu8) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "get fan pwm from cpld failed");
                index = sprintf(buf, "Error");
                break;
            }
            if (tempu8 <= bd->fan_min_speed_pwm)
            {
                fan_pwm_ratio = bd->fan_min_speed_pwm;
            }
            else
            {
                fan_pwm_ratio = (tempu8 - bd->fan_min_speed_pwm) * (bd->fan_max_pwm_speed_percentage - bd->fan_min_pwm_speed_percentage) / (bd->fan_max_speed_pwm - bd->fan_min_speed_pwm);
            }
            index = sprintf(buf, "%d", fan_pwm_ratio);
            break;
        }
        case MOTOR_DIRECTION:
        {
            index = sprintf(buf, "0");
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "Not found attribte %d", attr->index);
            index = sprintf(buf, "Not support fan %d attribute %d", fan_index, attr->index);
            break;
        }
    }
    return index;
}

static ssize_t bsp_sysfs_fan_custom_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    //int temp = 0;
    u8 tempu8 = 0;
    u8 status = 0;
    ssize_t index = 0;
    u8 fan_eeprom_info[FAN_EEPROM_STRING_MAX_LENGTH] = {0};
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    fan_info_st *fan_ptr = NULL;
    int fan_index = 0;
    board_static_data *bd = bsp_get_board_data();

    //case of NUM_FANS, num_fans is in fan root
    if ((struct kobject *)kobj != kobj_fan_root)
    {
        fan_ptr = container_of((struct kobject *)kobj, fan_info_st, kobj_fan);
        fan_index = fan_ptr->fan_index;
    }
    switch (attr->index)
    {
        case DIS_FAN_MON:
        {
            index = sprintf(buf, "%d\n", fan_monitor_task_sleep);
            break;
        }
        case DIS_ADJUST_SPEED:
        {
            index = sprintf(buf, "%d\n", fan_mon_not_adjust_speed);
            break;
        }
        case NUM_FANS:
        {
            index = sprintf(buf, "%d", (int)bd->fan_num);
            break;
        }
        case VENDOR_NAME:
        {
            memset(fan_eeprom_info, 0, sizeof(fan_eeprom_info));
            if (bsp_fan_get_info_from_eeprom(fan_index, attr->index, fan_eeprom_info) == ERROR_SUCCESS)
            {
                index = sprintf(buf, "%s\n", (char *)fan_eeprom_info);
            }
            else
            {
                index = sprintf(buf, "Unknown\n");
            }
            break;
        }
        case SN:
        case PRODUCT_NAME:
        {
            memset(fan_eeprom_info, 0, sizeof(fan_eeprom_info));
            if (bsp_fan_get_info_from_eeprom(fan_index, attr->index, fan_eeprom_info) == ERROR_SUCCESS)
            {
                index = sprintf(buf, "%s\n", (char *)fan_eeprom_info);
            }
            else
            {
                index = sprintf(buf, "Unknown\n");
            }
            break;
        }
        case PN:
        {
            index = sprintf(buf, "N/A\n");
            break;
        }
        case HW_VERSION:
        {
            memset(fan_eeprom_info, 0, sizeof(fan_eeprom_info));
            if (bsp_fan_get_info_from_eeprom(fan_index, attr->index, fan_eeprom_info) == ERROR_SUCCESS)
            {
                index = sprintf(buf, "%s\n", (fan_eeprom_info[0] == 0xff || fan_eeprom_info[0] == 0x0) ? "1.0" : (char *)fan_eeprom_info);
            }
            else
            {
                index = sprintf(buf, "Unknown\n");
            }
            break;
        }
        case NUM_MOTORS:
        {
            index = sprintf(buf, "%d", (int)bd->motors_per_fan);
            break;
        }
        case STATUS:
        case LED_STATUS:
        {
            status = fan_info[fan_index].status;
            if (attr->index == STATUS)
            {
                index = sprintf(buf, "%d", status);
            }
            else
            {
                index = sprintf(buf, "%d", bsp_fan_get_status_color(status));
                //printk("bsp_sysfs_fan_custom_get_attr LED_STATUS status is %d \r\n", status);
            }
            break;
        }
        case FAN_SPEED_PWM:
        {
            if (bsp_cpld_get_fan_pwm_reg(&tempu8) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "get fan pwm from cpld failed");
                index = sprintf(buf, "%s", "Failed!");
            }
            else
            {
                index = sprintf(buf, "%d", tempu8);
            }
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_INFO, "Not found attribte %d", attr->index);
            index = sprintf(buf, "Not support fan %d attribute %d", fan_index, attr->index);
            break;
        }
    }

    return index;

}

static ssize_t bsp_sysfs_fan_get_attr(struct device *dev, struct device_attribute *da, char *buf)
{
    int motor_index = 0;
    int index = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int fan_index = bsp_sysfs_fan_get_index_from_dev(dev);
    if (fan_index == -1)
    {
        return sprintf(buf, "Error for fan index");
    }

    switch (attr->index)
    {
        case SENSOR_NAME:
        {
            index = sprintf(buf, "Fan%d %s\n", fan_index + 1, bsp_fan_get_status_string(fan_info[fan_index].status));
            break;
        }
        case FAN1_MIN:
        case FAN2_MIN:
        {
            index = sprintf(buf, "%d\n", bsp_get_board_data()->fan_min_speed);
            break;
        }
        case FAN1_MAX:
        case FAN2_MAX:
        {
            index = sprintf(buf, "%d\n", bsp_get_board_data()->fan_max_speed);
            break;
        }
        case FAN1_INPUT:
        case FAN2_INPUT:
        {
            motor_index = attr->index == FAN1_INPUT ? 0 : 1;
            index = sprintf(buf, "%d\n", fan_info[fan_index].fan_motor[motor_index].speed);
            break;
        }
        case FAN1_PULSES:
        case FAN2_PULSES:
        {
            index = sprintf(buf, "%d\n", fan_info[fan_index].pwm);
            break;
        }
        case FAN1_LABEL:
        case FAN2_LABEL:
        {
            motor_index = attr->index == FAN1_LABEL ? 0 : 1;
            index = sprintf(buf, "motor%d(%s)\n", motor_index + 1, motor_index == 1 ? "outer" : "inner");
            break;
        }
        case FAN1_ENABLE:
        case FAN2_ENABLE:
        {
            index = sprintf(buf, "1\n");
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


static SENSOR_DEVICE_ATTR(name,       S_IRUGO, bsp_sysfs_fan_get_attr, NULL, SENSOR_NAME);
static SENSOR_DEVICE_ATTR(fan1_min,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_MIN);
static SENSOR_DEVICE_ATTR(fan1_max,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_MAX);
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_INPUT);
static SENSOR_DEVICE_ATTR(pwm1,       S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_PULSES);
//static SENSOR_DEVICE_ATTR(fan1_speed, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_SPEED);
static SENSOR_DEVICE_ATTR(fan1_label, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_LABEL);
static SENSOR_DEVICE_ATTR(fan1_enable, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_ENABLE);
//static SENSOR_DEVICE_ATTR(fan1_alarm, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_ALARM);
static SENSOR_DEVICE_ATTR(fan2_min,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_MIN);
static SENSOR_DEVICE_ATTR(fan2_max,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_MAX);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_INPUT);
static SENSOR_DEVICE_ATTR(pwm2,       S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_PULSES);
//static SENSOR_DEVICE_ATTR(fan2_speed, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_SPEED);
static SENSOR_DEVICE_ATTR(fan2_label, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_LABEL);
static SENSOR_DEVICE_ATTR(fan2_enable, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_ENABLE);
//static SENSOR_DEVICE_ATTR(fan2_alarm, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_ALARM);


static SENSOR_DEVICE_ATTR(num_fans,     S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, NUM_FANS);
static SENSOR_DEVICE_ATTR(vendor_name, S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, VENDOR_NAME);
static SENSOR_DEVICE_ATTR(product_name, S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, PRODUCT_NAME);
static SENSOR_DEVICE_ATTR(sn,           S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, SN);
static SENSOR_DEVICE_ATTR(pn,           S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, PN);
static SENSOR_DEVICE_ATTR(hw_version,   S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, HW_VERSION);
static SENSOR_DEVICE_ATTR(num_motors,   S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, NUM_MOTORS);
static SENSOR_DEVICE_ATTR(status,       S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, STATUS);
static SENSOR_DEVICE_ATTR(led_status,   S_IRUGO | S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, LED_STATUS);


static SENSOR_DEVICE_ATTR(motor_speed,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_SPEED);
static SENSOR_DEVICE_ATTR(motor_target,    S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TARGET);
static SENSOR_DEVICE_ATTR(motor_tolerance, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TOLERANCE);
static SENSOR_DEVICE_ATTR(motor_ratio,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_RATIO);
static SENSOR_DEVICE_ATTR(motor_direction, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_DIRECTION);
static SENSOR_DEVICE_ATTR(motor_status,    S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_STATUS);


//附合tcsos规范
static SENSOR_DEVICE_ATTR(speed,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_SPEED);
static SENSOR_DEVICE_ATTR(speed_target,    S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TARGET);
static SENSOR_DEVICE_ATTR(speed_tolerance, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TOLERANCE);
static SENSOR_DEVICE_ATTR(ratio,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_RATIO);
static SENSOR_DEVICE_ATTR(direction, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_DIRECTION);




//debug node
static SENSOR_DEVICE_ATTR(disable_fan_mon,        S_IRUGO | S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, DIS_FAN_MON);
static SENSOR_DEVICE_ATTR(disable_adjust_speed,  S_IRUGO | S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, DIS_ADJUST_SPEED);
static SENSOR_DEVICE_ATTR(fan_speed_pwm,         S_IRUGO | S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, FAN_SPEED_PWM);



//attribute array
static struct attribute *fan_attributes[] =
{
    &sensor_dev_attr_name.dev_attr.attr,
    &sensor_dev_attr_fan1_min.dev_attr.attr,
    &sensor_dev_attr_fan1_max.dev_attr.attr,
    &sensor_dev_attr_fan1_input.dev_attr.attr,
    &sensor_dev_attr_pwm1.dev_attr.attr,
    &sensor_dev_attr_fan1_label.dev_attr.attr,
    &sensor_dev_attr_fan1_enable.dev_attr.attr,
    &sensor_dev_attr_fan2_min.dev_attr.attr,
    &sensor_dev_attr_fan2_max.dev_attr.attr,
    &sensor_dev_attr_fan2_input.dev_attr.attr,
    &sensor_dev_attr_pwm2.dev_attr.attr,
    &sensor_dev_attr_fan2_label.dev_attr.attr,
    &sensor_dev_attr_fan2_enable.dev_attr.attr,
    NULL
};

static struct attribute *fan_custom_attributes[] =
{
    &sensor_dev_attr_vendor_name.dev_attr.attr,
    &sensor_dev_attr_product_name.dev_attr.attr,
    &sensor_dev_attr_sn.dev_attr.attr,
    &sensor_dev_attr_pn.dev_attr.attr,
    &sensor_dev_attr_hw_version.dev_attr.attr,
    &sensor_dev_attr_num_motors.dev_attr.attr,
    &sensor_dev_attr_status.dev_attr.attr,
    &sensor_dev_attr_led_status.dev_attr.attr,
    NULL
};

static struct attribute *fan_custom_moter_attributes[] =
{
    &sensor_dev_attr_motor_speed.dev_attr.attr,
    &sensor_dev_attr_motor_target.dev_attr.attr,
    &sensor_dev_attr_motor_tolerance.dev_attr.attr,
    &sensor_dev_attr_motor_ratio.dev_attr.attr,
    &sensor_dev_attr_motor_direction.dev_attr.attr,
    &sensor_dev_attr_motor_status.dev_attr.attr,
    &sensor_dev_attr_speed.dev_attr.attr,
    &sensor_dev_attr_speed_target.dev_attr.attr,
    &sensor_dev_attr_speed_tolerance.dev_attr.attr,
    &sensor_dev_attr_ratio.dev_attr.attr,
    &sensor_dev_attr_direction.dev_attr.attr,
    NULL
};

static struct attribute *fan_debug_attributes[] =
{
    &sensor_dev_attr_disable_fan_mon.dev_attr.attr,
    &sensor_dev_attr_disable_adjust_speed.dev_attr.attr,
    &sensor_dev_attr_fan_speed_pwm.dev_attr.attr,
    NULL
};

//attribute groups
static const struct attribute_group fan_attribute_group =
{
    .attrs = fan_attributes,
};

static const struct attribute_group fan_custom_attribute_group =
{
    .attrs = fan_custom_attributes,
};

static const struct attribute_group fan_custom_motor_attribute_group =
{
    .attrs = fan_custom_moter_attributes,
};

static const struct attribute_group fan_debug_attribute_group =
{
    .attrs = fan_debug_attributes,
};

static struct attribute *fan_customer_device_attributes_num_fan[] =
{
    &sensor_dev_attr_num_fans.dev_attr.attr,
    NULL
};

static const struct attribute_group fan_customer_group_num_fan =
{
    .attrs = fan_customer_device_attributes_num_fan,
};
extern int bsp_sensor_get_max6696_temp(int max6696_index, int spot_index, s8 *value);

int h3c_adjust_fan_speed(void)
{

    //获取温度bsp_sensor_get_max6696_temp(0, 0~2, return)
    //设置转速bsp_cpld_set_fan_pwm_reg()
    int i = 0;
    int j = 0;
    s8 curr_max_temperature = 0;
    s8 temp_temperature = 0;

    u16 curve_temp_min = 0;
    u16 curve_temp_max = 0;
    u8  curve_pwm_min = 0;
    u8  curve_pwm_max = 0;
    u8  target_pwm = 0;
    u8  target_pwm_with_fan_absent = 0;
    u8  current_running_fan_num = 0;
    u16 k = 0;

    board_static_data *bd = bsp_get_board_data();

    if (fan_mon_not_adjust_speed != 0)
    {
        DBG_ECHO(DEBUG_DBG, "fan speed adjustment is stopped. fan_mon_not_adjust_speed=%d", fan_mon_not_adjust_speed);
        return ERROR_SUCCESS;
    }

    for (i = 0; i < bd->fan_num; i++)
    {
        if (fan_info[i].status == FAN_STATUS_NORMAL)
            current_running_fan_num ++;
    }

    if (current_running_fan_num == 0)
    {
        SYSLOG(LOG_LEVEL_CRIT, "WARNING:No fan is normal!");
        return ERROR_FAILED;
    }

    for (j = 0; j < bd->max6696_num; j++)
    {
        for (i = 0; i < MAX6696_SPOT_NUM; i++)
        {
            if (bsp_sensor_get_max6696_temp(j, i, &temp_temperature) == ERROR_SUCCESS)
            {
                curr_max_temperature = curr_max_temperature < temp_temperature ? temp_temperature : curr_max_temperature;
            }
            else
            {
                DBG_ECHO(DEBUG_INFO, "fan task get max6696(%d) temperature %d failed ", j, i);
            }
        }
    }

    DBG_ECHO(DEBUG_DBG, "current temp = %d", curr_max_temperature);

    curve_pwm_max = bd->fan_max_speed_pwm;
    curve_pwm_min = bd->fan_min_speed_pwm;
    curve_temp_min = bd->fan_temp_low;
    curve_temp_max = bd->fan_temp_high;

    if (curr_max_temperature <= curve_temp_min)
    {
        target_pwm = curve_pwm_min;
    }
    else if (curr_max_temperature >= curve_temp_max)
    {
        target_pwm = curve_pwm_max;
    }
    else
    {
        //避免小数，整体值放大1000倍，再缩小1000倍
        k = (u16)(curve_pwm_max - curve_pwm_min) * 1000 / (curve_temp_max - curve_temp_min);
        target_pwm = curve_pwm_max - k * (curve_temp_max - curr_max_temperature) / 1000;
    }

    target_pwm_with_fan_absent = (bd->fan_num * target_pwm  / current_running_fan_num);   //按风扇数量线性计算总转速

    if (target_pwm_with_fan_absent > curve_pwm_max)
    {
        target_pwm_with_fan_absent = curve_pwm_max;
    }

    if (bsp_cpld_set_fan_pwm_reg(target_pwm_with_fan_absent) != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "set fan pwm 0x%x failed!", target_pwm_with_fan_absent);
    }

    DBG_ECHO(DEBUG_DBG, "k = %d, target_pwm = 0x%x", k, target_pwm_with_fan_absent);

    return ERROR_SUCCESS;
}



//风扇模块监控任务
int h3c_fan_kernel_monitor_thread(void *arg)
{
    int fan_index = (int)(long)arg;
    int motor_index = 0;
    u8 pwm = 0;
    u16 speed = 0;
    int temp_status = 0;
    board_static_data *bd = bsp_get_board_data();
    u8 absent = 0;
    u8 good = 0;

    DBG_ECHO(DEBUG_INFO, "fan%d monitor started!", fan_index + 1);

    while (1)
    {
        msleep(fan_mon_interval);
        if (kthread_should_stop())
        {
            break;
        }

        if (fan_monitor_task_sleep == 1)
        {
            continue;
        }

        if (bsp_cpld_get_fan_pwm_reg(&pwm) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "cpld get pwm failed!");
        }

        if (bsp_cpld_get_fan_absent(&absent, fan_index) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "fan %d get absent failed!", fan_index + 1);
            continue;
        }
        if (bsp_cpld_get_fan_status(&good, fan_index) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "fan %d get status failed!", fan_index + 1);
            continue;
        }

        //更新风扇状态, 两个马达的状态都是good才NORMAL，否则FAULT
        temp_status = (absent == CODE_FAN_ABSENT) ? FAN_STATUS_ABSENT :
                      ((good == CODE_FAN_GOOD) ? FAN_STATUS_NORMAL : FAN_STATUS_FAULT);


        if (temp_status != fan_info[fan_index].status)
        {
            if (fan_info[fan_index].status == FAN_STATUS_ABSENT)
            {
                //wait for the fan status ready
                msleep(4000);
            }
        }

        fan_info[fan_index].status = temp_status;
        fan_info[fan_index].pwm = pwm;

        for (motor_index = 0; motor_index < bd->motors_per_fan; motor_index ++)
        {
            if (bsp_cpld_get_fan_speed(&speed, fan_index, motor_index) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "fan %d moter %d get speed failed!", fan_index + 1, motor_index + 1);
            }
            else
            {
                fan_info[fan_index].fan_motor[motor_index].speed  = speed;

                switch (temp_status)
                {
                    case FAN_STATUS_FAULT:
                        fan_info[fan_index].fan_motor[motor_index].motor_status = (speed < bd->fan_min_speed) ? FAN_STATUS_FAULT : FAN_STATUS_NORMAL;
                        break;
                    case FAN_STATUS_ABSENT:
                    case FAN_STATUS_NORMAL:
                    case FAN_STATUS_UNKNOWN:
                        fan_info[fan_index].fan_motor[motor_index].motor_status = temp_status;
                        break;
                    default:
                        fan_info[fan_index].fan_motor[motor_index].motor_status = FAN_STATUS_UNKNOWN;
                }
            }
        }


        //设置风扇led
        if (fan_info[fan_index].status == FAN_STATUS_NORMAL)
        {
            bsp_cpld_set_fan_led_green(CODE_LED_ON, fan_index);
            bsp_cpld_set_fan_led_red(CODE_LED_OFF, fan_index);
        }
        else
        {
            bsp_cpld_set_fan_led_green(CODE_LED_OFF, fan_index);
            bsp_cpld_set_fan_led_red(CODE_LED_ON, fan_index);
        }

        //调速
        h3c_adjust_fan_speed();

    }

    DBG_ECHO(DEBUG_INFO, "fan%d monitor exited!", fan_index + 1);
    return ERROR_SUCCESS;
}


//设置初始化入口函数
static int __init fan_init(void)
{

    int ret = ERROR_SUCCESS;
    int i = 0, j = 0;
    char temp_str[128] = {0};
    int bsp_fan_num;
    board_static_data *bd = bsp_get_board_data();


    DBG_ECHO(DEBUG_INFO, "fan module init started\n");

    bsp_fan_get_fan_number(&bsp_fan_num);

    memset(fan_info, 0, sizeof(fan_info));

    //create node for switch
    kobj_fan_root = kobject_create_and_add("fan", kobj_switch);

    if (kobj_fan_root == NULL)
    {
        DBG_ECHO(DEBUG_ERR, "kobj_fan_root create falled!\n");
        ret = -ENOMEM;
        goto exit;
    }

    //CHECK_CREATE_SYSFS_FILE(kobj_fan_root, sensor_dev_attr_num_fans.dev_attr, ret);
    CHECK_IF_ERROR_GOTO_EXIT(ret = sysfs_create_group(kobj_fan_root, &fan_customer_group_num_fan), "create num_fan group failed");

    //build sysfs directory
    for (i = 0; i < bsp_fan_num; i++)
    {
        sprintf(temp_str, "fan%d", (i + 1));
        fan_info[i].fan_index = i;
        fan_info[i].status = FAN_STATUS_UNKNOWN;

        //fan_info[i].kobj_fan = kobject_create_and_add(temp_str, kobj_fan_root);

        ret = kobject_init_and_add(&fan_info[i].kobj_fan, &static_kobj_ktype, kobj_fan_root, temp_str);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "kobj_fan %d create falled!\n", (i + 1));

        ret = sysfs_create_group(&(fan_info[i].kobj_fan), &fan_custom_attribute_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "fan %d custom attribute group create failed!\n", i + 1);

        //add motor group
        for (j = 0; j < bd->motors_per_fan; j++)
        {
            sprintf(temp_str, "motor%d", j);
            fan_info[i].fan_motor[j].motor_index = j;
            fan_info[i].fan_motor[j].fan_index = i;

            ret = kobject_init_and_add(&(fan_info[i].fan_motor[j].kobj_motor), &static_kobj_ktype, &(fan_info[i].kobj_fan), temp_str);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "motor%d kobject int and add failed!", j);

            ret = sysfs_create_group(&(fan_info[i].fan_motor[j].kobj_motor), &fan_custom_motor_attribute_group);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "motor%d attribute group created failed!", j);
        }

        // for sensors
        fan_info[i].parent_hwmon = hwmon_device_register(NULL);
        CHECK_IF_NULL_GOTO_EXIT(ret, fan_info[i].parent_hwmon, "fan %d hwmon register failed", i + 1);

        ret = sysfs_create_group(&(fan_info[i].parent_hwmon->kobj), &fan_attribute_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "fan %d attribute create failed!\n", i + 1);


    }


    for (i = 0; i < bsp_fan_num; i++)
    {
        DBG_ECHO(DEBUG_INFO, "fan%d monitor task creating...", i + 1);
        fan_monitor_task[i] = kthread_run(h3c_fan_kernel_monitor_thread, (void *)(long)i,  "h3c_fan%d_mon", i + 1);
        CHECK_IF_NULL_GOTO_EXIT(ret, fan_monitor_task[i], "fan%d monitor task failed creating!", i + 1);
    }

    //create fan debug node
    kobj_fan_debug = kobject_create_and_add("fan", kobj_debug);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_fan_debug, "fan debug kobject created failed!");

    ret = sysfs_create_group(kobj_fan_debug, &fan_debug_attribute_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "fan debug attribute group created failed!");



exit:
    if (ret != 0)
    {
        DBG_ECHO(DEBUG_ERR, "fan module init failed!\n");
        release_all_fan_kobj();
    }
    else
    {
        INIT_PRINT("fan module finished and success!");
    }


    return ret;
}

void release_all_fan_kobj(void)
{
    int bsp_fan_num = 0;
    int i = 0, j = 0;
    board_static_data *bd = bsp_get_board_data();
    bsp_fan_get_fan_number(&bsp_fan_num);

    for (i = 0; i < bsp_fan_num; i++)
    {
        for (j = 0; j < bd->motors_per_fan; j++)
        {
            if (fan_info[i].fan_motor[j].kobj_motor.state_initialized)
            {
                sysfs_remove_group(&(fan_info[i].fan_motor[j].kobj_motor), &fan_custom_motor_attribute_group);
                kobject_put(&(fan_info[i].fan_motor[j].kobj_motor));
            }
        }
        if (fan_info[i].kobj_fan.state_initialized)
        {
            sysfs_remove_group(&(fan_info[i].kobj_fan), &fan_custom_attribute_group);
            kobject_put(&(fan_info[i].kobj_fan));
        }

        if (fan_info[i].parent_hwmon != NULL)
        {
            sysfs_remove_group(&(fan_info[i].parent_hwmon->kobj), &fan_attribute_group);
            hwmon_device_unregister(fan_info[i].parent_hwmon);
        }
    }

    for (i = 0; i < bsp_fan_num; i++)
    {
        if (fan_monitor_task[i] != NULL)
            kthread_stop(fan_monitor_task[i]);
        fan_monitor_task[i] = NULL;
    }

    if (kobj_fan_debug != NULL)
    {
        sysfs_remove_group(kobj_fan_debug, &fan_debug_attribute_group);
        kobject_put(kobj_fan_debug);
    }

    if (kobj_fan_root != NULL)
    {
        sysfs_remove_group(kobj_fan_root, &fan_customer_group_num_fan);
        kobject_put(kobj_fan_root);
    }
    return;
}

//设置出口函数
static void __exit fan_exit(void)
{
    release_all_fan_kobj();
    INIT_PRINT("module fan uninstalled !\n");
    return;
}

module_init(fan_init);
module_exit(fan_exit);
