
/*公有文件引入*/
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>

/*私有文件*/
#include "pub.h"
#include "bsp_base.h"
#include "i2c_dev_reg.h"



#define MODULE_NAME "sensor"


void release_all_sensor_kobj(void);

int sensor_debug_level =  DEBUG_INFO | DEBUG_ERR;

#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(sensor_debug_level, level, fmt, ##args)

enum sensor_type
{
    TEMP_MAX6696,
    TEMP_LM75,
    IN_VR,
    SENSOR_TYPE_BUTT
};

struct temp_sensors
{
    int sensor_type;
    int temp_sensor_index;
    union
    {
        int max6696_index;
        int lm75_index;
        int in_index;
    };
    int spot_index;
    struct kobject * custom_kobj;
};

typedef struct tagDrvSecondaryVolItem
{
    u32 uiSecondVolInput;
    char cSecondVolDesp[30];
    
} DRV_SECONDARY_VOL_ITEM_S;


struct device *hwmon_sensors[MAX_MAX6696_NUM + MAX_LM75_NUM + MAX_ISL68127_NUM] = {NULL};
struct kobject * max6696_kobj[MAX_MAX6696_NUM] = {NULL};   //?kobject?max6696 index????
struct temp_sensors temp_sensors_info[MAX6696_SPOT_NUM * MAX_MAX6696_NUM + MAX_LM75_NUM + MAX_ISL68127_NUM] = {{0}};

int num_temp_sensor = 0;
int test_sensor_temp = -1;    //模拟温度，-1是按实际温度输出

enum sensor_sysfs_attributes
{
    SENSOR_NAME,
    TEMP1_LABEL,
    TEMP1_TYPE,
    TEMP1_MAX,
    TEMP1_MIN,
    TEMP1_CRIT,
    TEMP1_MAX_HYST,
    TEMP1_INPUT,
    TEMP1_ENABLE,
    TEMP2_LABEL,
    TEMP2_TYPE,
    TEMP2_MAX,
    TEMP2_MIN,
    TEMP2_CRIT,
    TEMP2_MAX_HYST,
    TEMP2_INPUT,
    TEMP2_ENABLE,
    TEMP3_LABEL,
    TEMP3_TYPE,
    TEMP3_MAX,
    TEMP3_MIN,
    TEMP3_CRIT,
    TEMP3_MAX_HYST,
    TEMP3_INPUT,
    TEMP3_ENABLE,
    TEST_TEMP,
    SYSFS_ATTR_BUTT
};

enum senosr_custom_attributes
{
    NUM_TEMP_SENSORS = SYSFS_ATTR_BUTT + 1,
    TEMP_ALIAS,
    TEMP_TYPE,
    TEMP_MAX,
    TEMP_MAX_HYST,
    TEMP_MIN,
    TEMP_INPUT
};

/*VR power management chip ISL68127*/
enum senosr_in_attributes
{
    NUM_IN_SENSORS = TEMP_INPUT + 1,
    IN_ALIAS,
    IN_TYPE,
    IN_CRIT,
    IN_MAX,
    IN_INPUT,

    VP1_VOL,
    VP2_VOL,
    VP3_VOL,
    VP4_VOL,
    VH_VOL,
    VX1_VOL,
    VX2_VOL
};

enum enDrv_FT_ADM1166_Chan
{
    ADM1166_VP1,
    ADM1166_VP2,
    ADM1166_VP3,
    ADM1166_VP4,
    ADM1166_VH,
    ADM1166_VX1,
    ADM1166_VX2,
    ADM1166_BUTT
};

static struct kobject *kobj_sensor_root = NULL;
static struct kobject *kobj_sensor_debug = NULL;
static struct kobject *kobj_adm1166 = NULL;


/************************************************************************************/
int bsp_sensor_get_max6696_temp(int max6696_index, int spot_index, s8 *value)
{
    int ret = ERROR_SUCCESS;

    board_static_data *bd = bsp_get_board_data();
    if ((max6696_index >= bd->max6696_num) || (spot_index >= MAX6696_SPOT_NUM))
    {
        DBG_ECHO(DEBUG_ERR, "param error! max6696 index %d, spot_index %d, there's %d max6696!", max6696_index, spot_index, (int)bd->max6696_num);
        return ERROR_FAILED;
    }
    if (lock_i2c_path(I2C_DEV_MAX6696 + max6696_index) == ERROR_SUCCESS)
    {
        ret = bsp_i2c_Max6696_get_temp(bd->i2c_addr_max6696[max6696_index], spot_index, value);
    }
    unlock_i2c_path();
    return ret;
}

//获取/设置max6696各种限值
int bsp_sensor_rw_max6696_limit(REG_RW read_write,int max6696_index, MAX6696_LIMIT_INDEX limit_index, s8 *value)
{
    int ret = ERROR_SUCCESS;

    board_static_data *bd = bsp_get_board_data();
    if ((max6696_index >= bd->max6696_num) || (MAX6696_LIMIT_BUTT <= limit_index))
    {
        DBG_ECHO(DEBUG_ERR, "param error! max6696 index %d, limit_index %d, there's %d max6696!", max6696_index, limit_index, (int)bd->max6696_num);
        return ERROR_FAILED;
    }
    if (lock_i2c_path(I2C_DEV_MAX6696 + max6696_index) == ERROR_SUCCESS)
    {
        ret = bsp_i2c_Max6696_limit_rw(read_write, bd->i2c_addr_max6696[max6696_index], limit_index, value);
    }
    unlock_i2c_path();
    return ret;
}


