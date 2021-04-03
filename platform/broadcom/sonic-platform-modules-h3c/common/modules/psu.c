#include "bsp_base.h"
#include "static_ktype.h"

void release_all_psu_resource(void);

static int loglevel =  DEBUG_INFO | DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt, ##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)
#define ATTR_NAME_MAX_LEN    48
#define MODULE_NAME "psu"
#define PSU_I2C_CONTINUOUS_FAIL_COUNT 30

enum psu_alarm_type {
    PSU_NO_ALARM        = 0x00,
    PSU_NO_POWER        = 0x01,
    PSU_TERMAL_ERROR    = 0x02,
    PSU_FAN_ERROR       = 0x04,
    PSU_VOL_ERROR       = 0x08,
    PSU_VIN_ERROR       = 0x10,
    PSUERROR_BUTT
};

enum command_type {
    VOLTAGE_OUT,
    CURRENT_OUT,
    VOLTAGE_IN,
    CURRENT_IN,
    HW_VERSION_CMD,
    FW_VERSION_CMD,
    VOLTAGE_IN_TYPE_CMD,
    STATUS_TEMPERATURE,
    INPUT_TEMP,
    FAN_SPEED,
    SN_NUMBER,
    POWER_IN,
    POWER_OUT,
    PRODUCT_NAME_CMD,
};

enum psu_hwmon_sysfs_attributes {
    SENSOR_NAME = 0,
    SENSOR_NAME_BUTT = SENSOR_NAME + MAX_PSU_NUM - 1,
    IN1_MAX,
    IN_MAX_BUTT = IN1_MAX + MAX_PSU_NUM - 1,
    IN1_MIN,
    IN_MIN_BUTT = IN1_MIN + MAX_PSU_NUM - 1,
    IN1_INPUT,
    IN_INPUT_BUTT = IN1_INPUT + MAX_PSU_NUM - 1,
    IN1_LABEL,
    IN_LABEL_BUTT = IN1_LABEL + MAX_PSU_NUM - 1,
    CURR1_MAX,
    CURR_MAX_BUTT = CURR1_MAX + MAX_PSU_NUM - 1,
    CURR1_INPUT,
    CURR_INPUT_BUTT = CURR1_INPUT + MAX_PSU_NUM - 1,
    CURR1_LABEL,
    CURR_LABEL_BUTT = CURR1_LABEL + MAX_PSU_NUM - 1,
    POWER1_MAX,
    POWER_MAX_BUTT = POWER1_MAX + MAX_PSU_NUM - 1,
    POWER1_INPUT,
    POWER_INPUT_BUTT = POWER1_INPUT + MAX_PSU_NUM - 1,
    POWER1_LABEL,
    POWER_LABEL_BUTT = POWER1_LABEL + MAX_PSU_NUM - 1,
    PSU_ATTRIBUTE_BUTT
};

enum psu_customer_sysfs_attributes {
    NUM_PSUS,
    PRODUCT_NAME,
    SN,
    HW_VERSION,
    IN_VOL_TYPE,
    FW_VERSION,
    IN_CURR,
    IN_VOL,
    OUT_CURR,
    OUT_VOL,
    STATUS,
    LED_STATUS,
    IN_POWER,
    OUT_POWER,
    DIS_PSU_MON,
    ALARM,
    NUM_FANS,
    NUM_TEMP_SENSORS,
    ALARM_THRESHOLD_CURR,
    ALARM_THRESHOLD_VOL,
    MAX_OUTPUT_POWER,
    PRODUCTION_DATE,
    VENDOR
};

enum psu_temp_customer_sysfs_attributes {
    TEMP_INPUT,
    TEMPALIAS,
    TEMPTYPE,
    TEMPMAX,
    TEMPMAX_HYST,
    TEMPMIN
};

enum psu_fan_customer_sysfs_attributes {
    PSU_FAN_SPEED
};

typedef struct {
    int fan_index;
    int psu_index;
    int fan_speed;
    struct kobject fan_kobj;
}psu_fan_info_st;

typedef struct {
    int tempinput; 
    int temp_index;
    int psu_index;
    struct kobject temp_kobj;
}psu_sensor_info_st;

typedef struct {
    int status;
    int vout;
    int iout;
    int vin;
    int iin;
    int pin;
    int pout;     
    int tempstatus;
    int vin_type;
    u8 alarm;
    char sn_number[PSU_MAX_SN_LEN];
    char product_name[PSU_MAX_PRODUCT_NAME_LEN];
    char hw_version[PSU_MAX_HW_VERSION_LEN+1];
    char fw_version[PSU_MAX_FW_VERSION_LEN+1];
    psu_fan_info_st fan_info[PSU_MAX_FAN_NUM_MAX];
    psu_sensor_info_st sensor_info[PSU_MAX_FAN_NUM_MAX];
    struct device *parent_hwmon;
    struct kobject *customer_kobj;
} psu_info_st; 

static psu_info_st psu_info[MAX_PSU_NUM] = {{0}};
static struct sensor_device_attribute psu_hwmon_dev_attr[PSU_ATTRIBUTE_BUTT]; 
static struct attribute *psu_hwmon_attributes[MAX_PSU_NUM][PSU_ATTRIBUTE_BUTT / MAX_PSU_NUM + 1] = {{0}};
static struct attribute_group psu_hwmon_attribute_group[MAX_PSU_NUM] = {{0}};                               
static char psu_hwmon_attr_name[PSU_ATTRIBUTE_BUTT][ATTR_NAME_MAX_LEN] = {{0}};                                         
int psu_monitor_task_sleep = 0;
char *psu_status_string[] = {"Absent", "Normal", "Fault", "Unknown"};
static struct kobject *kobj_psu_root = NULL;
static struct kobject *kobj_psu_debug = NULL;
static struct kobject *kobj_psu_sub[MAX_PSU_NUM] = {NULL};
static struct timer_list timer;
static struct work_struct work;
static struct workqueue_struct *psu_workqueue = NULL;

char *bsp_psu_get_status_string(int status)
{
    if (status >= PSU_STATUS_ABSENT && status <= PSU_STATUS_UNKNOWN) {
        return psu_status_string[status];
    } else {
        DBG_ECHO(DEBUG_ERR, "psu unknown status %d", status);
        return psu_status_string[PSU_STATUS_UNKNOWN];
    }
}

int bsp_psu_get_status(int psu_index)
{
    if ((0 > psu_index) || 
        (psu_index >= MAX_PSU_NUM)) 
        return PSU_STATUS_UNKNOWN;
    else
        return psu_info[psu_index].status;
}

u32 calc_line(u8 *CalcNum)
{
    u32 power = 0;
    u32 base = 0;
    u32 result = 0;
    
    u8 flag = (CalcNum[1] >> 7) & (0x1);
    if (flag == 1) {
        power = (~(CalcNum[1] >> 3) & (0x1f)) + 1;
    } else {
        power = (CalcNum[1] >> 3) & 0x1f;
    }
    
    base = ((((u32)CalcNum[1] << 8 ) & 0x300) | (u32)CalcNum[0]) * 100; 
    result = flag == 1 ? base >> power : base << power;
    return result;
}

