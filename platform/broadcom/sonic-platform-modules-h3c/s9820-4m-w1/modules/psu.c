
/*公有文件引入*/
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
//#include <linux/syscalls.h>
//#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include "linux/kthread.h"
#include <linux/delay.h>


/*私有文件*/
#include "pub.h"
#include "bsp_base.h"
#include "i2c_dev_reg.h"

#define MODULE_NAME "psu"

void release_all_psu_resource(void);

int psu_debug_level =  DEBUG_INFO | DEBUG_ERR;


#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(psu_debug_level, level, fmt, ##args)

#define ATTR_NAME_MAX_LEN    48

enum command_type
{
    VOLTAGE_OUT,
    CURRENT_OUT,
    VOLTAGE_IN,
    CURRENT_IN,
    MFR_ID_CMD,
    HW_VERSION_CMD,
    FW_VERSION_CMD,
    VOLTAGE_IN_TYPE_CMD,
    STATUS_WORD_CMD,
    STATUS_TEMPERATURE,
    INPUT_TEMP,
    FAN_SPEED,
    SN_NUMBER,
    POWER_IN,
    POWER_OUT,
    PRODUCT_NAME_CMD,
    VENDOR_NAME_CMD
};


char *psu_status_string[] = {"Absent", "Normal", "Fault", "Unknown"};

enum psu_hwmon_sysfs_attributes
{
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


enum psu_customer_sysfs_attributes
{
    NUM_PSUS,
    PRODUCT_NAME,
    VENDOR_NAME,
    MFR_ID,
    SN,
    HW_VERSION,
    IN_VOL_TYPE,
    FW_VERSION,
    IN_CURR,
    IN_VOL,
    OUT_CURR,
    OUT_VOL,
    TEMP_INPUT,
    TEMPALIAS,
    TEMPTYPE,
    TEMPMAX,
    TEMPMAX_HYST,
    TEMPMIN,
    STATUS_WORD,
    STATUS,
    FAN,
    LED_STATUS,
    IN_POWER,
    OUT_POWER,
    DIS_PSU_MON
};

static struct kobject *kobj_psu_root = NULL;
static struct kobject *kobj_psu_debug = NULL;
static struct kobject *kobj_psu_sub[MAX_PSU_NUM] = {NULL};

typedef struct
{
    int status;
    int status_word;
    int vout;
    int iout;
    int vin;
    int iin;
    int pin;
    int pout;
    int tempinput;    //(当前温度)
    int fan_speed;    //(当前风速)
    int tempstatus;
    int vin_type;
    char sn_number[PSU_MAX_SN_LEN];
    char product_name[PSU_MAX_PRODUCT_NAME_LEN];
    char vendor_name[PSU_MAX_VENDOR_NAME_LEN];
    char MFR_ID[PSU_MAX_MFR_ID_LEN + 1];
    char hw_version[PSU_MAX_HW_VERSION_LEN + 1];
    char fw_version[PSU_MAX_FW_VERSION_LEN + 1];
    //struct attribute_group * psu_hwmon_attribute_group;
    struct device *parent_hwmon;
    struct kobject *customer_kobj;
} psu_info_st;


static psu_info_st psu_info[MAX_PSU_NUM] = {{0}};
static struct sensor_device_attribute psu_hwmon_dev_attr[PSU_ATTRIBUTE_BUTT];       //device attribute
static struct attribute *psu_hwmon_attributes[MAX_PSU_NUM][PSU_ATTRIBUTE_BUTT / MAX_PSU_NUM + 1] = {{0}};    //attribute array, MAX_PSU_NUM 必须大于1
static struct attribute_group psu_hwmon_attribute_group[MAX_PSU_NUM] = {{0}};                                //attribute group
static char psu_hwmon_attr_name[PSU_ATTRIBUTE_BUTT][ATTR_NAME_MAX_LEN] = {{0}};    //属性名                           //attribute name string

struct task_struct *psu_monitor_task = NULL;   //监控任务
int psu_mon_interval = 100;                   //任务间隔ms
int psu_monitor_task_sleep = 0;

/*************************************************************************/
char *bsp_psu_get_status_string(int status)
{
    if (status >= PSU_STATUS_ABSENT && status <= PSU_STATUS_UNKNOWN)
    {
        return psu_status_string[status];
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "psu unknown status %d", status);
        return psu_status_string[PSU_STATUS_UNKNOWN];
    }
}

int bsp_psu_get_status(int psu_index)
{
    board_static_data *bd = bsp_get_board_data();

    if (psu_index < 0 || psu_index >= bd->psu_num)
    {
        DBG_ECHO(DEBUG_INFO, "bsp_psu_get_status psu_index is out of range");
        return -ERROR_FAILED;
    }
    return psu_info[psu_index].status;
}
EXPORT_SYMBOL(bsp_psu_get_status);


u32 calc_line(u8 *CalcNum)
{
    u32 power = 0;
    u32 base = 0;
    u32 result = 0;

    u8 flag = (CalcNum[1] >> 7) & (0x1);
    if (flag == 1)
    {
        power = (~(CalcNum[1] >> 3) & (0x1f)) + 1;
    }
    else
    {
        power = (CalcNum[1] >> 3) & 0x1f;
    }
    base = ((((u32)CalcNum[1] << 8) & 0x300) | (u32)CalcNum[0]) * 100;
    //base = ((u32)(CalcNum[1] & 0x7) << 8) | ((u32)CalcNum[0]));
    //calc_p(power, &base, flag);
    result = flag == 1 ? base >> power : base << power;
    return result;
}