//找到dev对应的sensors结构体
struct temp_sensors * bsp_sysfs_sensor_get_index_from_kobj(struct kobject *kobj)
{
    int i;
    int sensor_num = bsp_get_board_data()->max6696_num * MAX6696_SPOT_NUM + bsp_get_board_data()->lm75_num + bsp_get_board_data()->isl68127_num; 
    for (i = 0; i < sensor_num; i++)
    {
        if (temp_sensors_info[i].custom_kobj == kobj)
            return &temp_sensors_info[i];
    }
    DBG_ECHO(DEBUG_ERR, "Not found matched sensor index, kobj=%p", kobj);
    return NULL;
}


/************************************************************************************/

static ssize_t bsp_sysfs_max6696_set_attr(struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{ 
    int temp = 0;
    s8  temp_s8 = 0;
    MAX6696_LIMIT_INDEX limit_index = -1;
    int max6696_index = -1;
    int found_match = 0;
    size_t max6696_num = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    board_static_data * bd = bsp_get_board_data();
    max6696_num = bd->max6696_num;

    for (max6696_index = 0; max6696_index < max6696_num; max6696_index++)
    {
        if (attr->index == TEST_TEMP)
        {
            found_match = 1;
            break;
        }

        if ((max6696_kobj[max6696_index] != NULL) && (max6696_kobj[max6696_index] == &(kobjs->kobj)))
        {
            found_match = 1;
            break;
        }
    }
    if (found_match != 1)
    {
        return count;
    }

    switch (attr->index)
    {
        case TEST_TEMP:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected! '-1' is stop testing", buf);
            }
            else
            {
                test_sensor_temp = temp;
            }
            break;
        }
        case TEMP1_MAX:
        case TEMP1_MIN:
        case TEMP2_MAX:
        case TEMP2_MIN:
        case TEMP3_MAX:
        case TEMP3_MIN:
        case TEMP1_CRIT:
        case TEMP2_CRIT:
        case TEMP3_CRIT:
        {
            if (sscanf(buf, "%d", &temp) <= 0)
            {
                DBG_ECHO(DEBUG_INFO, "Format '%s' error, integer expected! '-1' is stop testing", buf);
            }
            else
            {
                temp_s8 = (s8)temp;
                limit_index = attr->index == TEMP1_MAX ? SET_MAX6696_LOCAL_HIGH_ALERT :
                              (attr->index == TEMP2_MAX ? SET_MAX6696_REMOTE_CHANNEL1_HIGH_ALERT:
                               (attr->index == TEMP3_MAX ? SET_MAX6696_REMOTE_CHANNEL2_HIGH_ALERT:
                                (attr->index == TEMP1_MIN ? SET_MAX6696_LOCAL_LOW_ALERT:
                                 (attr->index == TEMP2_MIN ? SET_MAX6696_REMOTE_CHANNEL1_LOW_ALERT:
                                  (attr->index == TEMP3_MIN ? SET_MAX6696_REMOTE_CHANNEL2_LOW_ALERT:
                                   (attr->index == TEMP1_CRIT ? SET_MAX6696_LOCAL_OT2_LIMIT:
                                    (attr->index == TEMP2_CRIT ? SET_MAX6696_REMOTE_CHANNEL1_OT2_LIMIT:
                                     (attr->index == TEMP3_CRIT ? SET_MAX6696_REMOTE_CHANNEL2_OT2_LIMIT:-1))))))));
                if (bsp_sensor_rw_max6696_limit(REG_WRITE, max6696_index, limit_index, &temp_s8) != ERROR_SUCCESS)
                {
                    DBG_ECHO(DEBUG_INFO, "get temp_max for limit_index %d failed!", limit_index);
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


static ssize_t bsp_sysfs_max6696_get_attr(struct device *dev, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int max6696_index = -1;
    int found_match = 0;
    int index = 0;
    s8 temp = 0;
    MAX6696_SPOT_INDEX spot_index = -1;
    MAX6696_LIMIT_INDEX limit_index = -1;
    size_t max6696_num = 0;

    board_static_data * bd = bsp_get_board_data();
    max6696_num = bd->max6696_num;

    for (max6696_index = 0; max6696_index < max6696_num; max6696_index++)
    {
        //按kobject地址确认调用的是哪个max6696的kobject

        if (attr->index == TEST_TEMP)
        {
            found_match = 1;
            break;
        }

        if ((max6696_kobj[max6696_index] != NULL) && (max6696_kobj[max6696_index] == &(dev->kobj)))
        {
            found_match = 1;
            break;
        }
    }

    if (found_match != 1)
    {
        DBG_ECHO(DEBUG_ERR, "not found any matched max6696 kobject!");
        return sprintf(buf, "error, not found any matched max6696 kobject!\n");
    }

    switch (attr->index)
    {
        case SENSOR_NAME:
        {
            index = sprintf(buf, "Max6696_%d\n", max6696_index);
            break;
        }
        case TEMP1_LABEL:
        {
            index = sprintf(buf, "%s\n", bd->max6696_describe[max6696_index][0]);
            break;
        }
        case TEMP2_LABEL:
        {
            index = sprintf(buf, "%s\n", bd->max6696_describe[max6696_index][1]);
            break;
        }
        case TEMP3_LABEL:
        {
            index = sprintf(buf, "%s\n", bd->max6696_describe[max6696_index][2]);
            break;
        }
        case TEMP1_TYPE:
        case TEMP2_TYPE:
        case TEMP3_TYPE:
        {
            index = sprintf(buf, "3"); //thermal diode
            break;
        }
        case TEMP1_MAX:
        case TEMP2_MAX:
        case TEMP3_MAX:
        case TEMP1_MIN:
        case TEMP2_MIN:
        case TEMP3_MIN:
        case TEMP1_CRIT:
        case TEMP2_CRIT:
        case TEMP3_CRIT:
        {
            limit_index = attr->index == TEMP1_MAX ? MAX6696_LOCAL_HIGH_ALERT :
                          (attr->index == TEMP2_MAX ? MAX6696_REMOTE_CHANNEL1_HIGH_ALERT:
                           (attr->index == TEMP3_MAX ? MAX6696_REMOTE_CHANNEL2_HIGH_ALERT :
                            (attr->index == TEMP1_CRIT ? MAX6696_LOCAL_OT2_LIMIT:
                             (attr->index == TEMP2_CRIT ? MAX6696_REMOTE_CHANNEL1_OT2_LIMIT:
                              (attr->index == TEMP3_CRIT ? MAX6696_REMOTE_CHANNEL2_OT2_LIMIT :
                               (attr->index == TEMP1_MIN ? MAX6696_LOCAL_LOW_ALERT:
                                (attr->index == TEMP2_MIN ? MAX6696_REMOTE_CHANNEL1_LOW_ALERT :
                                 (attr->index == TEMP3_MIN ? MAX6696_REMOTE_CHANNEL2_LOW_ALERT : -1))))))));

            if (bsp_sensor_rw_max6696_limit(REG_READ, max6696_index, limit_index, &temp) != ERROR_SUCCESS)
            {
                index = sprintf(buf, "get temp_max for limit_index %d failed!\n", limit_index);
            }
            else
            {
                index = sprintf(buf, "%d\n", (int)temp * 1000);
            }
            break;
        }
        case TEMP1_MAX_HYST:
        {
            index = sprintf(buf, "10000");
            break;
        }
        case TEMP2_MAX_HYST:
        {
            index = sprintf(buf, "10000");
            break;
        }
        case TEMP3_MAX_HYST:
        {
            index = sprintf(buf, "10000");
            break;
        }
        case TEMP1_INPUT:
        case TEMP2_INPUT:
        case TEMP3_INPUT:
        {
            if (test_sensor_temp != -1)
            {
                // for debug temp test
                index = sprintf(buf, "%d\n", test_sensor_temp);
                break;
            }

            spot_index = attr->index == TEMP1_INPUT ? MAX6696_LOCAL_SOPT_INDEX :
                         (attr->index == TEMP2_INPUT ? MAX6696_REMOTE_CHANNEL1_SOPT_INDEX :
                          (attr->index == TEMP3_INPUT ? MAX6696_REMOTE_CHANNEL2_SOPT_INDEX : -1));

            if (bsp_sensor_get_max6696_temp(max6696_index, spot_index, &temp) != ERROR_SUCCESS)
            {
                index = sprintf(buf, "get temp for spot %d failed!\n", spot_index);
            }
            else
            {
                //温度值是有符号数, 可能为负
                index = sprintf(buf, "%d\n", (int)temp * 1000);
            }
            break;
        }
        case TEST_TEMP:
        {
            index = sprintf(buf, "%d\n", test_sensor_temp);
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



static ssize_t bsp_sysfs_temp_sensor_get_attr(struct device *dev, struct device_attribute *da, char *buf)
{
    ssize_t index = 0;
    int ret = ERROR_SUCCESS;
    struct temp_sensors * temp_sensor_info = NULL;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    board_static_data *bd = bsp_get_board_data();
    MAX6696_LIMIT_INDEX limit_index = -1;
    s8 temp = 0;

    if ((attr->index == TEMP_ALIAS) || (attr->index == TEMP_INPUT) || (attr->index == TEMP_TYPE) || (attr->index == TEMP_MAX) || (attr->index == TEMP_MIN) || (attr->index == TEMP_MAX_HYST))
    {
        temp_sensor_info =  bsp_sysfs_sensor_get_index_from_kobj((struct kobject *)dev);
        CHECK_IF_NULL_GOTO_EXIT(ret, temp_sensor_info, "Not found sensor_info by kobject");
    }

    switch(attr->index)
    {
        case NUM_TEMP_SENSORS:
        {
            index = sprintf(buf, "%d", num_temp_sensor);
            break;
        }
        case TEMP_INPUT:
        {
            if (temp_sensor_info->sensor_type == TEMP_MAX6696)
            {
                ret = bsp_sensor_get_max6696_temp(temp_sensor_info->max6696_index,temp_sensor_info->spot_index, &temp);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "get max6696 temp failed max6696 index %d", temp_sensor_info->max6696_index);
                index = sprintf(buf,"%d", (int)temp * 1000);
            }
            else
            {
                index = sprintf(buf, "Not support sensor type %d", temp_sensor_info->sensor_type);
            }
            break;
        }
        case TEMP_TYPE:
        {
            if (temp_sensor_info->sensor_type == TEMP_MAX6696)
            {
                index = sprintf(buf, "%s", "max6696");
            }
            else if (temp_sensor_info->sensor_type == TEMP_LM75)
            {
                index = sprintf(buf, "%s", "lm75");
            }
            else
            {
                index = sprintf(buf, "%s", "unknown");
            }
            break;
        }
        case TEMP_ALIAS:
        {
            if (temp_sensor_info->sensor_type == TEMP_MAX6696)
            {
                index = sprintf(buf, "%s", bd->max6696_describe[temp_sensor_info->max6696_index][temp_sensor_info->spot_index]);
            }
            else if (temp_sensor_info->sensor_type == TEMP_LM75)
            {
                index = sprintf(buf, "%s", bd->lm75_describe[temp_sensor_info->lm75_index]);
            }
            else
            {
                index = sprintf(buf, "unknown");
            }
            break;
        }
        case TEMP_MAX:
        {
            limit_index = temp_sensor_info->spot_index == 0 ? MAX6696_LOCAL_HIGH_ALERT :
                          (temp_sensor_info->spot_index == 1 ? MAX6696_REMOTE_CHANNEL1_HIGH_ALERT:
                           (temp_sensor_info->spot_index == 2 ? MAX6696_REMOTE_CHANNEL2_HIGH_ALERT:-1));
            if (bsp_sensor_rw_max6696_limit(REG_READ, temp_sensor_info->max6696_index, limit_index, &temp) != ERROR_SUCCESS)
            {
                index = sprintf(buf, "get temp_max for limit_index %d failed!\n", limit_index);
            }
            else
            {
                index = sprintf(buf, "%d\n", (int)temp * 1000);
            }
            break;
        }
        case TEMP_MIN:
        {
            limit_index = temp_sensor_info->spot_index == 0 ? MAX6696_LOCAL_LOW_ALERT :
                          (temp_sensor_info->spot_index == 1 ? MAX6696_REMOTE_CHANNEL1_LOW_ALERT:
                           (temp_sensor_info->spot_index == 2 ? MAX6696_REMOTE_CHANNEL2_LOW_ALERT:-1));
            if (bsp_sensor_rw_max6696_limit(REG_READ, temp_sensor_info->max6696_index, limit_index, &temp) != ERROR_SUCCESS)
            {
                index = sprintf(buf, "get temp_max for limit_index %d failed!\n", limit_index);
            }
            else
            {
                index = sprintf(buf, "%d\n", (int)temp * 1000);
            }
            break;
        }
        case TEMP_MAX_HYST:
        {
            index = sprintf(buf, "10000\n");
            break;
        }
        default:
        {
            index = sprintf(buf, "Not support\n");
            break;
        }
    }

exit:

    return index;
}


static SENSOR_DEVICE_ATTR(name, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, SENSOR_NAME);
static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP1_LABEL);
static SENSOR_DEVICE_ATTR(temp1_type, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP1_TYPE);
static SENSOR_DEVICE_ATTR(temp1_max, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP1_MAX);
//static SENSOR_DEVICE_ATTR(temp1_max_hyst, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP1_MAX_HYST);
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP1_INPUT);
//static SENSOR_DEVICE_ATTR(temp1_enable, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP1_ENABLE);
static SENSOR_DEVICE_ATTR(temp2_label, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP2_LABEL);
static SENSOR_DEVICE_ATTR(temp2_type, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP2_TYPE);
static SENSOR_DEVICE_ATTR(temp2_max, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP2_MAX);
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP2_INPUT);
static SENSOR_DEVICE_ATTR(temp3_label, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP3_LABEL);
static SENSOR_DEVICE_ATTR(temp3_type, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP3_TYPE);
static SENSOR_DEVICE_ATTR(temp3_max, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP3_MAX);
static SENSOR_DEVICE_ATTR(temp3_input, S_IRUGO, bsp_sysfs_max6696_get_attr, NULL, TEMP3_INPUT);
static SENSOR_DEVICE_ATTR(temp1_min, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP1_MIN);
static SENSOR_DEVICE_ATTR(temp2_min, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP2_MIN);
static SENSOR_DEVICE_ATTR(temp3_min, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP3_MIN);
static SENSOR_DEVICE_ATTR(temp1_crit, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP1_CRIT);
static SENSOR_DEVICE_ATTR(temp2_crit, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP2_CRIT);
static SENSOR_DEVICE_ATTR(temp3_crit, S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEMP3_CRIT);

//custom node
static SENSOR_DEVICE_ATTR(num_temp_sensors, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, NUM_TEMP_SENSORS);
static SENSOR_DEVICE_ATTR(temp_alias, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_ALIAS);
static SENSOR_DEVICE_ATTR(temp_type, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_TYPE);
static SENSOR_DEVICE_ATTR(temp_min, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_MIN);
static SENSOR_DEVICE_ATTR(temp_max, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_MAX);
static SENSOR_DEVICE_ATTR(temp_input, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_INPUT);
static SENSOR_DEVICE_ATTR(temp_max_hyst, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_MAX_HYST);



static SENSOR_DEVICE_ATTR(test_temp , S_IRUGO|S_IWUSR, bsp_sysfs_max6696_get_attr, bsp_sysfs_max6696_set_attr, TEST_TEMP);


/*VR power management chip ISL68127 attr and attr group start */

static ssize_t bsp_sysfs_in_sensor_get_attr(struct device *dev, struct device_attribute *da, char *buf)
{
    ssize_t index = 0;
	u8 in_index = 0;
    int ret = ERROR_SUCCESS;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	u16 voltage = 0;
	u8 str_buf[10] = {0x00};
    board_static_data *bd = bsp_get_board_data();
	struct temp_sensors * temp_sensor_info = NULL;
/*
    IN_ALIAS,
    IN_TYPE,
    IN_CRIT,
    IN_MAX,
    IN_INPUT
*/
	if(NUM_IN_SENSORS != attr->index)
	{
		temp_sensor_info =  bsp_sysfs_sensor_get_index_from_kobj((struct kobject *)dev);
		CHECK_IF_NULL_GOTO_EXIT(ret, temp_sensor_info, "Not found sensor_info by kobject");
	}
    switch(attr->index)
    {
        case NUM_IN_SENSORS:
        {
            index = sprintf(buf, "%u", 1);
            break;
        }
        case IN_ALIAS:
        {
            index = sprintf(buf, "%s", "VR outlet to Mac chip");
            break;
        }
        case IN_TYPE:
        {
            index = sprintf(buf, "%s", "VR-ISL68127");
            break;
        }
        case IN_CRIT:
        {
#ifndef SUPPORT_FLOAT
            /*w10921 : ISL68127 ????????1000,??????????*/
            snprintf(str_buf, 10, "%s%u", ((bd->mac_rov_min_voltage/1000 == 1)?("1."):("0.")), ((bd->mac_rov_min_voltage/1000 == 1)?(bd->mac_rov_min_voltage-1000):(bd->mac_rov_min_voltage)));
            index = sprintf(buf, "%s", str_buf);
#endif
            break;
        }
        case IN_MAX:
        {
#ifndef SUPPORT_FLOAT
            bd->mac_rov_max_voltage = 1200;
            /*w10921 : ISL68127 ????????1000,??????????*/
            snprintf(str_buf, 10, "%s%u", ((bd->mac_rov_max_voltage/1000 == 1)?("1."):("0.")), ((bd->mac_rov_max_voltage/1000 == 1)?(bd->mac_rov_max_voltage-1000):(bd->mac_rov_max_voltage)));
#endif
            index = sprintf(buf, "%s", str_buf);
            break;
        }
        case IN_INPUT:
        {
            in_index = temp_sensor_info->in_index;
            if (lock_i2c_path(I2C_DEV_ISL68127) == ERROR_SUCCESS)
            {
                /*select page 1*/
                ret = bsp_i2c_isl68127_write_reg(bd->i2c_addr_isl68127[in_index], REG_ADDR_ISL68127_CMD_PAGE, 1, 1);
                if (ret != ERROR_SUCCESS)
                {
                    unlock_i2c_path();
                    index += sprintf(buf + index, "REG_ADDR_ISL68127_CMD_PAGE failed");
                    goto exit;
                }
                /*read voltage*/
                ret = bsp_i2c_isl68127_read_reg(bd->i2c_addr_isl68127[in_index], REG_ADDR_ISL68127_CMD_VOUT, &voltage, 2);
                if (ret != ERROR_SUCCESS)
                {
                    unlock_i2c_path();
                    index += sprintf(buf + index, "REG_ADDR_ISL68127_CMD_VOUT failed");
                    goto exit;
                }
                unlock_i2c_path();
#ifndef SUPPORT_FLOAT
                /*w10921 : ISL68127 ????????1000,??????????*/
                snprintf(str_buf, 10, "%s%u", ((voltage/1000 == 1)?("1."):("0.")), ((voltage/1000 == 1)?(voltage-1000):(voltage)));
#endif

                index = sprintf(buf, "%s", str_buf);
            }
            break;
        }
        default:
        {
            index = sprintf(buf, "Not support");
            break;
        }
    }
exit:

    return index;
}


DRV_SECONDARY_VOL_ITEM_S g_stSecondVol_item[] = 
{
        {5000000,       "VP1"},
        {3300000,       "VP2"},
        {1800000,       "VP3"},
        {1200000,       "VP4"},
        {12000000,      "VH"},
        {800000,        "VX1"},
        {800000,        "VX2"},
            
};
int bsp_get_adm11166_factor(u32 data_type,u32* puiFactor)
{
   DRV_SECONDARY_VOL_ITEM_S *pstSecondaryVolItem = NULL;
   u32 uiSecondaryVolInput = 0;

    if (NULL == puiFactor)
    {
        DBG_ECHO(DEBUG_ERR, "bsp_get_adm11166_factor puiFactor is null");
        return ERROR_FAILED;
    }
   pstSecondaryVolItem = &g_stSecondVol_item[data_type];
   uiSecondaryVolInput = pstSecondaryVolItem->uiSecondVolInput;

   if((uiSecondaryVolInput >= 573000) && (uiSecondaryVolInput <= 1300000))
   {
        *puiFactor = 1000;

   }
   else if((uiSecondaryVolInput > 1300000) && (uiSecondaryVolInput <= 2700000))
   {
        *puiFactor = 2181;

   }
   else if((uiSecondaryVolInput > 2700000) && (uiSecondaryVolInput <= 5500000))
   {
        *puiFactor = 4363;

   }
   else if((uiSecondaryVolInput > 5500000) && (uiSecondaryVolInput <= 14400000))
   {
        *puiFactor = 10472;

   }
   else
   {
      DBG_ECHO(DEBUG_ERR, "uiSecondaryVolInput is not within the range ");
      return ERROR_FAILED;
   }
   return ERROR_SUCCESS;

}


int bsp_adm1166_cal_output_voltage(u32 uiChanNo,u32 uiVolRegValue,u32* puiOutputVol)
{
    u32 ret = ERROR_FAILED;
    u32 uiFactor = 0;
    u64 uiVrefin = 2048;
    u64 ullTemp = 0;

    if(0 == uiVolRegValue)
    {
        *puiOutputVol = 0;
        return ERROR_SUCCESS;
    }
    else if(0xffffffff == uiVolRegValue)
    {
        *puiOutputVol = 0xffffffff;
        return ERROR_FAILED;
    }
    ret = bsp_get_adm11166_factor(uiChanNo,&uiFactor);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "failed to get the adm1166 factor");
    ullTemp = ((u64)uiVolRegValue) * ((u64)uiFactor) * uiVrefin;
    *puiOutputVol = (u32)(ullTemp / (4095 * 1000)) * 1000;
exit:
    return ret;


}

int bsp_adm1166_get_value(int uiChanNo, u32* value)
{
    
    int ret = ERROR_SUCCESS;
    int uiVolRegValue = 0;
    int uiOutputVol = 0;
    board_static_data *bd = bsp_get_board_data();

    
    ret = lock_i2c_path(I2C_DEV_ADM1166);   
    CHECK_IF_ERROR_GOTO_EXIT(ret, "lock adm1166 i2c path failed");
    ret = bsp_get_secondary_voltage_value(bd->i2c_addr_adm1166[0],uiChanNo,&uiVolRegValue);    
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get_secondary_voltage_value failed");
    
    if(0 == uiVolRegValue)
    {
        msleep(100);
        ret = bsp_get_secondary_voltage_value(bd->i2c_addr_adm1166[0], uiChanNo, &uiVolRegValue);    
        CHECK_IF_ERROR_GOTO_EXIT(ret, "get_secondary_voltage_value failed");

    }
    
    ret = bsp_adm1166_cal_output_voltage(uiChanNo, uiVolRegValue, &uiOutputVol);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "bsp_adm1166_cal_output_voltage failed");

    *value = uiOutputVol;
exit:
    unlock_i2c_path();
    return ret;


}

static ssize_t bsp_sysfs_adm1166g_get_attr(struct device *kobj, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    ssize_t index = -EIO;
    int uiChanNo = 0;
    int data = 0;

    switch(attr->index)
    {
        case VP1_VOL:
        {
            uiChanNo = ADM1166_VP1;
            if (bsp_adm1166_get_value(uiChanNo, &data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "adm1166 get VP1_VOL failed!\n");
            }
            else
            {
                index = sprintf(buf, "%d.%03d\n", data/1000,data-(data/1000)*1000);
            }
            break;
        }
        case VP2_VOL:
        {
            uiChanNo = ADM1166_VP2;
            if (bsp_adm1166_get_value(uiChanNo, &data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "adm1166 get VP2_VOL failed!\n");
            }
            else
            {
                index = sprintf(buf, "%d.%03d\n", data/1000,data-(data/1000)*1000);
            }
            break;
        }
        case VP3_VOL:
        {
            uiChanNo = ADM1166_VP3;
            if (bsp_adm1166_get_value(uiChanNo, &data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "adm1166 get VP3_VOL failed!\n");
            }
            else
            {
                index = sprintf(buf, "%d.%03d\n", data/1000,data-(data/1000)*1000);
            }
            break;
        }
        case VP4_VOL:
        {
            uiChanNo = ADM1166_VP4;
            if (bsp_adm1166_get_value(uiChanNo, &data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "adm1166 get VP4_VOL failed!\n");
            }
            else
            {
                index = sprintf(buf, "%d.%03d\n", data/1000,data-(data/1000)*1000);
            }
            break;
        }
        case VH_VOL:
        {
            uiChanNo = ADM1166_VH;
            if (bsp_adm1166_get_value(uiChanNo, &data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "adm1166 get VH_VOL failed!\n");
            }
            else
            {
                index = sprintf(buf, "%d.%03d\n", data / 1000, data - (data / 1000) * 1000);
            }
            break;
        }
        case VX1_VOL:
        {
            uiChanNo = ADM1166_VX1;
            if (bsp_adm1166_get_value(uiChanNo, &data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "adm1166 get VX1_VOL failed!\n");
            }
            else
            {
                index = sprintf(buf, "%d.%03d\n", data/1000,data-(data/1000)*1000);
            }
            break;
        }
        case VX2_VOL:
        {
            uiChanNo = ADM1166_VX2;
            if (bsp_adm1166_get_value(uiChanNo, &data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "adm1166 get VX1_VOL failed!\n");
            }
            else
            {
                index = sprintf(buf, "%d.%03d\n", data / 1000,data - (data / 1000) * 1000);
            }
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

static SENSOR_DEVICE_ATTR(in_num_sensors, S_IRUGO, bsp_sysfs_in_sensor_get_attr, NULL, NUM_IN_SENSORS);
static SENSOR_DEVICE_ATTR(in_alias, S_IRUGO, bsp_sysfs_in_sensor_get_attr, NULL, IN_ALIAS);
static SENSOR_DEVICE_ATTR(in_type, S_IRUGO, bsp_sysfs_in_sensor_get_attr, NULL, IN_TYPE);
static SENSOR_DEVICE_ATTR(in_crit, S_IRUGO, bsp_sysfs_in_sensor_get_attr, NULL, IN_CRIT);
static SENSOR_DEVICE_ATTR(in_max, S_IRUGO, bsp_sysfs_in_sensor_get_attr, NULL, IN_MAX);
static SENSOR_DEVICE_ATTR(in_input, S_IRUGO, bsp_sysfs_in_sensor_get_attr, NULL, IN_INPUT);

static SENSOR_DEVICE_ATTR(s_5v0 , S_IRUGO, bsp_sysfs_adm1166g_get_attr, NULL, VP1_VOL);
static SENSOR_DEVICE_ATTR(mac_3v3 , S_IRUGO, bsp_sysfs_adm1166g_get_attr, NULL, VP2_VOL);
static SENSOR_DEVICE_ATTR(mac_1v8 , S_IRUGO, bsp_sysfs_adm1166g_get_attr, NULL, VP3_VOL);
static SENSOR_DEVICE_ATTR(mac_1v2 , S_IRUGO, bsp_sysfs_adm1166g_get_attr, NULL, VP4_VOL);
static SENSOR_DEVICE_ATTR(vh_12v, S_IRUGO, bsp_sysfs_adm1166g_get_attr, NULL, VH_VOL);
static SENSOR_DEVICE_ATTR(avs_0v8 , S_IRUGO, bsp_sysfs_adm1166g_get_attr, NULL, VX1_VOL);
static SENSOR_DEVICE_ATTR(mac_0v8 , S_IRUGO, bsp_sysfs_adm1166g_get_attr, NULL, VX2_VOL);

static struct attribute *in_sensor_num_attributes[] =
{
    &sensor_dev_attr_in_num_sensors.dev_attr.attr,
    NULL
};
static struct attribute *in_sensor_attributes[] =
{
    &sensor_dev_attr_in_alias.dev_attr.attr,
    &sensor_dev_attr_in_type.dev_attr.attr,
    &sensor_dev_attr_in_crit.dev_attr.attr,
    &sensor_dev_attr_in_max.dev_attr.attr,
    &sensor_dev_attr_in_input.dev_attr.attr,
    NULL
};
static const struct attribute_group in_sensor_num_attributes_group =
{
    .attrs = in_sensor_num_attributes,
};
static const struct attribute_group in_sensor_attributes_group =
{
    .attrs = in_sensor_attributes,
};
/*VR power management chip ISL68127 attr and attr group end */

static struct attribute *max6696_sensor_attributes[] =
{
    &sensor_dev_attr_name.dev_attr.attr,
    &sensor_dev_attr_temp1_label.dev_attr.attr,
    &sensor_dev_attr_temp1_type.dev_attr.attr,
    &sensor_dev_attr_temp1_max.dev_attr.attr,
    &sensor_dev_attr_temp1_min.dev_attr.attr,
    &sensor_dev_attr_temp1_crit.dev_attr.attr,
    &sensor_dev_attr_temp1_input.dev_attr.attr,
    &sensor_dev_attr_temp2_label.dev_attr.attr,
    &sensor_dev_attr_temp2_type.dev_attr.attr,
    &sensor_dev_attr_temp2_max.dev_attr.attr,
    &sensor_dev_attr_temp2_min.dev_attr.attr,
    &sensor_dev_attr_temp2_crit.dev_attr.attr,
    &sensor_dev_attr_temp2_input.dev_attr.attr,
    &sensor_dev_attr_temp3_label.dev_attr.attr,
    &sensor_dev_attr_temp3_type.dev_attr.attr,
    &sensor_dev_attr_temp3_max.dev_attr.attr,
    &sensor_dev_attr_temp3_min.dev_attr.attr,
    &sensor_dev_attr_temp3_crit.dev_attr.attr,
    &sensor_dev_attr_temp3_input.dev_attr.attr,
    NULL
};

static struct attribute *custom_temp_sensor_attributes[] =
{
    &sensor_dev_attr_temp_alias.dev_attr.attr,
    &sensor_dev_attr_temp_type.dev_attr.attr,
    &sensor_dev_attr_temp_min.dev_attr.attr,
    &sensor_dev_attr_temp_max.dev_attr.attr,
    &sensor_dev_attr_temp_max_hyst.dev_attr.attr,
    &sensor_dev_attr_temp_input.dev_attr.attr,
    NULL
};

static struct attribute *max6696_test_attributes[] =
{
    &sensor_dev_attr_test_temp.dev_attr.attr,
    NULL
};

static struct attribute *adm1166_customer_device_attributes[] =
{
    &sensor_dev_attr_s_5v0.dev_attr.attr,
    &sensor_dev_attr_mac_3v3.dev_attr.attr,
    &sensor_dev_attr_mac_1v8.dev_attr.attr,
    &sensor_dev_attr_mac_1v2.dev_attr.attr,
    &sensor_dev_attr_vh_12v.dev_attr.attr,
    &sensor_dev_attr_avs_0v8.dev_attr.attr,
    &sensor_dev_attr_mac_0v8.dev_attr.attr,
    NULL
};

static const struct attribute_group adm1166_customer_group =
{
    .attrs = adm1166_customer_device_attributes
};

static struct attribute *custom_num_temp_sensors_attributes[] =
{
    &sensor_dev_attr_num_temp_sensors.dev_attr.attr,
    NULL
};

static const struct attribute_group max6696_sensor_attribute_group =
{
    .attrs = max6696_sensor_attributes,
};

static const struct attribute_group custom_temp_sensor_attribute_group =
{
    .attrs = custom_temp_sensor_attributes,
};

static const struct attribute_group custom_num_temp_sensor_attribute_group =
{
    .attrs = custom_num_temp_sensors_attributes,
};

static const struct attribute_group max6696_test_attribute_group =
{
    .attrs = max6696_test_attributes,
};
extern ssize_t get_cpu_core_data(unsigned int index, char *buf);
static ssize_t cpu_core_show (struct class *class,struct class_attribute *attr, char *buf)
{
    get_cpu_core_data(0, buf);
    return (ssize_t)strlen(buf);
}

static ssize_t cpu_core_store (struct class *class,struct class_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static CLASS_ATTR_RW(cpu_core);

static struct attribute *h3c_device_class_attrs [] = {
    &class_attr_cpu_core.attr,
    NULL
};

ATTRIBUTE_GROUPS(h3c_device_class);

static struct class h3c_device_class = {
    .name        = "h3c",
    .class_groups = h3c_device_class_groups,
};


//设置初始化入口函数
static int __init sensor_init(void)
{

    int ret = ERROR_SUCCESS;
    int i = 0, j = 0, sensor_index = 0, in_index=0;
    char temp_str[128] = {0};
    board_static_data *bd = bsp_get_board_data();

    INIT_PRINT("sensor module init started\n");
    //static struct class *sensor_class = NULL;
    //static struct device *sensor_dev = NULL;
    memset(hwmon_sensors, 0, sizeof(hwmon_sensors));
    memset(max6696_kobj, 0 , sizeof(max6696_kobj));

    kobj_sensor_root = kobject_create_and_add("sensor", kobj_switch);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_sensor_root, "sensor root kobject create failed");
    ret = sysfs_create_group(kobj_sensor_root, &custom_num_temp_sensor_attribute_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create temp sensor num node failed!");

	/*start  add class h3c cpu */
    ret = class_register (&h3c_device_class);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create class h3c cpu failed!");
	/*end  add class h3c cpu */
    
    //创建SysFS目录
    for (i = 0; i < bd->max6696_num; i++)
    {
        //使用之前重置一次，否则可能会不正常
        bsp_cpld_reset_max6696(i);

        hwmon_sensors[i] = hwmon_device_register(NULL);
        if (hwmon_sensors[i] == NULL)
        {
            CHECK_IF_ERROR_GOTO_EXIT((ret=ERROR_FAILED), "hwmon_device_register failed for %d", i);
        }

        ret = sysfs_create_group(&hwmon_sensors[i]->kobj, &max6696_sensor_attribute_group);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "sysfs crteat group failed!");
        max6696_kobj[i] = &(hwmon_sensors[i]->kobj);

        //创建custom节点
        for (j = 0; j < MAX6696_SPOT_NUM; j++)
        {
            sensor_index = j + i * MAX6696_SPOT_NUM;
            sprintf(temp_str, "temp%d", sensor_index + 1);
            temp_sensors_info[sensor_index].sensor_type = TEMP_MAX6696;
            temp_sensors_info[sensor_index].temp_sensor_index = sensor_index;
            temp_sensors_info[sensor_index].max6696_index = i;
            temp_sensors_info[sensor_index].spot_index = j;
            temp_sensors_info[sensor_index].custom_kobj = kobject_create_and_add(temp_str, kobj_sensor_root);

            ret = sysfs_create_group(temp_sensors_info[sensor_index].custom_kobj, &custom_temp_sensor_attribute_group);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "sysfs crteat custom group failed!");
            
            num_temp_sensor ++;
            
        }
    }
	//??inXX??
	in_index = sensor_index + 1;
	ret = sysfs_create_group(kobj_sensor_root, &in_sensor_num_attributes_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "in_sensor_num_attributes_group create failed!");	
    for (i = 0; i < bd->isl68127_num; i++)
    {
		in_index += i;
		sprintf(temp_str, "in%d", i);
		temp_sensors_info[in_index].sensor_type = IN_VR;
		temp_sensors_info[in_index].temp_sensor_index = in_index;
		temp_sensors_info[in_index].in_index = i;
		temp_sensors_info[in_index].custom_kobj = kobject_create_and_add(temp_str, kobj_sensor_root);
		
		ret = sysfs_create_group(temp_sensors_info[in_index].custom_kobj, &in_sensor_attributes_group);
		CHECK_IF_ERROR_GOTO_EXIT(ret, "sysfs crteat custom group failed!");
    }

    kobj_sensor_debug = kobject_create_and_add("sensor", kobj_debug);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_sensor_debug, "sensor debug kobject created failed!");

    kobj_adm1166 = kobject_create_and_add("adm1166", kobj_sensor_debug);
    if (kobj_adm1166 == NULL)
    {
        DBG_ECHO(DEBUG_INFO, "adm1166 node node create failed!");
        ret =  -EACCES;
        goto exit;
    }   
    CHECK_IF_ERROR_GOTO_EXIT(ret=sysfs_create_group(kobj_adm1166, &adm1166_customer_group), "failed to create adm1166 custome group!");

    ret = sysfs_create_group(kobj_sensor_debug, &max6696_test_attribute_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "sensor debug group created failed!");

exit:
    if (ret != 0)
    {
        DBG_ECHO(DEBUG_ERR, "sensor module init failed!\n");
        release_all_sensor_kobj();
    }
    else
    {
        INIT_PRINT("sensor module finished and success!");
    }

    return ret;
}

void release_all_sensor_kobj(void)
{
    int i = 0;
    
    class_unregister (&h3c_device_class); //add class h3c cpu

    for (i = 0; i < sizeof(hwmon_sensors) / sizeof(hwmon_sensors[0]); i++)
    {
        if (hwmon_sensors[i] != NULL)
        {
            sysfs_remove_group(&(hwmon_sensors[i]->kobj), &max6696_sensor_attribute_group);
            hwmon_device_unregister(hwmon_sensors[i]);
        }
    }

    for (i = 0; i < sizeof(temp_sensors_info) / sizeof(temp_sensors_info[0]); i++)
    {
        if (temp_sensors_info[i].custom_kobj != NULL)
        {
            sysfs_remove_group(temp_sensors_info[i].custom_kobj, &in_sensor_attributes_group);
            kobject_put(temp_sensors_info[i].custom_kobj);
        }
    }

    if(kobj_adm1166 != NULL)
    {
        sysfs_remove_group(kobj_adm1166, &adm1166_customer_group);
        kobject_put(kobj_adm1166);
    }

    if (kobj_sensor_debug != NULL)
    {
        sysfs_remove_group(kobj_sensor_debug, &adm1166_customer_group);
        kobject_put(kobj_sensor_debug);
    }

    if (kobj_sensor_root != NULL)
    {
        sysfs_remove_group(kobj_sensor_root, &custom_num_temp_sensor_attribute_group);
        kobject_put(kobj_sensor_root);
    }

    return;
}

//设置出口函数
static void __exit sensor_exit(void)
{
    release_all_sensor_kobj();
    INIT_PRINT("module sensor uninstalled !\n");
    return;
}


EXPORT_SYMBOL(bsp_sensor_get_max6696_temp);



module_init(sensor_init);
module_exit(sensor_exit);


/*********************************************/
MODULE_AUTHOR("Wan Huan <wan.huan@h3c.com>");
MODULE_DESCRIPTION("h3c system eeprom driver");
MODULE_LICENSE("Dual BSD/GPL");
/*********************************************/