u32 calc_voltage(u8 *CalcNum)
{
    return ((CalcNum[0] + (((u32)CalcNum[1]) << 8)) * 1000 / 512);
}

u32 calc_current_integer(u8 *CalcNum)
{
    u16 value = CalcNum[0] + ((u16)CalcNum[1] << 8);
    s8 cExp = 0;
    s16 sMantissa = 0;
    u8 ucTempExp = 0;
    u16 usTempMant = 0;

    ucTempExp = value >> 11;
    usTempMant = value & 0x7ff;
    cExp = (char) ucTempExp;
    sMantissa = (short)usTempMant;
    
    if (usTempMant & 0x400) {
        usTempMant |= 0xf800;
        sMantissa = (short)usTempMant;
        sMantissa = -sMantissa;
    }
    
    if (ucTempExp & 0x10) {
        ucTempExp |= 0xE0;
        cExp = (char)ucTempExp;
        cExp = -cExp;
        value = (u16)sMantissa >> (u8)cExp;
    } else {
        value = (u16)((u32)(u16)sMantissa << (u32)(u8)cExp);
    }
    
    return (u32)value;    
}

u32 calc_current_remain (u8 *CalcNum)
{
    u16 value = CalcNum[0] + ((u16)CalcNum[1] << 8);
    s8 cExp = 0;
    s16 sMantissa = 0;
    u8 ucTempExp = 0;
    u16 usTempMant = 0;

    ucTempExp = value >> 11;
    usTempMant = value & 0x7ff;
    cExp = (char) ucTempExp;
    sMantissa = (short)usTempMant;
    
    if (usTempMant & 0x400) {
        usTempMant |= 0xf800;
        sMantissa = (short)usTempMant;
        sMantissa = -sMantissa;
    }
    
    if (ucTempExp & 0x10) {
        ucTempExp |= 0xE0;
        cExp = (char)ucTempExp;
        cExp = -cExp;
        value = ((u16)sMantissa * 100 >> (u8)cExp) % 100;
    } else {
        value = 0;
    }
    
    return (u32)value;    
}

int bsp_psu_reset_i2c (int psu_index)
{
    return bsp_reset_smbus_slave(I2C_DEV_PSU + psu_index);;
}

