#include "bsp_base.h"
#include "static_ktype.h"

void release_all_fan_kobj(void);

enum fan_hwmon_sysfs_attributes {
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

enum fan_sensor_sysfs_attributes {
    DIS_FAN_MON = FAN_HWMON_ATTR_BUTT + 1,
    DIS_ADJUST_SPEED,
    
    NUM_FANS,
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
    FAN_LED_OFF =0,
    FAN_LED_GREEN,
    FAN_LED_RED,
};

typedef struct {
    int motor_index;
    int speed;
    int fan_index;
    int motor_status;
    struct kobject kobj_motor;
} fan_motor_st;

typedef struct {
    int status;
    int fan_index;
    int pwm;
    struct device *parent_hwmon;
    struct kobject kobj_fan;
    fan_motor_st fan_motor[MAX_FAN_MOTER_NUM];
} fan_info_st;

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static int loglevel = DEBUG_INFO | DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt, ##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)

#define MODULE_NAME "fan"
#define FAN_EEPROM_STRING_MAX_LENGTH 128
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct timer_list timer;
static struct work_struct work;
static struct workqueue_struct *fan_workqueue = NULL;

static fan_info_st fan_info[MAX_FAN_NUM];
static struct kobject *kobj_fan_root = NULL;
static struct kobject *kobj_fan_debug = NULL;   
int fan_monitor_task_sleep = 0;     
int fan_mon_not_adjust_speed = 1;    
char *fan_status_string[] = {"Absent", "OK", "NOT OK", "Unknown"};

int bsp_fan_get_status(int fan_index)
{
    if ((0 > fan_index) || 
        (MAX_FAN_NUM <= fan_index)) {
        DBG_ECHO(DEBUG_ERR, "fan id %d is invalid.", fan_index);
        return FAN_STATUS_UNKNOWN;
    } else {
        return fan_info[fan_index].status;
    }
}

char *bsp_fan_get_status_string(int status)
{
    if ((status >= FAN_STATUS_ABSENT) && 
        (status <= FAN_STATUS_FAULT)) {
        return fan_status_string[status];
    } else {
        DBG_ECHO(DEBUG_ERR, "fan unknown status %d", status);
        return fan_status_string[FAN_STATUS_UNKNOWN];
    }
}

int bsp_fan_get_status_color(int status)
{
    int color;
    switch(status) {
    case FAN_STATUS_NORMAL:
        color = LED_COLOR_GREEN;
        break;
    case FAN_STATUS_FAULT:
        color = LED_COLOR_RED;
        break;
    default:
        color = LED_COLOR_DARK;
    }
    
    return color;
}

int bsp_fan_get_fan_number(int *fan_number)
{
    if (!fan_number) {
        return ERROR_FAILED;
    }
    
    *fan_number = bsp_get_board_data()->fan_num;
    return ERROR_SUCCESS;
}

int _bsp_fan_get_info_from_eeprom(int fan_index, int info_type, OUT u8 *info_data)
{
    u16 start_offset = 0;
    u16 read_length = 0;
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();

    if (!bd || !info_data) {
        return ERROR_FAILED;
    }
    
    if ((fan_index < 0) || 
        (fan_index > bd->fan_num)) {
        DBG_ECHO(DEBUG_ERR, "fan index %d is invalied!", fan_index);
        ret = ERROR_FAILED;
        goto exit;
    }
    
    switch(info_type) {
    case PRODUCT_NAME:
        start_offset = REG_ADDR_FAN_PDT_NAME;
        read_length = FAN_PDT_NAME_BYTE_COUNT;
        break;
    case SN:
        start_offset = REG_ADDR_FAN_SN;
        read_length = FAN_SN_BYTE_COUNT;
        break;
    case HW_VERSION:
        start_offset = REG_ADDR_FAN_HW_VER;
        read_length = FAN_HW_VER_BYTE_COUNT;
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "not support info type %d", info_type);
        ret = ERROR_FAILED;
        goto exit;
        break;
    }
    
    if (lock_i2c_path(I2C_DEV_FAN + fan_index) == ERROR_SUCCESS)
    {
        ret = bsp_i2c_24LC128_eeprom_read_bytes(bd->i2c_addr_fan[fan_index], start_offset, read_length, info_data);    
    }
    unlock_i2c_path();
    
exit:
    return ret;
}