//mv
u32 calc_voltage(u8 *CalcNum)
{
    u32 Vout = (CalcNum[0] + (((u32)CalcNum[1]) << 8)) * 1000 / 512;
    return Vout;
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
    if (usTempMant & 0x400)
    {
        usTempMant |= 0xf800;
        sMantissa = (short)usTempMant;
        sMantissa = -sMantissa;
    }
    if (ucTempExp & 0x10)
    {
        ucTempExp |= 0xE0;
        cExp = (char)ucTempExp;
        cExp = -cExp;
        value = (u16)sMantissa >> (u8)cExp;
    }
    else
    {
        value = (u16)((u32)(u16)sMantissa << (u32)(u8)cExp);
    }
    return (u32)value;

}


u32 calc_current_remain(u8 *CalcNum)
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

    if (usTempMant & 0x400)
    {
        usTempMant |= 0xf800;
        sMantissa = (short)usTempMant;
        sMantissa = -sMantissa;
    }
    if (ucTempExp & 0x10)
    {
        ucTempExp |= 0xE0;
        cExp = (char)ucTempExp;
        cExp = -cExp;
        value = ((u16)sMantissa * 100 >> (u8)cExp) % 100;
    }
    else
    {
        value = 0;
    }
    return (u32)value;

}

int bsp_psu_get_value(int command, int psu_index, u32 *value)
{
    int ret = ERROR_SUCCESS;
    u8 temp_value[PSU_MAX_INFO_LEN] = {0};
    board_static_data *bd = bsp_get_board_data();
    int i = 0;

    if ((NULL == bd) || (NULL == value))
    {
        DBG_ECHO(DEBUG_INFO, "bsp_psu_get_value bd or value is null");
        ret = ERROR_FAILED;
        return ret;
    }

    if ((psu_index < 0) || (psu_index >= bd->psu_num))
    {
        DBG_ECHO(DEBUG_INFO, "bsp_psu_get_value psu_index is out of range");
        ret = ERROR_FAILED;
        return ret;
    }
    ret = lock_i2c_path(I2C_DEV_PSU + psu_index);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "lock psu %d i2c path failed", psu_index);

    switch (command)
    {

        case CURRENT_IN:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                *value = 0;
            }
            else
            {
                ret = bsp_i2c_power1600W_read_current_in(bd->i2c_addr_psu_pmbus[psu_index], PW1600_IIN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_current_integer(temp_value) * 100 + calc_current_remain(temp_value);
            }
            break;
        }
        case CURRENT_OUT:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_current(bd->i2c_addr_psu_pmbus[psu_index], PW650_IOUT_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_current(bd->i2c_addr_psu_pmbus[psu_index], PW1600_IOUT_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_current_integer(temp_value) * 100 + calc_current_remain(temp_value);
            }
            break;
        }
        case VOLTAGE_OUT:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_voltage(bd->i2c_addr_psu_pmbus[psu_index], PW650_VOUT_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_voltage(bd->i2c_addr_psu_pmbus[psu_index], PW1600_VOUT_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_voltage(temp_value) / 10;

            }
            break;
        }
        case VOLTAGE_IN:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                *value = 0;
            }
            else
            {
                ret = bsp_i2c_power1600W_read_voltage_in(bd->i2c_addr_psu_pmbus[psu_index], PW1600_VIN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            break;
        }
        case VOLTAGE_IN_TYPE_CMD:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                *value = PSU_IN_VOL_TYPE_NOT_SUPPORT;
            }
            else
            {
                ret = bsp_i2c_power1600W_read_voltage_in_type(bd->i2c_addr_psu_pmbus[psu_index], PW1600_VIN_TYPE_BYTE_COUNT, temp_value);
                *value = temp_value[0];
            }
            break;
        }
        case STATUS_WORD_CMD:
        {
            ret = bsp_i2c_power_read_status_word(bd->i2c_addr_psu_pmbus[psu_index], PW650_STAWORD_BYTE_COUNT, temp_value);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            *value = temp_value[0] + (temp_value[1] << 8);
            break;
        }

        case STATUS_TEMPERATURE:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_temperstatus(bd->i2c_addr_psu_pmbus[psu_index], PW650_STATEMPURE_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = temp_value[0];
            }
            else
            {
                ret = bsp_i2c_power1600W_read_temperstatus(bd->i2c_addr_psu_pmbus[psu_index], PW1600_STATEMPURE_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = temp_value[0];
            }
            break;
        }
        case INPUT_TEMP:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_temper(bd->i2c_addr_psu_pmbus[psu_index], PW650_TEMPURE_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_temper(bd->i2c_addr_psu_pmbus[psu_index], PW1600_TEMPURE_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            break;
        }
        case FAN_SPEED:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_fan_speed(bd->i2c_addr_psu_pmbus[psu_index], PW650_FAN_BYTE_COUNT, 1, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                if (temp_value[0] == 0 && temp_value[1] == 0)
                {
                    ret = bsp_i2c_power650W_read_fan_speed(bd->i2c_addr_psu_pmbus[psu_index], PW650_FAN_BYTE_COUNT, 0, temp_value);
                    CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for read_fan_speed! command=%d", command);
                }
                *value = calc_line(temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_fan_speed(bd->i2c_addr_psu_pmbus[psu_index], PW1600_FAN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for read_fan_speed! command=%d", command);
                *value = calc_line(temp_value);
            }
            break;
        }
        case SN_NUMBER:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_SN(bd->i2c_addr_psu[psu_index], PW650_SN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                sprintf((u8 *)value, "%s", temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_SN(bd->i2c_addr_psu[psu_index], PW1600_SN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                sprintf((u8 *)value, "%s", temp_value);
            }
            break;
        }
        case PRODUCT_NAME_CMD:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power_reg_read(bd->i2c_addr_psu_pmbus[psu_index], REG_ADDR_PW650W_PRONUMB, PW650_PRONUMB_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                //  FSP: YM-2651B   GRE: PSR650B-12A1
                if (strstr(temp_value, "M-2651B") != NULL || strcmp(temp_value, "PSR650B-12A1") == 0)
                {
                    sprintf((u8 *)value, "%s", "LSVM1AC650");
                }
                else
                {
                    sprintf((u8 *)value, "%s", "Unknown");
                }
            }
            else
            {
                ret = bsp_i2c_power1600W_read_SN(bd->i2c_addr_psu[psu_index], PW1600_SN_BYTE_COUNT, temp_value);

                if (strstr(temp_value, "0231ABV7") != NULL)
                {
                    sprintf((u8 *)value, "%s", "PSR1600B-12A-B");
                }
                else
                {
                    sprintf((u8 *)value, "%s", "Unknown");
                }
            }
            break;
        }
        case VENDOR_NAME_CMD:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_vendorname(bd->i2c_addr_psu[psu_index], PW650_VENDOR_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                sprintf((u8 *)value, "%s", temp_value);
            }
            else if (bd->psu_type == PSU_TYPE_1600W)
            {
                ret = bsp_i2c_power1600W_read_vendorname(bd->i2c_addr_psu[psu_index], PW1600_VENDOR_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                sprintf((u8 *)value, "%s", temp_value);
            }
            else
            {
                sprintf((u8 *)value, "%s", "N/A");
            }
            break;
        }
        case MFR_ID_CMD:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_MFR_ID(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_MFR_ID_LEN, temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_MFR_ID(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_MFR_ID_LEN, temp_value);
            }
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);

            if (temp_value[0] < (PSU_MAX_MFR_ID_LEN - 1))
            {
                //first byte is the element length, followed by the content
                temp_value[temp_value[0] + 1] = '\0';
            }
            else
            {
                temp_value[PSU_MAX_MFR_ID_LEN] = '\0';
            }

            //trim 0xff
            for (i = 0; i < PSU_MAX_MFR_ID_LEN; i++)
            {
                if (temp_value[i] == (u8)0xff)
                    temp_value[i] = '\0';
            }
            sprintf((u8 *)value, "%s", temp_value);
            break;
        }
        case HW_VERSION_CMD:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_hw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_HW_VERSION_LEN, temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_hw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_HW_VERSION_LEN, temp_value);
            }
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);

            if (temp_value[0] < (PSU_MAX_HW_VERSION_LEN - 1))
            {
                //first byte is the element length, followed by the content
                temp_value[temp_value[0] + 1] = '\0';
            }
            else
            {
                temp_value[PSU_MAX_HW_VERSION_LEN] = '\0';
            }
            //trim 0xff
            for (i = 0; i < PSU_MAX_HW_VERSION_LEN; i++)
            {
                if (temp_value[i] == (u8)0xff)
                    temp_value[i] = '\0';
            }
            sprintf((u8 *)value, "%s", temp_value);
            break;
        }
        case FW_VERSION_CMD:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_fw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_FW_VERSION_LEN, temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_hw_version(bd->i2c_addr_psu_pmbus[psu_index], PSU_MAX_FW_VERSION_LEN, temp_value);
            }
            CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
            if (temp_value[0] < (PSU_MAX_FW_VERSION_LEN - 1))
            {
                //first byte is the element length, followed by the content
                temp_value[temp_value[0] + 1] = '\0';
            }
            else
            {
                temp_value[PSU_MAX_FW_VERSION_LEN] = '\0';
            }
            //trim 0xff
            for (i = 0; i < PSU_MAX_FW_VERSION_LEN; i++)
            {
                if (temp_value[i] == (u8)0xff)
                    temp_value[i] = '\0';
            }
            sprintf((u8 *)value, "%s", temp_value);

            break;
        }
        case POWER_IN:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_powerin(bd->i2c_addr_psu_pmbus[psu_index], PW650_PIN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_powerin(bd->i2c_addr_psu_pmbus[psu_index], PW1600_PIN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            break;
        }
        case POWER_OUT:
        {
            if (bd->psu_type == PSU_TYPE_650W)
            {
                ret = bsp_i2c_power650W_read_powerout(bd->i2c_addr_psu_pmbus[psu_index], PW650_FAN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            else
            {
                ret = bsp_i2c_power1600W_read_powerout(bd->i2c_addr_psu_pmbus[psu_index], PW1600_FAN_BYTE_COUNT, temp_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_psu_get_value! command=%d", command);
                *value = calc_line(temp_value);
            }
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "unknown command %d", command);
            ret = ERROR_FAILED;
            break;
        }
    }
exit:
    unlock_i2c_path();

    return ret;
}


//找到dev对应的psu index
int bsp_sysfs_psu_get_index_from_kobj(struct kobject *kobj)
{
    int i;
    int psu_num = bsp_get_board_data()->psu_num;
    for (i = 0; i < psu_num; i++)
    {
        if (psu_info[i].customer_kobj == kobj)
            return i;
    }
    DBG_ECHO(DEBUG_ERR, "Not found matched psu index, kobj=%p", kobj);
    return -1;
}

//除法返回浮点数字符串
char *math_int_divide(int dividend, int divisor, int decimal_width, char *result)
{
    int i = 0;
    int mod = dividend % divisor;
    int integer = 0;
    int decimal = 0;
    //char temp[20] = {0};
    int len = 0;
    integer = dividend / divisor;
    decimal_width = decimal_width > 10 ? 10 : decimal_width;
    len = sprintf(result, "%d%s", integer, decimal_width == 0 ? "" : ".");
    for (i = 0; i < decimal_width; i++, decimal *= 10)
    {
        decimal = mod * 10 / divisor;
        len += sprintf(result + len, "%d", decimal);
        mod = mod * 10 % divisor;
    }
    return result;
}

int parameter_int_to_float_deal(int value, int *integer_value, int *float_value)
{

    int temp_value = 0;
    *integer_value = value / 100;
    temp_value = value % 100;
    if (temp_value > 9)
    {
        *float_value = temp_value / 10;
    }
    else
    {
        *float_value = temp_value;
    }
    return 0;
}

static ssize_t bsp_sysfs_psu_hwmon_get_attr(struct device *dev, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int psu_index = 0;

    if ((attr->index >= SENSOR_NAME) && (attr->index <= SENSOR_NAME_BUTT))
    {
        psu_index = attr->index - SENSOR_NAME;
        return sprintf(buf, "Power%d %s\n", psu_index + 1, bsp_psu_get_status_string(psu_info[psu_index].status));
    }
    else if ((attr->index >= IN1_LABEL) && (attr->index <= IN_LABEL_BUTT))
    {
        return sprintf(buf, "Voltage %d\n", attr->index - IN1_LABEL + 1);
    }
    else if ((attr->index >= CURR1_LABEL) && (attr->index <= CURR_LABEL_BUTT))
    {
        return sprintf(buf, "Current %d\n", attr->index - CURR1_LABEL + 1);
    }
    else if ((attr->index >= POWER1_LABEL) && (attr->index <= POWER_LABEL_BUTT))
    {
        return sprintf(buf, "Power %d\n", attr->index - CURR1_LABEL + 1);
    }
    else if ((attr->index >= IN1_INPUT) && (attr->index <= IN_INPUT_BUTT))
    {
        psu_index = attr->index - IN1_INPUT;
        return sprintf(buf, "%d\n", psu_info[psu_index].vout * 10);
    }
    else if ((attr->index >= CURR1_INPUT) && (attr->index <= CURR_INPUT_BUTT))
    {
        psu_index = attr->index - CURR1_INPUT;
        return sprintf(buf, "%d\n", psu_info[psu_index].iout * 10);
    }
    else if ((attr->index >= POWER1_INPUT) && (attr->index <= POWER_INPUT_BUTT))
    {
        psu_index = attr->index - POWER1_INPUT;
        return sprintf(buf, "%d\n", psu_info[psu_index].pout * 10000);
    }
    else
    {
        return sprintf(buf, "0\n");
    }

    return 0;

}



static ssize_t bsp_sysfs_psu_debug_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    ssize_t index = -EIO;
    //    int uiChanNo = 0;
    //   int data = 0;

    switch (attr->index)
    {
        case DIS_PSU_MON:
        {
            index = sprintf(buf, "%d", psu_monitor_task_sleep);
            break;
        }
        default:
        {
            index = sprintf(buf, "Not support attribute %d\n", attr->index);
            break;
        }
    }

    return index;
}