int _bsp_psu_get_value(int command, int psu_index, u32 *value)
{
    int ret = ERROR_SUCCESS;
    u8 temp_value[PSU_MAX_INFO_LEN] = {0};
    board_static_data *bd = bsp_get_board_data();
    int i = 0;
    
    ret = lock_i2c_path(I2C_DEV_PSU + psu_index);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "lock psu %d i2c path failed", psu_index);
    
    switch(command) {
    case CURRENT_IN:
        if (bd->psu_type == PSU_TYPE_650W) {
            *value = 0;
        } else {
            ret = bsp_i2c_power1600W_read_current_in(bd->i2c_addr_psu_pmbus[psu_index], PW1600_IIN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_current_integer(temp_value) * 100 + calc_current_remain(temp_value);
        }
        break;
    case CURRENT_OUT:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_current(bd->i2c_addr_psu_pmbus[psu_index], PW650_IOUT_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_current(bd->i2c_addr_psu_pmbus[psu_index], PW1600_IOUT_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_current_integer(temp_value) * 100 + calc_current_remain(temp_value);

        }
        break;        
    case VOLTAGE_OUT:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_voltage(bd->i2c_addr_psu_pmbus[psu_index], PW650_VOUT_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
            
        } else {
            ret = bsp_i2c_power1600W_read_voltage(bd->i2c_addr_psu_pmbus[psu_index], PW1600_VOUT_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_voltage(temp_value) / 10;
        }
        break;
    case VOLTAGE_IN:
        if (bd->psu_type == PSU_TYPE_650W) {
            *value = 0;    
        } else {
            ret = bsp_i2c_power1600W_read_voltage_in(bd->i2c_addr_psu_pmbus[psu_index], PW1600_VIN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        }
        break;
    case VOLTAGE_IN_TYPE_CMD:
        if (bd->psu_type == PSU_TYPE_650W) {
            *value = PSU_IN_VOL_TYPE_NOT_SUPPORT;
        } else {
            ret = bsp_i2c_power1600W_read_voltage_in_type(bd->i2c_addr_psu_pmbus[psu_index], PW1600_VIN_TYPE_BYTE_COUNT, temp_value);
            *value = temp_value[0];
        }
        break;
    case STATUS_TEMPERATURE:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_temperstatus(bd->i2c_addr_psu_pmbus[psu_index], PW650_STATEMPURE_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = temp_value[0];
        } else {
            ret = bsp_i2c_power1600W_read_temperstatus(bd->i2c_addr_psu_pmbus[psu_index], PW1600_STATEMPURE_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = temp_value[0];
        }
        break;
    case INPUT_TEMP:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_temper(bd->i2c_addr_psu_pmbus[psu_index], PW650_TEMPURE_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_temper(bd->i2c_addr_psu_pmbus[psu_index], PW1600_TEMPURE_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        }
        break;
    case FAN_SPEED:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_fan_speed(bd->i2c_addr_psu_pmbus[psu_index], PW650_FAN_BYTE_COUNT, 1, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);

            if (temp_value[0] == 0 && temp_value[1] == 0) {
                ret = bsp_i2c_power650W_read_fan_speed(bd->i2c_addr_psu_pmbus[psu_index], PW650_FAN_BYTE_COUNT, 0, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for read_fan_speed! command=%d", command);
            }
            
            *value = calc_line(temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_fan_speed(bd->i2c_addr_psu_pmbus[psu_index], PW1600_FAN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for read_fan_speed! command=%d", command);
            *value = calc_line(temp_value);
        }
        break;
    case SN_NUMBER:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_SN(bd->i2c_addr_psu[psu_index], PW650_SN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            sprintf((u8 *)value, "%s", temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_SN(bd->i2c_addr_psu[psu_index], PW1600_SN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            sprintf((u8 *)value, "%s", temp_value);
        }
        break;
    case PRODUCT_NAME_CMD:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_SN(bd->i2c_addr_psu[psu_index], PW650_SN_BYTE_COUNT, temp_value);
            if (strstr(temp_value, "0231A0QM") != NULL) {
                sprintf((u8 *)value, "%s", "LSVM1AC650");
            }
            else {
                sprintf((u8 *)value, "%s", "Unknown");
            }
        } else {
            ret = bsp_i2c_power1600W_read_SN(bd->i2c_addr_psu[psu_index], PW1600_SN_BYTE_COUNT, temp_value);
            
            if (strstr(temp_value, "0231ABV7") != NULL) {
                sprintf((u8 *)value, "%s", "PSR1600B-12A-B");
            } else {
                sprintf((u8 *)value, "%s", "Unknown");
            }
        }
        break;        
    case HW_VERSION_CMD:        
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_hw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_HW_VERSION_LEN, temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_hw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_HW_VERSION_LEN, temp_value);
        }
        
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
        
        if (temp_value[0] < PSU_MAX_HW_VERSION_LEN - 1) {
            temp_value[temp_value[0] + 1] = '\0';
        } else {
            temp_value[PSU_MAX_HW_VERSION_LEN] = '\0';
        }
        
        for (i = 0; i < PSU_MAX_HW_VERSION_LEN; i++) {
            if (temp_value[i] == (u8)0xff)
                temp_value[i] = '\0';
        }
        
        sprintf((u8 *)value, "%s", temp_value);
        break;
    case FW_VERSION_CMD:        
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_fw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_FW_VERSION_LEN, temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_hw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_FW_VERSION_LEN, temp_value);
        }
        
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
        
        if (temp_value[0] < PSU_MAX_FW_VERSION_LEN - 1) {
            temp_value[temp_value[0] + 1] = '\0';
        } else {
            temp_value[PSU_MAX_FW_VERSION_LEN] = '\0';
        }
        
        for (i = 0; i < PSU_MAX_FW_VERSION_LEN; i++) {
            if (temp_value[i] == (u8)0xff)
                temp_value[i] = '\0';
        }
        
        sprintf((u8 *)value, "%s", temp_value);
        break;
    case POWER_IN:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_powerin(bd->i2c_addr_psu_pmbus[psu_index], PW650_PIN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_powerin(bd->i2c_addr_psu_pmbus[psu_index], PW1600_PIN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        }
        break;
    case POWER_OUT:
        if (bd->psu_type == PSU_TYPE_650W) {
            ret = bsp_i2c_power650W_read_powerout(bd->i2c_addr_psu_pmbus[psu_index], PW650_FAN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        } else {
            ret = bsp_i2c_power1600W_read_powerout(bd->i2c_addr_psu_pmbus[psu_index], PW1600_FAN_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = calc_line(temp_value);
        }
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "unknown command %d", command);
        ret = ERROR_FAILED;
        break;
    }
    
exit:
    unlock_i2c_path();   
    return ret;
}

int bsp_psu_get_value (int command, int psu_index, u32 *value)
{
    u8 absent = 0;
        
    if (bsp_cpld_get_psu_absent(&absent, psu_index) != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "psu %d get absent failed!", psu_index + 1);
        return ERROR_FAILED;
    }
    
    if (1 == absent) {
        *value = 0;
        return ERROR_FAILED;
    }
    
    return _bsp_psu_get_value(command, psu_index, value);
}

int bsp_sysfs_psu_get_index_from_kobj (struct kobject *kobj)
{
    int i;
    int psu_num = bsp_get_board_data()->psu_num; 
    
    for (i = 0; i < psu_num; i++) {
        if (psu_info[i].customer_kobj == kobj)
            return i;
    }
    
    DBG_ECHO(DEBUG_ERR, "Not found matched psu index, kobj=%p", kobj);
    return -1;
}

char *math_int_divide (int dividend, int divisor, int decimal_width, char * result)
{
    int i = 0;
    int mod = dividend % divisor;
    int integer = 0;
    int decimal = 0;
    int len = 0;

    integer = dividend / divisor;
    decimal_width = decimal_width > 10 ? 10 : decimal_width;
    len = sprintf(result, "%d%s", integer, decimal_width == 0 ? "" : ".");

    for (i = 0; i < decimal_width; i++, decimal *= 10) {
        decimal = mod * 10 / divisor;
        len += sprintf(result + len, "%d", decimal);
        mod = mod * 10 % divisor;
    }
    
    return result;
}

int parameter_int_to_float_deal(int value,int *integer_value, int *float_value)
{
    int temp_value =0;
    *integer_value = value / 100;
    temp_value = value % 100;
    
    if(temp_value > 9) {
        *float_value = temp_value /10;
    } else {
        *float_value = temp_value;
    }
    
    return 0;
}

static ssize_t bsp_sysfs_psu_hwmon_get_attr(struct device *dev, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int psu_index = 0;

    if ((attr->index >= SENSOR_NAME) && 
        (attr->index <= SENSOR_NAME_BUTT)) {
        psu_index = attr->index - SENSOR_NAME;   
        return sprintf(buf, "Power%d %s\n", psu_index + 1, bsp_psu_get_status_string(psu_info[psu_index].status));  
    } else if (attr->index >= IN1_LABEL && attr->index <= IN_LABEL_BUTT) {
        return sprintf(buf, "Voltage %d\n", attr->index - IN1_LABEL + 1);    
    } else if (attr->index >= CURR1_LABEL && attr->index <= CURR_LABEL_BUTT) {
        return sprintf(buf, "Current %d\n", attr->index - CURR1_LABEL + 1);
    } else if (attr->index >= POWER1_LABEL && attr->index <= POWER_LABEL_BUTT) {
        return sprintf(buf, "Power %d\n", attr->index - CURR1_LABEL + 1);
    } else if (attr->index >= IN1_INPUT && attr->index <= IN_INPUT_BUTT) {
        psu_index = attr->index - IN1_INPUT;
        return sprintf(buf, "%d\n", psu_info[psu_index].vout * 10);
    } else if (attr->index >= CURR1_INPUT && attr->index <= CURR_INPUT_BUTT) {
        psu_index = attr->index - CURR1_INPUT;
        return sprintf(buf, "%d\n", psu_info[psu_index].iout * 10);
    } else if (attr->index >= POWER1_INPUT && attr->index <= POWER_INPUT_BUTT) {
        psu_index = attr->index - POWER1_INPUT;
        return sprintf(buf, "%d\n", psu_info[psu_index].pout * 10000);
    } else {
        return sprintf(buf, "0\n");
    }
    
    return 0;
}

static ssize_t bsp_sysfs_psu_debug_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    ssize_t index = -EIO;
    
    switch(attr->index) {
    case DIS_PSU_MON:
        index = sprintf(buf, "%d\n", psu_monitor_task_sleep);
        break;
    default:
        index = sprintf(buf, "Not support attribute %d\n", attr->index);
        break;
    }

    return index;
}

static ssize_t bsp_sysfs_psu_customer_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int index = 0;
    int psu_index = 0;
    int int_value =0;
    int float_value =0;
    int rv = 0;
    u8 alarm = PSU_NO_ALARM;
    board_static_data *bd = bsp_get_board_data();

    if (attr->index != NUM_PSUS) {
        psu_index = bsp_sysfs_psu_get_index_from_kobj ((struct kobject *)kobj);
    }
    
    switch(attr->index) {
    case NUM_PSUS:
        index = sprintf(buf, "%d\n", (int)bd->psu_num);
        break;
    case PRODUCT_NAME:  
        rv = bsp_psu_get_value(PRODUCT_NAME_CMD, psu_index, (u32 *)(psu_info[psu_index].product_name));
        if (rv) {
            index = sprintf(buf, "%s\n", "Unknown\n");
            DBG_ECHO (DEBUG_DBG, "psu %d get PRODUCT_NAME failed!\n", psu_index + 1);
        } else {
            index = sprintf(buf, "%s\n", psu_info[psu_index].product_name);
        }
        break;
    case SN:
        rv = bsp_psu_get_value (SN_NUMBER, psu_index, (u32 *)(psu_info[psu_index].sn_number));
        if (rv) {
            index = sprintf(buf, "%s\n", "Unknown\n");
            DBG_ECHO(DEBUG_DBG, "psu %d get sn failed!\n", psu_index + 1);
        } else {
            index = sprintf(buf, "%s\n", psu_info[psu_index].sn_number);
        }
        break;
    case HW_VERSION:
        rv = bsp_psu_get_value(HW_VERSION_CMD, psu_index, (u32 *)(psu_info[psu_index].hw_version));
        if (rv) {
            index = sprintf(buf, "%s\n", "Unknown\n");
            DBG_ECHO(DEBUG_DBG, "psu %d get hw version failed!\n", psu_index + 1);
        } else {       
            index = sprintf(buf, "%s\n", psu_info[psu_index].hw_version);
        }
        break;
    case FW_VERSION:
        rv = bsp_psu_get_value(FW_VERSION_CMD, psu_index, (u32 *)(psu_info[psu_index].fw_version));
        if (rv) {
            index = sprintf(buf, "%s\n", "0\n");
            DBG_ECHO(DEBUG_DBG, "psu %d get fw version failed!\n", psu_index + 1);
        } else {
            index = sprintf(buf, "%s\n", psu_info[psu_index].fw_version);
        }
        break;
    case IN_CURR:   
        if (PSU_TYPE_650W != bd->psu_type) {
            rv = bsp_psu_get_value (CURRENT_IN, psu_index, &(psu_info[psu_index].iin));
            if (rv) {
                index = sprintf(buf, "%s\n", "0\n");
                DBG_ECHO(DEBUG_DBG, "psu %d get input current failed!", psu_index + 1);
                return index;
            }

            parameter_int_to_float_deal(psu_info[psu_index].iin,&int_value,&float_value);         
            index = sprintf(buf, "%d.%d\n", int_value,float_value);
        } else {
            index = sprintf(buf, "%s\n", "N/A");  
        }
        break;
    case IN_VOL:
        if (PSU_TYPE_650W != bd->psu_type) {
            rv = bsp_psu_get_value(VOLTAGE_IN, psu_index, &(psu_info[psu_index].vin));
            if (rv) {
                index = sprintf(buf, "%s\n", "0\n");
                DBG_ECHO(DEBUG_DBG, "psu %d get input voltage failed!", psu_index + 1);
            }
        } else {        
            index = sprintf(buf, "%s\n", "N/A");
        }
        break;
    case IN_VOL_TYPE:
        rv = bsp_psu_get_value(VOLTAGE_IN_TYPE_CMD, psu_index, &(psu_info[psu_index].vin_type));
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "psu %d get fw version failed!\n", psu_index + 1);
            index = sprintf(buf, "%s", "Unknown\n");
        } else {
            switch(psu_info[psu_index].vin_type) {
            case PSU_IN_VOL_TYPE_NO_INPUT:
                index = sprintf(buf, "No Input\n");
                break;
            case PSU_IN_VOL_TYPE_AC:
                index = sprintf(buf, "AC\n");
                break;
            case PSU_IN_VOL_TYPE_HVDC:
                index = sprintf(buf, "DC\n");
                break;
            case PSU_IN_VOL_TYPE_NOT_SUPPORT:
                index = sprintf(buf, "No Support\n");
                break;
            default: 
                index = sprintf(buf, "Unknown vin type %d\n", psu_info[psu_index].vin_type);
                break;
            }
        }
        break;
    case OUT_CURR:     
        index = sprintf(buf, "%d\n", psu_info[psu_index].iout);
        break;
    case OUT_VOL:         
        index = sprintf(buf, "%d\n", psu_info[psu_index].vout);
        break;
    case LED_STATUS:
        if(psu_info[psu_index].status == PSU_STATUS_ABSENT) {
            index = sprintf(buf, "%d\n", LED_COLOR_DARK);
        } else if(psu_info[psu_index].status == PSU_STATUS_FAULT) {
            index = sprintf(buf, "%d\n", LED_COLOR_RED);
        } else if(psu_info[psu_index].status == PSU_STATUS_NORMAL) {
            index = sprintf(buf, "%d\n", LED_COLOR_GREEN);      
        }
        break;
    case IN_POWER:
        rv = bsp_psu_get_value(POWER_IN, psu_index, &(psu_info[psu_index].pin));
        if (rv)
            DBG_ECHO(DEBUG_DBG, "psu %d get power in failed!", psu_index + 1);
                 
        index = sprintf(buf, "%d\n", psu_info[psu_index].pin);
        break;
    case OUT_POWER:
        rv = bsp_psu_get_value(POWER_OUT, psu_index, &(psu_info[psu_index].pout));
        if (rv)
            DBG_ECHO(DEBUG_DBG, "psu %d get power out failed!", psu_index + 1);
                  
        index = sprintf(buf, "%d\n", psu_info[psu_index].pout);
        break;
    case STATUS:
        index = sprintf(buf, "%d\n", psu_info[psu_index].status);
        break;
    case ALARM:
        if (PSU_STATUS_NORMAL != psu_info[psu_index].status) {
            alarm |= PSU_NO_POWER;
        } else {
            if (bd->psu_type != PSU_TYPE_650W) {
                if (((psu_info[psu_index].vin / 100) < PSU_ALARM_VIN_MIN) || 
                    ((psu_info[psu_index].vin / 100) >= PSU_ALARM_VIN_MAX))
                    alarm |= PSU_VIN_ERROR;
            }

            if ((psu_info[psu_index].vout / 100) >= PSU_ALARM_VOUT_MAX)
                alarm |= PSU_VOL_ERROR;

            if ((psu_info[psu_index].fan_info[0].fan_speed / 100) < PSU_ALARM_FAN_MIN)
                alarm |= PSU_FAN_ERROR;

            if (((psu_info[psu_index].sensor_info[0].tempinput / 100) <= PSU_ALARM_TEMP_LOWER) || 
                ((psu_info[psu_index].sensor_info[0].tempinput /100) >  PSU_ALARM_TEMP_HIGH))
                alarm |= PSU_TERMAL_ERROR;
        }
        
        index = sprintf(buf, "%d\n", alarm);
        break;
    case NUM_TEMP_SENSORS:
        index = sprintf(buf, "%d\n", bd->psu_sensor_num);
        break;
    case NUM_FANS:
        index = sprintf(buf, "%d\n", bd->psu_fan_num);
        break;
    case ALARM_THRESHOLD_CURR:
        if (bd->psu_type == PSU_TYPE_650W) {
            index += sprintf(buf + index, "  in_curr  : %s\n", "N/A");
            index += sprintf(buf + index, "  out_curr : %s\n", "> 60A");
        } else {
            index = sprintf(buf, "%s\n", "N/A");
        }
        break;
    case ALARM_THRESHOLD_VOL:
        if (bd->psu_type == PSU_TYPE_650W) {
            index += sprintf(buf + index, "  in_vol  : %s\n", "N/A");
            index += sprintf(buf + index, "  out_vol : %s\n", "> 15V");
        } else {
            index = sprintf(buf, "%s\n", "N/A");
        }
        break;
    case MAX_OUTPUT_POWER:
        index = sprintf(buf, "%d\n", bd->psu_max_output_power);
        break;
    case PRODUCTION_DATE:
        if (bd->psu_type == PSU_TYPE_650W)
            index = sprintf(buf, "%s\n", "N/A");
        break;
    case VENDOR:
        index = sprintf(buf, "%s\n", "H3C");
        break;
    default:
        index = sprintf(buf, "Not support attribute %d\n", attr->index);
        break;
    }
    
    return index;
}

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static ssize_t bsp_sysfs_psu_temp_customer_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    ssize_t index = 0;
    int rv = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    psu_sensor_info_st *sensor = container_of((struct kobject *)kobj, psu_sensor_info_st, temp_kobj);
    
    switch(attr->index) {
     case TEMP_INPUT:
        rv = bsp_psu_get_value(INPUT_TEMP, sensor->psu_index, &(psu_info[sensor->psu_index].sensor_info[sensor->temp_index].tempinput));
        if (rv) 
            DBG_ECHO(DEBUG_DBG, "psu %d get current temperature  failed!", sensor->psu_index + 1);
                 
        index = sprintf(buf, "%d\n", psu_info[sensor->psu_index].sensor_info[sensor->temp_index].tempinput);
        break;
    case TEMPALIAS:
        index = sprintf(buf, "psu_inner\n");
        break;
    case TEMPTYPE:
        index = sprintf(buf, "psu sensor%d\n", attr->index);
        break;
    case TEMPMAX:
        index = sprintf(buf, "%d\n", 80);
        break;
    case TEMPMAX_HYST:
        index = sprintf(buf, "%d\n", 5);
        break;
    case TEMPMIN:
        index = sprintf(buf, "%d\n", 0);
        break;
    
default:
        index = sprintf(buf, "Not support attribute %d\n", attr->index);
        break;
    }
    
    return index;
}