int bsp_fan_get_info_from_eeprom (int fan_index, int info_type, OUT u8 *info_data)
{
    u8 absent = 0;

    if (!info_data) 
        return -ENOMEM;
    
    if (bsp_cpld_get_fan_absent(&absent, fan_index) != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "fan %d get absent failed!", fan_index + 1);
        return ERROR_FAILED;
    }
    
    if(1 == absent) {
        info_data[0] = 0x00;
        return ERROR_FAILED;
    }
    
    return _bsp_fan_get_info_from_eeprom(fan_index, info_type, info_data);
}

int bsp_sysfs_fan_get_index_from_dev (struct device *dev)
{
    int i;
    int fan_num = bsp_get_board_data()->fan_num; 
    
    for (i = 0; i < fan_num; i++) {
        if (fan_info[i].parent_hwmon == dev)
            return i;
    }
    
    DBG_ECHO(DEBUG_ERR, "Not found matched fan hwmon, dev=%p", dev);
    return -1;
}

static ssize_t bsp_sysfs_fan_custom_set_attr(struct device *kobject, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    fan_info_st * fan_ptr = container_of((struct kobject *)kobject, fan_info_st, kobj_fan);
    int fan_index = fan_ptr->fan_index;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    
    switch (attr->index) {
    case DIS_FAN_MON:
    case DIS_ADJUST_SPEED:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
        } else {
            attr->index == DIS_FAN_MON ? (fan_monitor_task_sleep = (temp != 0)) : (fan_mon_not_adjust_speed = (temp != 0));
        }
        break;
    case FAN_SPEED_PWM:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
            return count;
        }
        
        if (bsp_cpld_set_fan_pwm_reg((u8)temp) != ERROR_SUCCESS) {
            DBG_ECHO(DEBUG_ERR, "set fan pwm cpld failed");
        }
        break;
    case LED_STATUS:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
            return count;
        }
        
        if (temp == FAN_LED_OFF) {
            bsp_cpld_set_fan_led_red(CODE_LED_OFF, fan_index);
            bsp_cpld_set_fan_led_green(CODE_LED_OFF, fan_index);
        } else if (temp == FAN_LED_GREEN) {
            bsp_cpld_set_fan_led_green(CODE_LED_ON, fan_index);
            bsp_cpld_set_fan_led_red(CODE_LED_OFF, fan_index);
        } else if(temp == FAN_LED_RED) {
            bsp_cpld_set_fan_led_green(CODE_LED_OFF, fan_index);
            bsp_cpld_set_fan_led_red(CODE_LED_ON, fan_index);
        } else {
            return count;
        }
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "Not found attribte %d", attr->index);
        break;
    }
    
    return count;
}

static ssize_t bsp_sysfs_fan_custom_motor_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    ssize_t index = 0;
    int temp = 0;
    int speed_target = 0;
    u8 tempu8 = 0;
    board_static_data *bd = bsp_get_board_data();
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    fan_motor_st *motor = container_of((struct kobject *)kobj, fan_motor_st, kobj_motor);
    int motor_index = motor->motor_index;
    int fan_index = motor->fan_index;
    int fan_pwm_ratio = 0;

    switch(attr->index) {
    case MOTOR_STATUS:
        index = sprintf(buf, "%d\n", fan_info[fan_index].fan_motor[motor_index].motor_status);
        break;
    case MOTOR_SPEED:
        index = sprintf(buf, "%d\n", fan_info[fan_index].fan_motor[motor_index].speed);
        break;
    case MOTOR_RATIO:
        temp = (fan_info[fan_index].fan_motor[motor_index].speed) * 100 / (bd->fan_max_speed);
        index = sprintf(buf, "%d\n", temp);
        break;
    case MOTOR_TOLERANCE:
    case MOTOR_TARGET:
        if (bsp_cpld_get_fan_pwm_reg(&tempu8) != ERROR_SUCCESS) {
            DBG_ECHO(DEBUG_ERR, "get fan pwm from cpld failed");
            index = sprintf(buf, "Error");
            break;
        }
        
        if (tempu8 <= bd->fan_min_speed_pwm) {
            fan_pwm_ratio = bd->fan_min_speed_pwm;
        } else {
            fan_pwm_ratio = (tempu8 - bd->fan_min_speed_pwm) * (bd->fan_max_pwm_speed_percentage - bd->fan_min_pwm_speed_percentage) / (bd->fan_max_speed_pwm - bd->fan_min_speed_pwm);
        }
        
        speed_target = bd->fan_target_speed_coef0[motor_index] + bd->fan_target_speed_coef1[motor_index] * fan_pwm_ratio + bd->fan_target_speed_coef2[motor_index] * fan_pwm_ratio * fan_pwm_ratio  + bd->fan_target_speed_coef3[motor_index] * fan_pwm_ratio * fan_pwm_ratio * fan_pwm_ratio;
        speed_target = speed_target / 1000;
        
        if (attr->index == MOTOR_TARGET) {
            index = sprintf(buf, "%d\n", speed_target);
        } else {
            index = sprintf(buf, "%d\n", (int)(speed_target * 20 / 100));
        }
        break;
    case MOTOR_DIRECTION:
        index = sprintf(buf, "0\n");
        break;
    }
    
    return index;
}