static ssize_t bsp_sysfs_psu_customer_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int index = 0;
    //u32 temp_value = 0;
    //u8  temp_value_u8 = 0;
    int psu_index = 0;
    int int_value = 0;
    int float_value = 0;
    //char temp[128] = {0};
    board_static_data *bd = bsp_get_board_data();

    if (attr->index != NUM_PSUS)
    {
        psu_index = bsp_sysfs_psu_get_index_from_kobj((struct kobject *)kobj);
        if ((psu_info[psu_index].status == PSU_STATUS_ABSENT) && (attr->index != STATUS))
        {
            DBG_ECHO(DEBUG_ERR, "psu %d is absent!\n", psu_index + 1);
            return -ENODEV;
        }
    }
    switch (attr->index)
    {
        case NUM_PSUS:
        {
            index = sprintf(buf, "%d\n", (int)bd->psu_num);
            break;
        }
        case PRODUCT_NAME:
        {
            if (bsp_psu_get_value(PRODUCT_NAME_CMD, psu_index, (u32 *)(psu_info[psu_index].product_name)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get PRODUCT_NAME failed!\n", psu_index + 1);
                sprintf(psu_info[psu_index].product_name, "%s", "N/A");
            }
            index = sprintf(buf, "%s\n", psu_info[psu_index].product_name);
            break;
        }
        case VENDOR_NAME:
        {
            if (bsp_psu_get_value(VENDOR_NAME_CMD, psu_index, (u32 *)(psu_info[psu_index].vendor_name)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get VENDOR_NAME failed!\n", psu_index + 1);
                sprintf(psu_info[psu_index].vendor_name, "%s", "N/A");
            }
            index = sprintf(buf, "%s\n", psu_info[psu_index].vendor_name);
            break;
        }
        case SN:
        {
            if (bsp_psu_get_value(SN_NUMBER, psu_index, (u32 *)(psu_info[psu_index].sn_number)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get sn failed!\n", psu_index + 1);
                sprintf(psu_info[psu_index].sn_number, "%s", "N/A");
            }
            index = sprintf(buf, "%s\n", psu_info[psu_index].sn_number);
            break;
        }
        case MFR_ID:
        {
            if (bsp_psu_get_value(MFR_ID_CMD, psu_index, (u32 *)(psu_info[psu_index].MFR_ID)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get MFR ID failed!\n", psu_index + 1);
                sprintf(psu_info[psu_index].MFR_ID, "%s", "N/A");
            }
            index = sprintf(buf, "%s\n", psu_info[psu_index].MFR_ID);
            break;
        }
        case HW_VERSION:
        {
            if (bsp_psu_get_value(HW_VERSION_CMD, psu_index, (u32 *)(psu_info[psu_index].hw_version)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get hw version failed!\n", psu_index + 1);
                sprintf(psu_info[psu_index].hw_version, "%s", "N/A");
            }
            index = sprintf(buf, "%s\n", psu_info[psu_index].hw_version);
            break;
        }
        case FW_VERSION:
        {
            if (bsp_psu_get_value(FW_VERSION_CMD, psu_index, (u32 *)(psu_info[psu_index].fw_version)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get fw version failed!\n", psu_index + 1);
                sprintf(psu_info[psu_index].fw_version, "%s", "N/A");
            }
            index = sprintf(buf, "%s\n", psu_info[psu_index].fw_version);
            break;
        }
        case IN_CURR:
        {
            if (bsp_psu_get_value(CURRENT_IN, psu_index, &(psu_info[psu_index].iin)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get input current failed!", psu_index + 1);
            }
            if (bd->psu_type == PSU_TYPE_650W)
            {
                index = sprintf(buf, "no_support\n");
            }
            else
            {
                parameter_int_to_float_deal(psu_info[psu_index].iin, &int_value, &float_value);
                index = sprintf(buf, "%d.%d\n", int_value, float_value);
            }
            break;
        }
        case IN_VOL:
        {
            if (bsp_psu_get_value(VOLTAGE_IN, psu_index, &(psu_info[psu_index].vin)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get input voltage failed!", psu_index + 1);
            }
            if (bd->psu_type == PSU_TYPE_650W)
            {
                index = sprintf(buf, "no_support\n");
            }
            else
            {
                parameter_int_to_float_deal(psu_info[psu_index].vin, &int_value, &float_value);
                index = sprintf(buf, "%d.%d\n", int_value, float_value);
            }
            break;
        }
        case IN_VOL_TYPE:
        {
            if (bsp_psu_get_value(VOLTAGE_IN_TYPE_CMD, psu_index, &(psu_info[psu_index].vin_type)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get fw version failed!\n", psu_index + 1);
                index = sprintf(buf, "Failed to get voltage input type!\n");
            }
            else
            {
                switch (psu_info[psu_index].vin_type)
                {
                    case PSU_IN_VOL_TYPE_NO_INPUT:
                    {
                        index = sprintf(buf, "No Input\n");
                        break;
                    }
                    case PSU_IN_VOL_TYPE_AC:
                    {
                        index = sprintf(buf, "AC\n");
                        break;
                    }
                    case PSU_IN_VOL_TYPE_HVDC:
                    {
                        index = sprintf(buf, "DC\n");
                        break;
                    }
                    case PSU_IN_VOL_TYPE_NOT_SUPPORT:
                    {
                        index = sprintf(buf, "no_support\n");
                        break;
                    }
                    default:
                    {
                        index = sprintf(buf, "Unknown vin type %d\n", psu_info[psu_index].vin_type);
                        break;
                    }
                }
            }
            break;
        }
        case OUT_CURR:
        {
            if (bsp_psu_get_value(CURRENT_OUT, psu_index, &(psu_info[psu_index].iout)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get current out failed!", psu_index + 1);
                psu_info[psu_index].iout = 0;
            }
            parameter_int_to_float_deal(psu_info[psu_index].iout, &int_value, &float_value);
            index = sprintf(buf, "%d.%d\n", int_value, float_value);
            break;
        }
        case OUT_VOL:
        {
            if (bsp_psu_get_value(VOLTAGE_OUT, psu_index, &(psu_info[psu_index].vout)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get voltage out failed!", psu_index + 1);
                psu_info[psu_index].vout = 0;
            }
            if (bd->psu_type == PSU_TYPE_650W)
            {
                parameter_int_to_float_deal(psu_info[psu_index].vout, &int_value, &float_value);
                index = sprintf(buf, "%d.%d\n", int_value, float_value);
            }
            else
            {
                parameter_int_to_float_deal(psu_info[psu_index].vout, &int_value, &float_value);
                index = sprintf(buf, "%d.%d\n", int_value, float_value);
            }
            break;
        }
        case TEMP_INPUT:
        {
            if (bsp_psu_get_value(INPUT_TEMP, psu_index, &(psu_info[psu_index].tempinput)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get current temperature  failed!", psu_index + 1);
                psu_info[psu_index].tempinput = 0;
            }
            parameter_int_to_float_deal(psu_info[psu_index].tempinput, &int_value, &float_value);
            index = sprintf(buf, "%d.%d\n", int_value, float_value);
            break;
        }
        case TEMPALIAS:
        {
            index = sprintf(buf, "psu_inner\n");
            break;
        }
        case TEMPTYPE:
        {
            index = sprintf(buf, "psu sensor%d\n", attr->index);
            break;
        }
        case TEMPMAX:
        {
            index = sprintf(buf, "%d\n", 80);
            break;
        }
        case TEMPMAX_HYST:
        {
            index = sprintf(buf, "%d\n", 5);
            break;
        }
        case TEMPMIN:
        {
            index = sprintf(buf, "%d\n", 0);
            break;
        }
        case STATUS_WORD:
        {
            if (bsp_psu_get_value(STATUS_WORD_CMD, psu_index, &(psu_info[psu_index].status_word)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get STATUS_WORD failed!\n", psu_index + 1);
            }
            index = sprintf(buf, "%d\n", psu_info[psu_index].status_word);
            break;
        }
        case STATUS:
        {
            index = sprintf(buf, "%d\n", psu_info[psu_index].status);
            break;
        }
        case FAN:
        {
            if (bsp_psu_get_value(FAN_SPEED, psu_index, &(psu_info[psu_index].fan_speed)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get fan speed failed!", psu_index + 1);
                psu_info[psu_index].fan_speed = 0;
            }
            parameter_int_to_float_deal(psu_info[psu_index].fan_speed, &int_value, &float_value);
            index = sprintf(buf, "%d.%d\n", int_value, float_value);
            break;
        }
        case LED_STATUS:
        {
            if (psu_info[psu_index].status == PSU_STATUS_ABSENT)           //(absent)
            {
                index = sprintf(buf, "%d\n", LED_COLOR_DARK);
            }
            else if (psu_info[psu_index].status == PSU_STATUS_FAULT)     //(fault)
            {
                index = sprintf(buf, "%d\n", LED_COLOR_RED);
            }
            else if (psu_info[psu_index].status == PSU_STATUS_NORMAL)    //(normal)
            {
                index = sprintf(buf, "%d\n", LED_COLOR_GREEN);
            }
            break;
        }
        case IN_POWER:
        {
            if (bsp_psu_get_value(POWER_IN, psu_index, &(psu_info[psu_index].pin)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get power in failed!", psu_index + 1);
                psu_info[psu_index].pin = 0;
            }
            parameter_int_to_float_deal(psu_info[psu_index].pin, &int_value, &float_value);
            index = sprintf(buf, "%d.%d\n", int_value, float_value);
            break;
        }
        case OUT_POWER:
        {
            if (bsp_psu_get_value(POWER_OUT, psu_index, &(psu_info[psu_index].pout)) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get power out failed!", psu_index + 1);
                psu_info[psu_index].pout = 0;
            }
            parameter_int_to_float_deal(psu_info[psu_index].pout, &int_value, &float_value);
            index = sprintf(buf, "%d.%d\n", int_value, float_value);
            break;
        }
        default:
        {
            index = sprintf(buf, "Not support attribute %d\n", attr->index);
            break;
        }
    }
    return index;
}


//电源模块监控任务
int h3c_psu_monitor_thread(void *arg)
{
    int i = 0;
    u8 absent = 0;
    u8 good = 0;
    int temp_status = 0;
    size_t psu_num = 0;

    board_static_data *bd = bsp_get_board_data();
    psu_num = bd->psu_num;

    while (1)
    {
        msleep(psu_mon_interval);
        if (kthread_should_stop())
            break;
        if (psu_monitor_task_sleep == 1)
        {
            continue;
        }

        for (i = 0; i < psu_num; i++)
        {

            if (bsp_cpld_get_psu_absent(&absent, i) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get absent failed!", i + 1);
                continue;
            }
            if (bsp_cpld_get_psu_good(&good, i) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "psu %d get status failed!", i + 1);
                continue;
            }
            temp_status = (absent == 1) ? PSU_STATUS_ABSENT :
                          ((good == 1) ? PSU_STATUS_NORMAL : PSU_STATUS_FAULT);
            if (temp_status == PSU_STATUS_NORMAL)
            {
                // update voltage out and current out
                if ((bsp_psu_get_value(VOLTAGE_OUT, i, &(psu_info[i].vout)) != ERROR_SUCCESS) || (bsp_psu_get_value(CURRENT_OUT, i, &(psu_info[i].iout)) != ERROR_SUCCESS))
                {
                    psu_info[i].vout = 0;
                    psu_info[i].iout = 0;
                    // correct temp psu status from Normal to Fault due to fail access
                    temp_status = PSU_STATUS_FAULT;
                }
            }
            if (temp_status != psu_info[i].status)
            {
                // update psu status
                psu_info[i].status = temp_status;
                DBG_ECHO(DEBUG_INFO, "Power%d status changed from %s to %s", i + 1, bsp_psu_get_status_string(psu_info[i].status), bsp_psu_get_status_string(temp_status));
            }
        }

    }
    DBG_ECHO(DEBUG_INFO, "psu monitor task exited.");
    return ERROR_SUCCESS;
}



static ssize_t bsp_sysfs_psu_debug_set_attr(struct device *kobject, struct device_attribute *da, const char *buf, size_t count)
{
    int temp = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);

    switch (attr->index)
    {
        case DIS_PSU_MON:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected!", buf);
            }
            else
            {
                psu_monitor_task_sleep = (temp != 0);
            }
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "unknown attribute index %d", attr->index);
            break;
        }
    }
    return count;
}


//customer private node
static SENSOR_DEVICE_ATTR(num_psus, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, NUM_PSUS);
static SENSOR_DEVICE_ATTR(product_name, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, PRODUCT_NAME);
static SENSOR_DEVICE_ATTR(vendor_name, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, VENDOR_NAME);
static SENSOR_DEVICE_ATTR(sn, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, SN);
static SENSOR_DEVICE_ATTR(vendor_name_id, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, MFR_ID);
static SENSOR_DEVICE_ATTR(hw_version, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, HW_VERSION);
static SENSOR_DEVICE_ATTR(fw_version, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, FW_VERSION);
static SENSOR_DEVICE_ATTR(in_curr, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_CURR);
static SENSOR_DEVICE_ATTR(in_vol, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_VOL);
static SENSOR_DEVICE_ATTR(out_curr, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, OUT_CURR);
static SENSOR_DEVICE_ATTR(out_vol, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, OUT_VOL);
static SENSOR_DEVICE_ATTR(temp_input, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, TEMP_INPUT);
static SENSOR_DEVICE_ATTR(temp_alias, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, TEMPALIAS);
static SENSOR_DEVICE_ATTR(temp_type, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, TEMPTYPE);
static SENSOR_DEVICE_ATTR(temp_max, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, TEMPMAX);
static SENSOR_DEVICE_ATTR(temp_max_hyst, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, TEMPMAX_HYST);
static SENSOR_DEVICE_ATTR(temp_min, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, TEMPMIN);
static SENSOR_DEVICE_ATTR(status_word, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, STATUS_WORD);
static SENSOR_DEVICE_ATTR(status, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, STATUS);
static SENSOR_DEVICE_ATTR(fan, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, FAN);
static SENSOR_DEVICE_ATTR(led_status, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, LED_STATUS);

static SENSOR_DEVICE_ATTR(in_power, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_POWER);
static SENSOR_DEVICE_ATTR(out_power, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, OUT_POWER);

static SENSOR_DEVICE_ATTR(in_vol_type, S_IRUGO, bsp_sysfs_psu_customer_get_attr, NULL, IN_VOL_TYPE);


static SENSOR_DEVICE_ATTR(disable_psu_mon, S_IRUGO | S_IWUSR, bsp_sysfs_psu_debug_get_attr, bsp_sysfs_psu_debug_set_attr, DIS_PSU_MON);

static struct attribute *psu_debug_attr[] =
{
    &sensor_dev_attr_disable_psu_mon.dev_attr.attr,
    NULL
};

static const struct attribute_group psu_debug_group =
{
    .attrs = psu_debug_attr,
};

static struct attribute *psu_customer_device_attributes_num_psu[] =
{
    &sensor_dev_attr_num_psus.dev_attr.attr,
    NULL
};

static const struct attribute_group psu_customer_group_num_psu =
{
    .attrs = psu_customer_device_attributes_num_psu,
};


static struct attribute *psu_customer_device_attributes[] =
{
    &sensor_dev_attr_product_name.dev_attr.attr,
    &sensor_dev_attr_sn.dev_attr.attr,
    &sensor_dev_attr_vendor_name_id.dev_attr.attr,
    &sensor_dev_attr_hw_version.dev_attr.attr,
    &sensor_dev_attr_fw_version.dev_attr.attr,
    &sensor_dev_attr_in_curr.dev_attr.attr,
    &sensor_dev_attr_in_vol.dev_attr.attr,
    &sensor_dev_attr_in_vol_type.dev_attr.attr,
    &sensor_dev_attr_out_curr.dev_attr.attr,
    &sensor_dev_attr_out_vol.dev_attr.attr,
    &sensor_dev_attr_temp_input.dev_attr.attr,
    &sensor_dev_attr_temp_alias.dev_attr.attr,
    &sensor_dev_attr_temp_type.dev_attr.attr,
    &sensor_dev_attr_temp_max.dev_attr.attr,
    &sensor_dev_attr_temp_max_hyst.dev_attr.attr,
    &sensor_dev_attr_temp_min.dev_attr.attr,
    &sensor_dev_attr_status.dev_attr.attr,
    &sensor_dev_attr_fan.dev_attr.attr,
    &sensor_dev_attr_led_status.dev_attr.attr,
    &sensor_dev_attr_in_power.dev_attr.attr,
    &sensor_dev_attr_out_power.dev_attr.attr,
    &sensor_dev_attr_vendor_name.dev_attr.attr,
    &sensor_dev_attr_status_word.dev_attr.attr,
    NULL
};

static const struct attribute_group psu_customer_group =
{
    .attrs = psu_customer_device_attributes,
};



#define PSU_HWMON_SENSOR_DEV_ATTR(__attr_name_str,__attr_index,__mode,__show,__store)  \
    psu_hwmon_dev_attr[__attr_index].dev_attr.attr.name = (__attr_name_str); \
    psu_hwmon_dev_attr[__attr_index].dev_attr.attr.mode = (__mode); \
    psu_hwmon_dev_attr[__attr_index].dev_attr.show = (__show); \
    psu_hwmon_dev_attr[__attr_index].dev_attr.store = (__store); \
    psu_hwmon_dev_attr[__attr_index].index = (__attr_index);


//设置初始化入口函数
static int __init __psu_init(void)
{
    int ret = ERROR_SUCCESS;
    //int i = 0;
    int psu_index = 0;
    int temp_attr_index = 0;
    int temp_attr_arrary_index = 0;
    char temp_str[128] = {0};
    board_static_data *bd = bsp_get_board_data();

    INIT_PRINT("psu module init started\n");

    memset(psu_info, 0, sizeof(psu_info));
    memset(psu_hwmon_dev_attr, 0, sizeof(psu_hwmon_dev_attr));
    memset(psu_hwmon_attributes, 0, sizeof(psu_hwmon_attributes));
    memset(psu_hwmon_attribute_group, 0, sizeof(psu_hwmon_attribute_group));
    memset(kobj_psu_sub, 0, sizeof(kobj_psu_sub));


    //添加hwmon节点
    for (psu_index = 0; psu_index < bd->psu_num; psu_index++)
    {
        temp_attr_arrary_index = 0;

        temp_attr_index = SENSOR_NAME + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index], "name");
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = IN1_LABEL + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index], "in%d_label", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = IN1_INPUT + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index], "in%d_input", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = CURR1_LABEL + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index], "curr%d_label", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = CURR1_INPUT + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index], "curr%d_input", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = POWER1_LABEL + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index], "power%d_label", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);

        temp_attr_index = POWER1_INPUT + psu_index;
        sprintf(psu_hwmon_attr_name[temp_attr_index], "power%d_input", 1);
        PSU_HWMON_SENSOR_DEV_ATTR(psu_hwmon_attr_name[temp_attr_index], temp_attr_index, (S_IRUGO), bsp_sysfs_psu_hwmon_get_attr, NULL);
        psu_hwmon_attributes[psu_index][temp_attr_arrary_index ++] = &(psu_hwmon_dev_attr[temp_attr_index].dev_attr.attr);


        psu_hwmon_attributes[psu_index][temp_attr_arrary_index] = NULL;    //属性组最后一个属性为NULL
        psu_hwmon_attribute_group[psu_index].attrs = psu_hwmon_attributes[psu_index];

        //添加hwmon节点
        psu_info[psu_index].status = PSU_STATUS_ABSENT;
        psu_info[psu_index].parent_hwmon = hwmon_device_register(NULL);

        if (psu_info[psu_index].parent_hwmon == NULL)
        {
            DBG_ECHO(DEBUG_ERR, "psu %d hwmon register failed!\n", psu_index + 1);
            ret = -EACCES;
            goto exit;
        }

        ret = sysfs_create_group(&(psu_info[psu_index].parent_hwmon->kobj), &(psu_hwmon_attribute_group[psu_index]));

        DBG_ECHO(DEBUG_INFO, "psu %d! hwmon and attribute group created, ret=%d\n", psu_index, ret);


    }


    //添加订制节点
    kobj_psu_root = kobject_create_and_add("psu", kobj_switch);
    if (kobj_psu_root == NULL)
    {
        DBG_ECHO(DEBUG_INFO, "psu custom root node create failed!");
        ret =  -EACCES;
        goto exit;
    }

    CHECK_IF_ERROR_GOTO_EXIT(ret = sysfs_create_group(kobj_psu_root, &psu_customer_group_num_psu), "create num_psu group failed");
    for (psu_index = 0; psu_index < bd->psu_num; psu_index ++)
    {
        sprintf(temp_str, "psu%d", psu_index + 1);
        kobj_psu_sub[psu_index] = kobject_create_and_add(temp_str, kobj_psu_root);
        if (kobj_psu_sub[psu_index] == NULL)
        {
            DBG_ECHO(DEBUG_INFO, "create sub psu node psu%d failed!", psu_index + 1);
            ret =  -EACCES;
            goto exit;
        }
        CHECK_IF_ERROR_GOTO_EXIT(ret = sysfs_create_group(kobj_psu_sub[psu_index], &psu_customer_group),                                 "failed to create psu custome group!");
        psu_info[psu_index].customer_kobj = kobj_psu_sub[psu_index];
        DBG_ECHO(DEBUG_DBG, "psu %d kobj=%p", psu_index, kobj_psu_sub[psu_index])
    }

    //添加debug节点
    kobj_psu_debug = kobject_create_and_add("psu", kobj_debug);
    if (kobj_psu_debug == NULL)
    {
        DBG_ECHO(DEBUG_INFO, "psu debug root node create failed!");
        ret =  -EACCES;
        goto exit;
    }
    CHECK_IF_ERROR_GOTO_EXIT(ret = sysfs_create_group(kobj_psu_debug, &psu_debug_group),                             "failed to create psu debug group!");

    //创建电源监控任务
    INIT_PRINT("psu monitor task creating...");
    psu_monitor_task = kthread_run(h3c_psu_monitor_thread, NULL, "h3c_psu_mon");
    if (psu_monitor_task == NULL)
    {
        DBG_ECHO(DEBUG_INFO, "psu monitor task failed creating!");
        ret =  -EACCES;
        goto exit;
    }


exit:

    if (ret != ERROR_SUCCESS)
    {
        release_all_psu_resource();
    }
    else
    {
        INIT_PRINT("psu module init success!");
    }

    return ret;
}

void release_all_psu_resource(void)
{
    int i = 0;
    board_static_data *bd = bsp_get_board_data();

    if (psu_monitor_task != NULL)
    {
        kthread_stop(psu_monitor_task);
    }
    for (i = 0; i < bd->psu_num; i++)
    {
        if (psu_info[i].parent_hwmon != NULL)
        {
            sysfs_remove_group(&(psu_info[i].parent_hwmon->kobj), &(psu_hwmon_attribute_group[i]));
            hwmon_device_unregister(psu_info[i].parent_hwmon);
            psu_info[i].parent_hwmon = NULL;
        }

        if (kobj_psu_sub[i] != NULL)
        {
            sysfs_remove_group(kobj_psu_sub[i], &psu_customer_group);
            kobject_put(kobj_psu_sub[i]);
        }
    }

    if (kobj_psu_debug != NULL)
    {
        sysfs_remove_group(kobj_psu_debug, &psu_debug_group);
        kobject_put(kobj_psu_debug);
    }

    if (kobj_psu_root != NULL)
    {
        sysfs_remove_group(kobj_psu_root, &psu_customer_group_num_psu);
        kobject_put(kobj_psu_root);
    }
    return;
}

//设置出口函数
static void __exit __psu_exit(void)
{
    //ret = cancel_delayed_work(sysfs_control_psu_wq); //取消工作队列中的工作项
    //flush_workqueue(sysfs_control_psu_wq);           //刷新工作队列
    //destroy_workqueue(sysfs_control_psu_wq);         //销毁工作队列

    release_all_psu_resource();
    INIT_PRINT("module sensor uninstalled !\n");
    return;
}
module_init(__psu_init);
module_exit(__psu_exit);
/*********************************************/
MODULE_AUTHOR("Wan Huan <wan.huan@h3c.com>");
MODULE_DESCRIPTION("h3c system eeprom driver");
MODULE_LICENSE("Dual BSD/GPL");
/*********************************************/