static ssize_t bsp_sysfs_fan_customer_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    ssize_t index = 0;
    int rv = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    psu_fan_info_st *fan = container_of((struct kobject *)kobj, psu_fan_info_st, fan_kobj);
    
    switch(attr->index) {
    case PSU_FAN_SPEED:
        rv = bsp_psu_get_value(FAN_SPEED, fan->psu_index, &(psu_info[fan->psu_index].fan_info[fan->fan_index].fan_speed));
        if (rv)
            DBG_ECHO(DEBUG_DBG, "psu %d get fan speed failed!", fan->psu_index);

        index = sprintf(buf, "%d\n", psu_info[fan->psu_index].fan_info[fan->fan_index].fan_speed);
        break;
    default:
        index = sprintf(buf, "Not support attribute %d\n", attr->index);
    }
    
    return index;
}
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static void bsp_psu_monitor_proc (void)
{
    int i = 0;
    int j = 0;
    u8 absent = 0;
    u8 good = 0;
    int temp_status = 0;
    unsigned int continuous_i2c_failed_count[MAX_PSU_NUM] = {0};
    board_static_data *bd = bsp_get_board_data();
    
    for (i = 0; i < bd->psu_num; i++) {
        if(bsp_cpld_get_psu_absent(&absent, i) != ERROR_SUCCESS) {
            DBG_ECHO(DEBUG_ERR, "psu %d get absent failed!", i + 1);
            continue;
        }
        
        if(bsp_cpld_get_psu_good(&good, i) != ERROR_SUCCESS) {
            DBG_ECHO(DEBUG_ERR, "psu %d get status failed!", i + 1);
            continue;
        }

        temp_status = (absent == 1) ? PSU_STATUS_ABSENT : 
                       ((good == 1) ? PSU_STATUS_NORMAL : PSU_STATUS_FAULT);

        if (temp_status != psu_info[i].status)
        {
            SYSLOG(LOG_LEVEL_INFO, "Power%d status changed from %s to %s", i + 1, bsp_psu_get_status_string(psu_info[i].status), bsp_psu_get_status_string(temp_status));
        }
        
        if (temp_status == PSU_STATUS_NORMAL)
        {
            if (bsp_psu_get_value(VOLTAGE_OUT, i, &(psu_info[i].vout)) != ERROR_SUCCESS) {
                DBG_ECHO(DEBUG_ERR, "psu %d get voltage out failed!", i + 1);
                psu_info[i].vout = 0;
                continuous_i2c_failed_count[i]++;
            } else {
                continuous_i2c_failed_count[i] = 0;
            }

            if (bsp_psu_get_value(CURRENT_OUT, i, &(psu_info[i].iout)) != ERROR_SUCCESS) {
                DBG_ECHO(DEBUG_ERR, "psu %d get current out failed!", i + 1);
                psu_info[i].iout = 0;
                continuous_i2c_failed_count[i]++;
            } else {
                continuous_i2c_failed_count[i] = 0;
            }

            for (j = 0; j < bd->psu_sensor_num; j++) {
                if (bsp_psu_get_value(INPUT_TEMP, i, &(psu_info[i].sensor_info[j].tempinput))) {
                    DBG_ECHO(DEBUG_ERR, "psu %d get sensor index %d temp failed!", i + 1, j); 
                    psu_info[i].sensor_info[j].tempinput = 0;
                    continuous_i2c_failed_count[i]++;
                } else {
                    continuous_i2c_failed_count[i] = 0; 
                }    
            }

            for (j = 0; j < bd->psu_fan_num; j++) {
                if (bsp_psu_get_value(FAN_SPEED, i, &(psu_info[i].fan_info[j].fan_speed))) {
                    DBG_ECHO(DEBUG_ERR, "psu %d get fan index %d temp failed!", i + 1, j); 
                    psu_info[i].fan_info[j].fan_speed = 0;
                    continuous_i2c_failed_count[i]++;
                } else {
                    continuous_i2c_failed_count[i] = 0; 
                }    
            }

            if (continuous_i2c_failed_count[i] >= PSU_I2C_CONTINUOUS_FAIL_COUNT) {
                do {
                    if (ERROR_SUCCESS != bsp_psu_reset_i2c(i)) {
                        DBG_ECHO(DEBUG_ERR, "psu %d reset i2c slave failed", i + 1);
                    }
                    else if (bsp_psu_get_value(VOLTAGE_OUT, i, &(psu_info[i].vout)) == ERROR_SUCCESS) {
                        continuous_i2c_failed_count[i] = 0;
                        DBG_ECHO(DEBUG_ERR, "psu %d i2c fault detected, reset i2c slave done and i2c recovered", i + 1);
                        break;
                    } else {
                        DBG_ECHO(DEBUG_ERR, "psu %d i2c fault detected, reset i2c slave but i2c not recovered", i + 1);
                    }

                    if (psu_info[i].status != PSU_STATUS_FAULT) {
                        DBG_ECHO(DEBUG_INFO, "psu %d changes to fault due to i2c failed %d times(>%d)!", i + 1, continuous_i2c_failed_count[i], PSU_I2C_CONTINUOUS_FAIL_COUNT);
                    }
                    
                    psu_info[i].status = PSU_STATUS_FAULT;     
                } while (0);
            } else {
                psu_info[i].status = PSU_STATUS_NORMAL;
            }
        } else {
            psu_info[i].iout = 0;
            psu_info[i].vout = 0;
            psu_info[i].status = temp_status;
        }    
    } 
    
    return;
}