static ssize_t bsp_sysfs_fan_custom_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    u8 tempu8 = 0;
    u8 status = 0;
    ssize_t index = 0;
    u8 fan_eeprom_info[FAN_EEPROM_STRING_MAX_LENGTH] = {0};
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    fan_info_st *fan_ptr = container_of((struct kobject *)kobj, fan_info_st, kobj_fan);
    int fan_index = fan_ptr->fan_index;
    board_static_data *bd = bsp_get_board_data();

    if (!bd) {
        index = sprintf(buf, "%s", "Unknown\n");
        return index;
    }
    
    switch(attr->index) {
    case DIS_FAN_MON:
        index = sprintf(buf, "%d\n", fan_monitor_task_sleep);	
        break;
    case DIS_ADJUST_SPEED:
        index = sprintf(buf, "%d\n", fan_mon_not_adjust_speed);
        break;
    case NUM_FANS:
        index = sprintf(buf, "%d\n", (int)bd->fan_num);
        break;
    case SN:
    case PRODUCT_NAME:
        memset(fan_eeprom_info, 0, sizeof(fan_eeprom_info));
	    if (bsp_fan_get_info_from_eeprom(fan_index, attr->index, fan_eeprom_info) == ERROR_SUCCESS) {
            index = sprintf(buf, "%s\n", (char *)fan_eeprom_info);
	    } else {
            index = sprintf(buf, "Unknown\n");
        }
        break;
    case PN:
        index = sprintf(buf, "N/A\n");
        break;
    case HW_VERSION:
        memset(fan_eeprom_info, 0, sizeof(fan_eeprom_info));
        if (bsp_fan_get_info_from_eeprom(fan_index, attr->index, fan_eeprom_info) == ERROR_SUCCESS) {
            index = sprintf(buf, "%s\n", (fan_eeprom_info[0] == 0xff || fan_eeprom_info[0] == 0x0) ? "1.0" : (char *)fan_eeprom_info);
        } else {
            index = sprintf(buf, "Unknown\n");
        }
        break;
    case NUM_MOTORS:
        index = sprintf(buf, "%d\n", (int)bd->motors_per_fan);
        break;
    case STATUS:
    case LED_STATUS:
        status = fan_info[fan_index].status;
        if (attr->index == STATUS) {
            index = sprintf(buf, "%d\n", status);
        } else {
            index = sprintf(buf, "%d\n", bsp_fan_get_status_color(status));
        }
        break;
    case FAN_SPEED_PWM:
        if (bsp_cpld_get_fan_pwm_reg(&tempu8) != ERROR_SUCCESS) {
            DBG_ECHO(DEBUG_ERR, "get fan pwm from cpld failed");
            index = sprintf(buf, "%s\n", "Failed!");
        } else {
            index = sprintf(buf, "%d\n", tempu8);
        }
        break;
    default:
        DBG_ECHO(DEBUG_INFO, "Not found attribte %d", attr->index);
        index = sprintf(buf, "Not support fan %d attribute %d\n", fan_index, attr->index);
        break;
    }
    
    return index; 
}