static ssize_t bsp_sysfs_psu_debug_set_attr(struct device *kobject, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);

    switch(attr->index) {
    case DIS_PSU_MON:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
        } else {
            psu_monitor_task_sleep = (temp != 0);
        }
        break;
    default:
         DBG_ECHO(DEBUG_ERR, "unknown attribute index %d", attr->index);
    }

    return count;
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

    index += sprintf (buf + index, "%s", " Psu monitor control command:\n");
    index += sprintf (buf + index, "%s", "   0 ------ enable monitor\n");
    index += sprintf (buf + index, "%s", "   1 ------ disable monitor\n\n");
    index += sprintf (buf + index, "%s", " Disable/Enable monitor:\n");
    index += sprintf (buf + index, "%s", "   echo command > /sys/switch/debug/psu/disable_psu_mon\n\n");
    index += sprintf (buf + index, "%s", "   eg:\n\n");
    index += sprintf (buf + index, "%s", "      echo 1 > /sys/switch/debug/psu/disable_psu_mon\n\n");
    index += sprintf (buf + index, "%s", " Read psu eeprom:\n");
    index += sprintf (buf + index, "%s", "   you can run 'i2c_read.py' get help.\n\n");
    index += sprintf (buf + index, "%s", "   eg:\n\n");
    index += sprintf (buf + index, "%s", "      root@sonic:/sys/switch# i2c_read.py 0x1aa 0x50 0x01 0x0 0x80\n");
    index += sprintf (buf + index, "%s", "      Read dev id 0x1aa address 0x50 from 0x0 length 0x80\n");
    index += sprintf (buf + index, "%s", "      0000: a0 00 00 00 01 0b 00 f3  01 0a 19 c8 33 59 00 47  * ............3Y.G *\n");
    index += sprintf (buf + index, "%s", "      0010: 52 45 00 00 ca 50 53 52  36 35 30 42 2d 31 32 41  * RE...PSR650B-12A *\n");
    index += sprintf (buf + index, "%s", "      0020: 31 00 2d 32 36 35 31 42  00 00 00 00 00 d2 47 48  * 1.-2651B......GH *\n");
    index += sprintf (buf + index, "%s", "      0030: 4c 37 30 33 30 35 32 30  32 30 30 34 35 32 39 00  * L70305202004529. *\n");
    index += sprintf (buf + index, "%s", "      0040: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  * ................ *\n");
    index += sprintf (buf + index, "%s", "      0050: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  * ................ *\n");
    index += sprintf (buf + index, "%s", "      0060: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  * ................ *\n");
    index += sprintf (buf + index, "%s", "      0070: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  * ................ *\n\n");
    
    return index;
}