static ssize_t bsp_sysfs_fan_get_attr(struct device *dev, struct device_attribute *da, char *buf)
{
    int motor_index = 0;
    int index = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da); 
    int fan_index = bsp_sysfs_fan_get_index_from_dev(dev);
    
    if (fan_index == -1) {
        return sprintf(buf, "Error for fan index");
    }

    switch (attr->index) {
    case SENSOR_NAME:
		index = sprintf(buf, "Fan%d %s\n", fan_index + 1, bsp_fan_get_status_string(fan_info[fan_index].status));
		break;
	case FAN1_MIN:
    case FAN2_MIN:
		index = sprintf(buf, "%d\n",bsp_get_board_data()->fan_min_speed);
		break;
	case FAN1_MAX:
    case FAN2_MAX:
		index = sprintf(buf, "%d\n", bsp_get_board_data()->fan_max_speed);
		break;
	case FAN1_INPUT:
    case FAN2_INPUT:
		motor_index = attr->index == FAN1_INPUT ? 0 : 1;
		index = sprintf(buf, "%d\n", fan_info[fan_index].fan_motor[motor_index].speed);
		break;
	case FAN1_PULSES:
    case FAN2_PULSES:
		index = sprintf(buf, "%d\n", fan_info[fan_index].pwm);
		break;

	case FAN1_LABEL:
    case FAN2_LABEL:   
        motor_index = attr->index == FAN1_LABEL ? 0 : 1;
		index = sprintf(buf, "motor%d(%s)\n", motor_index + 1, motor_index == 1 ? "outer" : "inner");
		break;
	case FAN1_ENABLE:
    case FAN2_ENABLE:
		index = sprintf(buf, "1\n");
		break;
	default:
	    index = sprintf(buf, "Not support\n");	 
    }
    
	return index;	
}

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
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

static ssize_t bsp_default_debug_help (char *buf)
{
    ssize_t index = 0;

    index += sprintf (buf + index, "%s", " Fan monitor control command:\n");
    index += sprintf (buf + index, "%s", "   0 ------ enable monitor\n");
    index += sprintf (buf + index, "%s", "   1 ------ disable monitor\n\n");
    index += sprintf (buf + index, "%s", " Disable/Enable monitor:\n");
    index += sprintf (buf + index, "%s", "   echo command > /sys/switch/debug/fan/disable_fan_mon\n\n");
    index += sprintf (buf + index, "%s", "   eg:\n\n");
    index += sprintf (buf + index, "%s", "      echo 1 > /sys/switch/debug/fan/disable_fan_mon\n\n");   
    index += sprintf (buf + index, "%s", " Get fan pwm:\n");
    index += sprintf (buf + index, "%s", "    cat /sys/switch/debug/fan/fan_speed_pwm\n\n");
    index += sprintf (buf + index, "%s", "    eg:\n\n");
    index += sprintf (buf + index, "%s", "        root@sonic:/sys/switch/debug/fan# cat fan_speed_pwm\n");
    index += sprintf (buf + index, "%s", "         63\n\n");
    index += sprintf (buf + index, "%s", " Set fan pwm:\n");
    index += sprintf (buf + index, "%s", "    Warning: To manually control the fan PWM, the first step is to disable the fan monitoring tasks.\n\n");
    index += sprintf (buf + index, "%s", "    echo command > /sys/switch/debug/fan/fan_speed_pwm\n\n");
    index += sprintf (buf + index, "%s", "    eg:\n\n");
    index += sprintf (buf + index, "%s", "       echo 60 > /sys/switch/debug/fan/fan_speed_pwm\n\n");
    index += sprintf (buf + index, "%s", " Fan led set:\n");
    index += sprintf (buf + index, "%s", "    Warning: To manually control the fan led, the first step is to disable the fan monitoring tasks.\n\n");
    index += sprintf (buf + index, "%s", "    you can run 'cat /sys/switch/cpld/debug' get help.\n\n");
    index += sprintf (buf + index, "%s", "    eg:\n\n");
    index += sprintf (buf + index, "%s", "       root@sonic:/sys/switch/debug/fan# echo 0x7d:0xe0 > /sys/switch/debug/cpld/board_cpld\n\n");
    index += sprintf (buf + index, "%s", " Read fan eeprom:\n");
    index += sprintf (buf + index, "%s", "   you can run 'i2c_read.py' get help.\n\n");
    index += sprintf (buf + index, "%s", "   eg:\n\n");
    index += sprintf (buf + index, "%s", "      root@sonic:/home/admin# i2c_read.py 0x1b6 0x50 0x2 0x0 0x80\n");
    index += sprintf (buf + index, "%s", "      Read dev id 0x1b6 address 0x50 from 0x0 length 0x80\n");
    index += sprintf (buf + index, "%s", "      0000: 00 00 00 00 00 00 00 00  54 43 53 38 32 2d 31 32  * ........TCS82-12 *\n");
    index += sprintf (buf + index, "%s", "      0010: 30 2d 46 41 4e 00 00 00  00 00 00 00 00 00 00 00  * 0-FAN........... *\n");
    index += sprintf (buf + index, "%s", "      0020: 00 00 00 00 00 00 00 00  32 31 30 32 33 31 41 46  * ........210231AF *\n");
    index += sprintf (buf + index, "%s", "      0030: 41 35 32 30 32 30 30 38  32 32 30 31 00 00 00 00  * A52020082201.... *\n");
    index += sprintf (buf + index, "%s", "      0040: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  * ................ *\n");
    index += sprintf (buf + index, "%s", "      0050: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  * ................ *\n");
    index += sprintf (buf + index, "%s", "      0060: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  * ................ *\n");
    index += sprintf (buf + index, "%s", "      0070: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  * ................ *\n\n");
    
    return index;
}