static ssize_t bsp_default_debug_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return bsp_default_debug_help (buf);
}

static ssize_t bsp_default_debug_store (struct kobject *kobj, struct kobj_attribute *attr, const char* buf, size_t count)
{
	return count;
}
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static SENSOR_DEVICE_ATTR(num_psus          , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, NUM_PSUS);
static SENSOR_DEVICE_ATTR(product_name      , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, PRODUCT_NAME);
static SENSOR_DEVICE_ATTR(sn                , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, SN);
static SENSOR_DEVICE_ATTR(hw_version        , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, HW_VERSION);
static SENSOR_DEVICE_ATTR(fw_version        , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, FW_VERSION);
static SENSOR_DEVICE_ATTR(in_curr           , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_CURR);
static SENSOR_DEVICE_ATTR(in_vol            , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_VOL);
static SENSOR_DEVICE_ATTR(out_curr          , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, OUT_CURR);
static SENSOR_DEVICE_ATTR(out_vol           , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, OUT_VOL);
static SENSOR_DEVICE_ATTR(status            , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, STATUS);
static SENSOR_DEVICE_ATTR(led_status        , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, LED_STATUS);
static SENSOR_DEVICE_ATTR(in_power          , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_POWER);
static SENSOR_DEVICE_ATTR(out_power         ,S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, OUT_POWER);
static SENSOR_DEVICE_ATTR(in_vol_type        , S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_VOL_TYPE);
static SENSOR_DEVICE_ATTR(disable_psu_mon , S_IRUGO|S_IWUSR, bsp_sysfs_psu_debug_get_attr, bsp_sysfs_psu_debug_set_attr, DIS_PSU_MON);

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static SENSOR_DEVICE_ATTR(alarm_threshold_curr, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, ALARM_THRESHOLD_CURR);
static SENSOR_DEVICE_ATTR(alarm_threshold_vol, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, ALARM_THRESHOLD_VOL);
static SENSOR_DEVICE_ATTR(alarm, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, ALARM);
static SENSOR_DEVICE_ATTR(num_temp_sensors, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, NUM_TEMP_SENSORS);
static SENSOR_DEVICE_ATTR(num_fans, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, NUM_FANS);
static SENSOR_DEVICE_ATTR(max_output_power, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, MAX_OUTPUT_POWER);
static SENSOR_DEVICE_ATTR(date, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, PRODUCTION_DATE);
static SENSOR_DEVICE_ATTR(vendor, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, VENDOR);
static SENSOR_DEVICE_ATTR(temp_input, S_IRUGO, bsp_sysfs_psu_temp_customer_get_attr, NULL, TEMP_INPUT);
static SENSOR_DEVICE_ATTR(temp_alias, S_IRUGO, bsp_sysfs_psu_temp_customer_get_attr, NULL, TEMPALIAS);
static SENSOR_DEVICE_ATTR(temp_type, S_IRUGO, bsp_sysfs_psu_temp_customer_get_attr, NULL, TEMPTYPE);
static SENSOR_DEVICE_ATTR(temp_max, S_IRUGO, bsp_sysfs_psu_temp_customer_get_attr, NULL, TEMPMAX);
static SENSOR_DEVICE_ATTR(temp_max_hyst, S_IRUGO, bsp_sysfs_psu_temp_customer_get_attr, NULL, TEMPMAX_HYST);
static SENSOR_DEVICE_ATTR(temp_min, S_IRUGO, bsp_sysfs_psu_temp_customer_get_attr, NULL, TEMPMIN);
static SENSOR_DEVICE_ATTR(speed, S_IRUGO, bsp_sysfs_fan_customer_get_attr, NULL, PSU_FAN_SPEED);