static ssize_t bsp_default_debug_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return bsp_default_debug_help(buf);
}

static ssize_t bsp_default_debug_store (struct kobject *kobj, struct kobj_attribute *attr, const char* buf, size_t count)
{
	return count;
}
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static SENSOR_DEVICE_ATTR(name,       S_IRUGO, bsp_sysfs_fan_get_attr, NULL, SENSOR_NAME);
static SENSOR_DEVICE_ATTR(fan1_min,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_MIN);
static SENSOR_DEVICE_ATTR(fan1_max,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_MAX);
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_INPUT);
static SENSOR_DEVICE_ATTR(pwm1,       S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_PULSES);
static SENSOR_DEVICE_ATTR(fan1_label, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_LABEL);
static SENSOR_DEVICE_ATTR(fan1_enable,S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN1_ENABLE);
static SENSOR_DEVICE_ATTR(fan2_min,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_MIN);
static SENSOR_DEVICE_ATTR(fan2_max,   S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_MAX);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_INPUT);
static SENSOR_DEVICE_ATTR(pwm2,       S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_PULSES);
static SENSOR_DEVICE_ATTR(fan2_label, S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_LABEL);
static SENSOR_DEVICE_ATTR(fan2_enable,S_IRUGO, bsp_sysfs_fan_get_attr, NULL, FAN2_ENABLE);
static SENSOR_DEVICE_ATTR(num_fans,     S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, NUM_FANS);
static SENSOR_DEVICE_ATTR(product_name, S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, PRODUCT_NAME);
static SENSOR_DEVICE_ATTR(sn,           S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, SN);
static SENSOR_DEVICE_ATTR(pn,           S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, PN);
static SENSOR_DEVICE_ATTR(hw_version,   S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, HW_VERSION);
static SENSOR_DEVICE_ATTR(num_motors,   S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, NUM_MOTORS);
static SENSOR_DEVICE_ATTR(status,       S_IRUGO, bsp_sysfs_fan_custom_get_attr, NULL, STATUS);
static SENSOR_DEVICE_ATTR(led_status,   S_IRUGO|S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, LED_STATUS);
static SENSOR_DEVICE_ATTR(motor_speed,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_SPEED);
static SENSOR_DEVICE_ATTR(motor_target,    S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TARGET);
static SENSOR_DEVICE_ATTR(motor_tolerance, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TOLERANCE);
static SENSOR_DEVICE_ATTR(motor_ratio,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_RATIO);
static SENSOR_DEVICE_ATTR(motor_direction, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_DIRECTION);
static SENSOR_DEVICE_ATTR(motor_status,    S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_STATUS);
static SENSOR_DEVICE_ATTR(speed,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_SPEED);
static SENSOR_DEVICE_ATTR(speed_target,    S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TARGET);
static SENSOR_DEVICE_ATTR(speed_tolerance, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_TOLERANCE);
static SENSOR_DEVICE_ATTR(ratio,     S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_RATIO);
static SENSOR_DEVICE_ATTR(direction, S_IRUGO, bsp_sysfs_fan_custom_motor_get_attr, NULL, MOTOR_DIRECTION);
static SENSOR_DEVICE_ATTR(disable_fan_mon,        S_IRUGO|S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, DIS_FAN_MON);
static SENSOR_DEVICE_ATTR(disable_adjust_speed ,  S_IRUGO|S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, DIS_ADJUST_SPEED);
static SENSOR_DEVICE_ATTR(fan_speed_pwm ,         S_IRUGO|S_IWUSR, bsp_sysfs_fan_custom_get_attr, bsp_sysfs_fan_custom_set_attr, FAN_SPEED_PWM);

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct kobj_attribute loglevel_att =
    __ATTR(loglevel, S_IRUGO | S_IWUSR, bsp_default_loglevel_show, bsp_default_loglevel_store);

static struct kobj_attribute debug_att =
    __ATTR(debug, S_IRUGO | S_IWUSR, bsp_default_debug_show, bsp_default_debug_store);
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *fan_attributes[] = {
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

static struct attribute *fan_custom_attributes[] = {
    &sensor_dev_attr_product_name.dev_attr.attr,
    &sensor_dev_attr_sn.dev_attr.attr,
    &sensor_dev_attr_pn.dev_attr.attr,
    &sensor_dev_attr_hw_version.dev_attr.attr,
    &sensor_dev_attr_num_motors.dev_attr.attr,
    &sensor_dev_attr_status.dev_attr.attr,
    &sensor_dev_attr_led_status.dev_attr.attr,
    NULL
};

static struct attribute *fan_custom_moter_attributes[] = {
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
};

static struct attribute *fan_debug_attributes[] = {
    &sensor_dev_attr_disable_fan_mon.dev_attr.attr,
    &sensor_dev_attr_disable_adjust_speed.dev_attr.attr,
    &sensor_dev_attr_fan_speed_pwm.dev_attr.attr,
    NULL
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute *def_attrs[] = {
    &loglevel_att.attr,
    &debug_att.attr,
    NULL,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static const struct attribute_group fan_attribute_group = {
    .attrs = fan_attributes,
};

static const struct attribute_group fan_custom_attribute_group = {
    .attrs = fan_custom_attributes,
};

static const struct attribute_group fan_custom_motor_attribute_group = {
    .attrs = fan_custom_moter_attributes,
};

static const struct attribute_group fan_debug_attribute_group = {
    .attrs = fan_debug_attributes,
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute_group def_attr_group = {
    .attrs = def_attrs,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

int h3c_adjust_fan_speed(void)
{
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

    if (fan_mon_not_adjust_speed != 0) {
        DBG_ECHO(DEBUG_DBG, "fan speed adjustment is stopped. fan_mon_not_adjust_speed=%d", fan_mon_not_adjust_speed);
        return ERROR_SUCCESS;
    }

    for (i = 0; i < bd->fan_num; i++) {
        if (fan_info[i].status == FAN_STATUS_NORMAL)
            current_running_fan_num ++; 
    }

    if (current_running_fan_num == 0) {
        SYSLOG(LOG_LEVEL_CRIT,"WARNING:No fan is normal!");
        return ERROR_FAILED;
    }
    
    for (j = 0; j < bd->max6696_num; j++) {    
        for (i = 0; i < MAX6696_SPOT_NUM; i++) {
            if (bsp_sensor_get_max6696_temp(j, i, &temp_temperature) == ERROR_SUCCESS) {
                curr_max_temperature = curr_max_temperature < temp_temperature ? temp_temperature : curr_max_temperature;
            } else {
                DBG_ECHO(DEBUG_INFO, "fan task get max6696(%d) temperature %d failed ", j, i);
            }
        }
    }
    
    DBG_ECHO(DEBUG_DBG, "current temp = %d", curr_max_temperature);
    
    curve_pwm_max = bd->fan_max_speed_pwm;
    curve_pwm_min = bd->fan_min_speed_pwm;
    curve_temp_min = bd->fan_temp_low;
    curve_temp_max = bd->fan_temp_high;
    
    if (curr_max_temperature <= curve_temp_min) {
        target_pwm = curve_pwm_min;
    } else if (curr_max_temperature >= curve_temp_max) {
        target_pwm = curve_pwm_max;
    } else {
        k = (u16)(curve_pwm_max - curve_pwm_min) * 1000 / (curve_temp_max - curve_temp_min);
        target_pwm = curve_pwm_max - k * (curve_temp_max - curr_max_temperature) / 1000;
    }

    target_pwm_with_fan_absent = (bd->fan_num * target_pwm  / current_running_fan_num);
    if (target_pwm_with_fan_absent > curve_pwm_max) {
        target_pwm_with_fan_absent = curve_pwm_max;
    }

    if (bsp_cpld_set_fan_pwm_reg(target_pwm_with_fan_absent) != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "set fan pwm 0x%x failed!", target_pwm_with_fan_absent);
    }
    
    DBG_ECHO(DEBUG_DBG, "k = %d, target_pwm = 0x%x", k, target_pwm_with_fan_absent); 
    return ERROR_SUCCESS;
}

static void bsp_fan_monitor_proc (void)
{
    int i = 0;
    int motor_index = 0;
    u8 pwm = 0;
    u8 absent = 0;
    u8 good = 0;
    u16 speed = 0;
    int temp_status = 0; 
    board_static_data *bd = bsp_get_board_data();
    
    if (bsp_cpld_get_fan_pwm_reg(&pwm) != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "cpld get pwm failed!");
    }

    for (i = 0; i < bd->fan_num; i ++) {
        if (bsp_cpld_get_fan_absent(&absent, i) != ERROR_SUCCESS) {
            DBG_ECHO(DEBUG_ERR, "fan %d get absent failed!", i + 1);
            continue;
        }
        
        if (bsp_cpld_get_fan_status(&good, i) != ERROR_SUCCESS) {
            DBG_ECHO(DEBUG_ERR, "fan %d get status failed!", i + 1);
            continue;
        }

        temp_status = (absent == CODE_FAN_ABSENT) ? FAN_STATUS_ABSENT : 
                       ((good == CODE_FAN_GOOD) ? FAN_STATUS_NORMAL : FAN_STATUS_FAULT);   

        if (temp_status != fan_info[i].status) {
            SYSLOG(LOG_LEVEL_INFO, "Fan%d status changed from %s to %s", i + 1, bsp_fan_get_status_string(fan_info[i].status), bsp_fan_get_status_string(temp_status));
        }
        
        fan_info[i].status = temp_status;
        fan_info[i].pwm = pwm;
    
        for (motor_index = 0; motor_index < bd->motors_per_fan; motor_index ++) {
            if (bsp_cpld_get_fan_speed(&speed, i, motor_index) != ERROR_SUCCESS) {
                DBG_ECHO(DEBUG_ERR, "fan %d moter %d get speed failed!", i + 1, motor_index + 1);
            } else {
                fan_info[i].fan_motor[motor_index].speed  = speed;
                
                switch (temp_status) {
                case FAN_STATUS_FAULT:
                    fan_info[i].fan_motor[motor_index].motor_status =  (speed < bd->fan_min_speed) ? FAN_STATUS_FAULT : FAN_STATUS_NORMAL;
                    break;
                case FAN_STATUS_ABSENT:
                case FAN_STATUS_NORMAL:
                case FAN_STATUS_UNKNOWN:
                    fan_info[i].fan_motor[motor_index].motor_status = temp_status;
                    break;
                default:
                    fan_info[i].fan_motor[motor_index].motor_status = FAN_STATUS_UNKNOWN;
                }                                                                    
            }
        }
        
        if (fan_info[i].status == FAN_STATUS_NORMAL) {
            bsp_cpld_set_fan_led_green(CODE_LED_ON, i);
            bsp_cpld_set_fan_led_red(CODE_LED_OFF, i);
        } else {
            bsp_cpld_set_fan_led_green(CODE_LED_OFF, i);
            bsp_cpld_set_fan_led_red(CODE_LED_ON, i);
        }

        h3c_adjust_fan_speed();        
    }  

    return;
}

static void bsp_fan_work_handler (struct work_struct *work)
{
    if (!fan_monitor_task_sleep) {
        bsp_fan_monitor_proc ();
    }
    
    mod_timer (&timer, jiffies + 600 * HZ / 1000);
    return;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void bsp_fan_timer_proc (unsigned int data)
#else
static void bsp_fan_timer_proc (struct timer_list *timer)
#endif
{
    if (fan_workqueue) {
        if (!queue_work (fan_workqueue, &work)) {
            DBG_ECHO(DEBUG_ERR, "create h3c sys fan work queue error.\n");
         }
    }

    return;
}

static void bsp_fan_timer_deinit (void)
{
    del_timer (&timer);
    return;
}

static void bsp_fan_workqueue_deinit (void)
{
    if (fan_workqueue) {
        flush_workqueue (fan_workqueue);
        destroy_workqueue (fan_workqueue);
        fan_workqueue = NULL;        
    }
    
    return;
}

static int bsp_fan_workqueue_init (void)
{
    fan_workqueue = create_workqueue ("fan_monitor");
    if (!fan_workqueue) {
        DBG_ECHO(DEBUG_ERR, "create h3c sys fan work queue faild.\n");
        return -ENOMEM;
    }

    INIT_WORK (&work, bsp_fan_work_handler);
    return 0;
}

static void bsp_fan_timer_init (void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
    init_timer (&timer);
    timer.function = bsp_fan_timer_proc;
#else
    timer_setup(&timer, bsp_fan_timer_proc, 0);
#endif

    timer.expires = jiffies + 600 * HZ / 1000;
    add_timer(&timer);
    return;
}

static int __init fan_init(void)
{
    int ret = ERROR_SUCCESS;
    int i = 0, j = 0;
	char temp_str[128] = {0};
    int bsp_fan_num;
    board_static_data * bd = bsp_get_board_data();
    
    DBG_ECHO(DEBUG_INFO, "fan module init started\n"); 
	bsp_fan_get_fan_number(&bsp_fan_num);
    memset(fan_info, 0, sizeof(fan_info));
    
    kobj_fan_root = kobject_create_and_add("fan", kobj_switch);
	if (!kobj_fan_root)
	{
		DBG_ECHO(DEBUG_ERR, "kobj_fan_root create falled!\n");		  
		ret = -ENOMEM;	   
		goto exit;	 
	}

    if (sysfs_create_group(kobj_fan_root, &def_attr_group) != 0) {
        DBG_ECHO(DEBUG_INFO, "create fan default attr faild.\n");
        ret = -ENOSYS;
        goto exit;
    }

    CHECK_CREATE_SYSFS_FILE(kobj_fan_root, sensor_dev_attr_num_fans.dev_attr, ret);

    for (i = 0; i < bsp_fan_num; i++) {
        sprintf(temp_str, "fan%d", (i + 1));
		fan_info[i].fan_index = i;
		
		ret = kobject_init_and_add(&fan_info[i].kobj_fan, &static_kobj_ktype ,kobj_fan_root, temp_str);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "kobj_fan %d create falled!\n", (i + 1));

        ret = sysfs_create_group(&(fan_info[i].kobj_fan),&fan_custom_attribute_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "fan %d custom attribute group create failed!\n", i + 1);

        for (j = 0; j < bd->motors_per_fan; j++) {
            sprintf(temp_str, "motor%d", j);
            fan_info[i].fan_motor[j].motor_index = j;
            fan_info[i].fan_motor[j].fan_index = i;
            
            ret = kobject_init_and_add(&(fan_info[i].fan_motor[j].kobj_motor), &static_kobj_ktype , &(fan_info[i].kobj_fan), temp_str);         
            CHECK_IF_ERROR_GOTO_EXIT(ret, "motor%d kobject int and add failed!", j);
            
            ret = sysfs_create_group(&(fan_info[i].fan_motor[j].kobj_motor), &fan_custom_motor_attribute_group);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "motor%d attribute group created failed!", j);
        }

        fan_info[i].parent_hwmon = hwmon_device_register(NULL);
        CHECK_IF_NULL_GOTO_EXIT(ret, fan_info[i].parent_hwmon, "fan %d hwmon register failed", i + 1);
        
	    ret = sysfs_create_group(&(fan_info[i].parent_hwmon->kobj),&fan_attribute_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "fan %d attribute create failed!\n", i + 1);
    }

    kobj_fan_debug = kobject_create_and_add("fan", kobj_debug);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_fan_debug, "fan debug kobject created failed!");
    
    ret = sysfs_create_group(kobj_fan_debug, &fan_debug_attribute_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "fan debug attribute group created failed!");

    bsp_fan_timer_init ();
    ret = bsp_fan_workqueue_init();
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create fan monitor task return %d is error.\n", ret);
    	
exit:
    if (ret != 0) {
        DBG_ECHO(DEBUG_ERR, "fan module init failed!\n");
        release_all_fan_kobj();
    } else {
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
    
	for (i = 0; i < bsp_fan_num; i++) {
        for (j = 0; j < bd->motors_per_fan; j++) {
            if (fan_info[i].fan_motor[j].kobj_motor.state_initialized) {
                kobject_put(&(fan_info[i].fan_motor[j].kobj_motor));
            }
        }
        
        if (fan_info[i].kobj_fan.state_initialized) {
		    kobject_put(&(fan_info[i].kobj_fan));
        }
       
        if (fan_info[i].parent_hwmon != NULL) {
            hwmon_device_unregister(fan_info[i].parent_hwmon); 
            fan_info[i].parent_hwmon = NULL;
        }
	}

    bsp_fan_workqueue_deinit ();
    bsp_fan_timer_deinit ();
    
    if (kobj_fan_debug != NULL) {
        kobject_put(kobj_fan_debug);
        kobj_fan_debug = NULL;
    }

    if (kobj_fan_root != NULL) {
        sysfs_remove_group (kobj_fan_root, &def_attr_group);
        kobject_put(kobj_fan_root); 
        kobj_fan_root = NULL;
     }
}

static void __exit fan_exit(void)
{
	release_all_fan_kobj();
    INIT_PRINT("module fan uninstalled !\n");
}

module_init(fan_init);
module_exit(fan_exit);
EXPORT_SYMBOL(bsp_fan_get_status);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system fan driver");
MODULE_LICENSE("Dual BSD/GPL");