static struct kobj_attribute loglevel_att =
    __ATTR(loglevel, S_IRUGO | S_IWUSR, bsp_default_loglevel_show, bsp_default_loglevel_store);

static struct kobj_attribute debug_att =
    __ATTR(debug, S_IRUGO | S_IWUSR, bsp_default_debug_show, bsp_default_debug_store);
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *psu_debug_attr[] = {
    &sensor_dev_attr_disable_psu_mon.dev_attr.attr,
    NULL
};

static const struct attribute_group psu_debug_group = {
    .attrs = psu_debug_attr,
};

static struct attribute *psu_customer_device_attributes_num_psu[] = {
    &sensor_dev_attr_num_psus.dev_attr.attr,
    NULL
};

static const struct attribute_group psu_customer_group_num_psu = {
    .attrs = psu_customer_device_attributes_num_psu,
};

static struct attribute *psu_customer_device_attributes[] = {
    &sensor_dev_attr_product_name.dev_attr.attr,
    &sensor_dev_attr_sn.dev_attr.attr,
    &sensor_dev_attr_hw_version.dev_attr.attr,
    &sensor_dev_attr_fw_version.dev_attr.attr,
    &sensor_dev_attr_in_curr.dev_attr.attr,
    &sensor_dev_attr_in_vol.dev_attr.attr,
    &sensor_dev_attr_in_vol_type.dev_attr.attr,
    &sensor_dev_attr_out_curr.dev_attr.attr,
    &sensor_dev_attr_out_vol.dev_attr.attr,
    &sensor_dev_attr_status.dev_attr.attr,
    &sensor_dev_attr_led_status.dev_attr.attr,
    &sensor_dev_attr_in_power.dev_attr.attr,
    &sensor_dev_attr_out_power.dev_attr.attr,
    /*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
    &sensor_dev_attr_alarm.dev_attr.attr,
    &sensor_dev_attr_alarm_threshold_curr.dev_attr.attr,
    &sensor_dev_attr_alarm_threshold_vol.dev_attr.attr,
    &sensor_dev_attr_num_temp_sensors.dev_attr.attr,
    &sensor_dev_attr_num_fans.dev_attr.attr,
    &sensor_dev_attr_max_output_power.dev_attr.attr,
    &sensor_dev_attr_date.dev_attr.attr,
    &sensor_dev_attr_vendor.dev_attr.attr,
    /*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
    NULL
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute *psu_temp_customer_device_attributes[] = {
    &sensor_dev_attr_temp_input.dev_attr.attr,
    &sensor_dev_attr_temp_alias.dev_attr.attr,
    &sensor_dev_attr_temp_type.dev_attr.attr,
    &sensor_dev_attr_temp_max.dev_attr.attr,
    &sensor_dev_attr_temp_max_hyst.dev_attr.attr,
    &sensor_dev_attr_temp_min.dev_attr.attr,
    NULL
};

static struct attribute *psu_fan_customer_device_attributes[] = {
    &sensor_dev_attr_speed.dev_attr.attr,
    NULL
};

static struct attribute *def_attrs[] = {
    &loglevel_att.attr,
    &debug_att.attr,
    NULL,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static const struct attribute_group psu_customer_group = {
    .attrs = psu_customer_device_attributes,
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute_group psu_temp_customer_group = {
    .attrs = psu_temp_customer_device_attributes,
};

static struct attribute_group psu_fan_customer_group = {
    .attrs = psu_fan_customer_device_attributes,
};

static struct attribute_group def_attr_group = {
    .attrs = def_attrs,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

#define PSU_HWMON_SENSOR_DEV_ATTR(__attr_name_str,__attr_index,__mode,__show,__store)  \
    psu_hwmon_dev_attr[__attr_index].dev_attr.attr.name = (__attr_name_str); \
    psu_hwmon_dev_attr[__attr_index].dev_attr.attr.mode = (__mode); \
    psu_hwmon_dev_attr[__attr_index].dev_attr.show = (__show); \
    psu_hwmon_dev_attr[__attr_index].dev_attr.store = (__store); \
    psu_hwmon_dev_attr[__attr_index].index = (__attr_index);

static void bsp_psu_work_handler (struct work_struct *work)
{
    if (!psu_monitor_task_sleep) {
        bsp_psu_monitor_proc ();
    }
    
    mod_timer (&timer, jiffies + 600 * HZ / 1000);
    return;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void bsp_psu_timer_proc (unsigned int data)
#else
static void bsp_psu_timer_proc (struct timer_list *timer)
#endif
{
    if (psu_workqueue) {
        if (!queue_work (psu_workqueue, &work)) {
            DBG_ECHO(DEBUG_ERR, "create h3c sys psu work queue error.\n");
         }
    }

    return;
}

static void bsp_psu_timer_deinit (void)
{
    del_timer (&timer);
    return;
}

static void bsp_psu_workqueue_deinit (void)
{
    if (psu_workqueue) {
        flush_workqueue (psu_workqueue);
        destroy_workqueue (psu_workqueue);
        psu_workqueue = NULL;        
    }
    
    return;
}

static int bsp_psu_workqueue_init (void)
{
    psu_workqueue = create_workqueue ("psu_monitor");
    if (!psu_workqueue) {
        DBG_ECHO(DEBUG_ERR, "create h3c sys psu work queue faild.\n");
        return -ENOMEM;
    }

    INIT_WORK (&work, bsp_psu_work_handler);
    return 0;
}

static void bsp_psu_timer_init (void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
    init_timer (&timer);
    timer.function = bsp_psu_timer_proc;
#else
    timer_setup(&timer, bsp_psu_timer_proc, 0);
#endif

    timer.expires = jiffies + 600 * HZ / 1000;
    add_timer(&timer);
    return;
}

static int __init __psu_init(void)
{
    int ret = ERROR_SUCCESS;
    int i = 0;
    int psu_index = 0;
    int temp_attr_index = 0;
    int temp_attr_arrary_index = 0;
    char temp_str[128] = {0};
    board_static_data *bd = bsp_get_board_data();

    INIT_PRINT("psu module init started\n");

    memset(psu_info, 0 , sizeof(psu_info));
    memset(psu_hwmon_dev_attr, 0, sizeof(psu_hwmon_dev_attr));
    memset(psu_hwmon_attributes, 0, sizeof(psu_hwmon_attributes));
    memset(psu_hwmon_attribute_group, 0, sizeof(psu_hwmon_attribute_group));
    memset(kobj_psu_sub, 0, sizeof(kobj_psu_sub));

    for (psu_index = 0; psu_index < bd->psu_num; psu_index++)
    {
        temp_attr_arrary_index = 0;
        temp_attr_index = SENSOR_NAME + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index] , "name");
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);        
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = IN1_LABEL + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index] , "in%d_label", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = IN1_INPUT + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index] , "in%d_input", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = CURR1_LABEL + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index] , "curr%d_label", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = CURR1_INPUT + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index] , "curr%d_input", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);
   
        temp_attr_index = POWER1_LABEL + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index] , "power%d_label", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);
        
        temp_attr_index = POWER1_INPUT + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index] , "power%d_input", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);
                

        psu_hwmon_attributes[psu_index][temp_attr_arrary_index] = NULL; 
        psu_hwmon_attribute_group[psu_index].attrs = psu_hwmon_attributes[psu_index];

        psu_info[psu_index].status = PSU_STATUS_ABSENT;
        psu_info[psu_index].parent_hwmon = hwmon_device_register(NULL);
        if (!psu_info[psu_index].parent_hwmon) {
            DBG_ECHO(DEBUG_ERR, "psu %d hwmon register failed!\n", psu_index + 1);
            ret = -EACCES;
            goto exit;
        }
        
        ret = sysfs_create_group(&(psu_info[psu_index].parent_hwmon->kobj), &(psu_hwmon_attribute_group[psu_index]));
        DBG_ECHO(DEBUG_INFO, "psu %d! hwmon and attribute group created, ret=%d\n", psu_index, ret);
    }

    kobj_psu_root = kobject_create_and_add("psu", kobj_switch);
    if (!kobj_psu_root) {
        DBG_ECHO(DEBUG_INFO, "psu custom root node create failed!");
        ret =  -EACCES;;
        goto exit;
    }

    if (sysfs_create_group(kobj_psu_root, &def_attr_group) != 0) {
        DBG_ECHO(DEBUG_INFO, "create fan default attr faild.\n");
        ret = -ENOSYS;
        goto exit;
    }
    
    CHECK_IF_ERROR_GOTO_EXIT(ret = sysfs_create_group(kobj_psu_root, &psu_customer_group_num_psu), "create num_psu group failed");
    for (psu_index = 0; psu_index < bd->psu_num; psu_index ++) {
        sprintf(temp_str, "psu%d", psu_index + 1);
        kobj_psu_sub[psu_index] = kobject_create_and_add(temp_str, kobj_psu_root);
        if (!kobj_psu_sub[psu_index]) {
            DBG_ECHO(DEBUG_INFO, "create sub psu node psu%d failed!", psu_index + 1);
            ret =  -EACCES;;
            goto exit;
        }
        CHECK_IF_ERROR_GOTO_EXIT(ret=sysfs_create_group(kobj_psu_sub[psu_index], &psu_customer_group), "failed to create psu custome group!");

        /*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
        for (i = 0; i < bd->psu_sensor_num; i++) {
            sprintf(temp_str, "temp%d", i);
            ret = kobject_init_and_add(&(psu_info[psu_index].sensor_info[i].temp_kobj), &static_kobj_ktype , kobj_psu_sub[psu_index], temp_str);         
            CHECK_IF_ERROR_GOTO_EXIT(ret, "motor%d kobject int and add failed!", i);

            psu_info[psu_index].sensor_info[i].temp_index = i;
            psu_info[psu_index].sensor_info[i].psu_index = psu_index;
            CHECK_IF_ERROR_GOTO_EXIT(ret=sysfs_create_group(&psu_info[psu_index].sensor_info[i].temp_kobj, &psu_temp_customer_group), 
"failed to create psu custome group!");
        }

        for (i = 0; i < bd->psu_fan_num; i++) {
            sprintf(temp_str, "fan%d", i);
            ret = kobject_init_and_add(&(psu_info[psu_index].fan_info[i].fan_kobj), &static_kobj_ktype , kobj_psu_sub[psu_index], temp_str);         
            CHECK_IF_ERROR_GOTO_EXIT(ret, "motor%d kobject int and add failed!", i);

            psu_info[psu_index].fan_info[i].fan_index = i;
            psu_info[psu_index].fan_info[i].psu_index = psu_index;
            CHECK_IF_ERROR_GOTO_EXIT(ret=sysfs_create_group(&psu_info[psu_index].fan_info[i].fan_kobj, &psu_fan_customer_group), 
"failed to create psu custome group!");
        }
        /*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
        
        psu_info[psu_index].customer_kobj = kobj_psu_sub[psu_index];
        DBG_ECHO(DEBUG_DBG, "psu %d kobj=%p", psu_index, kobj_psu_sub[psu_index])
    }

    kobj_psu_debug = kobject_create_and_add("psu", kobj_debug);
    if (!kobj_psu_debug) {
        DBG_ECHO(DEBUG_INFO, "psu debug root node create failed!");
        ret =  -EACCES;;
        goto exit;
    }      
    CHECK_IF_ERROR_GOTO_EXIT(ret = sysfs_create_group(kobj_psu_debug, &psu_debug_group), "failed to create psu debug group!");
    
    bsp_psu_timer_init ();
    ret = bsp_psu_workqueue_init();
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create psu monitor task return %d is error.\n", ret);
    
exit:
    if (ret != ERROR_SUCCESS) {
        INIT_PRINT("psu module init faild!");
        release_all_psu_resource();
    } else {
        INIT_PRINT("psu module init success!");
    }

    return ret;
}

void release_all_psu_resource(void)
{
    int i = 0;
    int j = 0;
    board_static_data *bd = bsp_get_board_data();
    
    for (i = 0; i < bd->psu_num; i++) {
        if (psu_info[i].parent_hwmon != NULL) {
            hwmon_device_unregister(psu_info[i].parent_hwmon);
            psu_info[i].parent_hwmon = NULL;
        }

        if (kobj_psu_sub[i] != NULL) {
            for (j = 0; j < PSU_MAX_SENSOR_NUM_MAX; j++) {
                if (psu_info[i].sensor_info[j].temp_kobj.state_initialized)
                    kobject_put(&(psu_info[i].sensor_info[j].temp_kobj));
            }

            for (j = 0; j < PSU_MAX_FAN_NUM_MAX; j++) {
                if (psu_info[i].fan_info[j].fan_kobj.state_initialized)
                    kobject_put(&(psu_info[i].fan_info[j].fan_kobj));
            }
            
            kobject_put(kobj_psu_sub[i]);
            kobj_psu_sub[i] = NULL;
        }   
    }

    bsp_psu_workqueue_deinit ();
    bsp_psu_timer_deinit ();
    
    if (kobj_psu_debug != NULL) {
        kobject_put(kobj_psu_debug);
        kobj_psu_debug = NULL;
    }

    if (kobj_psu_root != NULL) {
        sysfs_remove_group (kobj_psu_root, &def_attr_group);
        kobject_put(kobj_psu_root);
        kobj_psu_root = NULL;
    }

    return;
}

static void __exit __psu_exit(void)
{
    release_all_psu_resource();
    INIT_PRINT("module sensor uninstalled !\n");
    return;
}

module_init(__psu_init);
module_exit(__psu_exit);
EXPORT_SYMBOL(bsp_psu_get_status);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Wan Huan <wan.huan@h3c.com>");
MODULE_DESCRIPTION("h3c system psu driver");
MODULE_LICENSE("Dual BSD/GPL");
