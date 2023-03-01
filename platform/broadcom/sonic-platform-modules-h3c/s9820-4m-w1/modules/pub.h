#include <linux/platform_device.h>



//This macro only used by developmet test
#define DEBUG_VERSION  0



#define IN
#define OUT
#define ERROR_SUCCESS 0
#define ERROR_FAILED 1

#define DEBUG_DBG    0x4
#define DEBUG_ERR    0x2
#define DEBUG_INFO   0x1
#define DEBUG_NO     0x0

#define DBG_STR0x4       "DEBUG"
#define DBG_STR0x2       "ERROR"
#define DBG_STR0x1       "INFO"

#define LOG_FILE_PATH_0    "/var/log/h3c_log0.txt"
#define LOG_FILE_PATH_1    "/var/log/h3c_log1.txt"
#define LOG_FILE_SIZE      5 * 1024 * 1024

#define LOG_STRING_LEN   256

#define LOG_LEVEL_DEBUG   0
#define LOG_LEVEL_INFO    1
#define LOG_LEVEL_ERR     2
#define LOG_LEVEL_CRIT    3

#define MAX_FILE_NAME_LEN 255

enum led_color
{
    LED_COLOR_DARK = 0,
    LED_COLOR_GREEN,
    LED_COLOR_YELLOW,
    LED_COLOR_RED,
    LED_COLOR_GREEN_FLASH,
    LED_COLOR_YELLOW_FLASH,
    LED_COLOR_RED_FLASH,
    LED_COLOR_BLUE
};

enum psu_status
{
    PSU_STATUS_ABSENT = 0,
    PSU_STATUS_NORMAL,
    PSU_STATUS_FAULT,
    PSU_STATUS_UNKNOWN
};

enum fan_status
{
    FAN_STATUS_ABSENT = 0,
    FAN_STATUS_NORMAL,
    FAN_STATUS_FAULT,
    FAN_STATUS_UNKNOWN
};


enum slot_status
{
    SLOT_STATUS_ABSENT = 0,
    SLOT_STATUS_NORMAL,
    SLOT_STATUS_FAULT,
    SLOT_STATUS_UNKNOWN
};



#define ENABLE      1
#define DISABLE     0

#define TRUE        1
#define FALSE       0

#define MAIN_BOARD_SLOT_INDEX        -1


#define MAX_SLOT_NUM                  5                                        //设备最大slot 数量, 对于有插卡的设备有意义
#define MAX_OPTIC_PER_SLOT            64                                       //单个子卡/主板最大光模块数量
#define MAX_OPTIC_COUNT               (MAX_OPTIC_PER_SLOT * (MAX_SLOT_NUM + 1))  //整个设备最大光模块数量, 主板有可能也有光模块，算到此数量里

#define MAX_PSU_NUM                   5              //最大电源个数
#define MAX_INA219_NUM                MAX_PSU_NUM    //INA219数量 

#define MAX_FAN_NUM                   8
#define MAX_FAN_MOTER_NUM             4              //一个风扇最多有几个马达

#define MAX_EEPROM_PER_SLOT           2
#define MAX_EEPROM_NUM                (MAX_EEPROM_PER_SLOT * (MAX_SLOT_NUM + 1))  //设备最多几个eeprom

#define MAX_LM75_NUM_PER_SLOT         2
#define MAX_LM75_NUM                  (MAX_LM75_NUM_PER_SLOT * (MAX_SLOT_NUM + 1)) //设备最大LM75数量

#define MAX_MAX6696_NUM_PER_SLOT      2
#define MAX_MAX6696_NUM               (MAX_MAX6696_NUM_PER_SLOT * (MAX_SLOT_NUM + 1)) //每板卡最大6696数量
#define MAX_PHY_NUM_PER_SLOT          16              //每子卡最多多少个外部PHY

#define MAX_CPLD_NUM_PER_SLOT         3
#define MAX_CPLD_NUM                  (MAX_CPLD_NUM_PER_SLOT * (MAX_SLOT_NUM + 1))

#define MAX_ISL68127_NUM             2

#define MAX_ADM1166_NUM              2

#define MAX_CPU_VR_TEMP_NUM         2
#define MAX_CPU_VOL_NUM              3
#define MAX_PCA9545_NUM              16
#define MAX_PCA9548_NUM              16

#define PORT_NAME_LEN               64
#define MAX_SUB_PORT_NUM             4


#define INVALID                      -1


enum product_type
{
    PDT_TYPE_TCS83_120F_4U = 1,
    PDT_TYPE_TCS81_120F_1U,
    PDT_TYPE_TCS83_120F_32H_SUBCARD,
    PDT_TYPE_TCS82_120F_1U,
    PDT_TYPE_BUTT
};

typedef enum
{
    REG_READ = 0,
    REG_WRITE,
    REG_BUTT
} REG_RW;


/**************************************
*
*用于i2c选路，包括子卡和主设备的I2C设备索引
***************************************/

//I2C device index
typedef enum
{
    /*光模块索引开始*/
    I2C_DEV_OPTIC_IDX_START = 1,                                                     //主板光模块开始
    I2C_DEV_SLOT0_OPTIC_IDX_START = I2C_DEV_OPTIC_IDX_START + MAX_OPTIC_PER_SLOT,     //子卡光模块开始
    I2C_DEV_OPTIC_BUTT = I2C_DEV_OPTIC_IDX_START + MAX_OPTIC_COUNT,   //光模块索引结束

    /*除光模块外的其他器件起始*/
    I2C_DEV_EEPROM,
    I2C_DEV_SLOT0_EEPROM = I2C_DEV_EEPROM + MAX_EEPROM_PER_SLOT,             //slot 0 eeprom开始位置
    I2C_DEV_EEPROM_BUTT =  I2C_DEV_EEPROM + MAX_EEPROM_NUM,
    I2C_DEV_LM75,
    I2C_DEV_SLOT0_LM75 = I2C_DEV_LM75 + MAX_LM75_NUM_PER_SLOT,       //slot 0 lm75开始位置
    I2C_DEV_LM75_BUTT =  I2C_DEV_LM75 + MAX_LM75_NUM,
    I2C_DEV_MAX6696,
    I2C_DEV_MAX6696_BUTT = I2C_DEV_MAX6696 + MAX_MAX6696_NUM,
    I2C_DEV_PSU,
    I2C_DEV_PSU_BUTT = I2C_DEV_PSU + MAX_PSU_NUM,
    I2C_DEV_INA219,
    I2C_DEV_INA219_BUTT = I2C_DEV_INA219 + MAX_INA219_NUM,
    I2C_DEV_I350,
    I2C_DEV_FAN,
    I2C_DEV_FAN_BUTT = I2C_DEV_FAN + MAX_FAN_NUM,
    I2C_DEV_ISL68127,
    I2C_DEV_ISL68127_BUTT = I2C_DEV_ISL68127 + MAX_ISL68127_NUM,
    I2C_DEV_ADM1166,
    I2C_DEV_ADM1166_BUTT = I2C_DEV_ADM1166 + MAX_ADM1166_NUM,
    I2C_DEV_CPU_VR,
    I2C_DEV_CPU_VR_BUTT = I2C_DEV_CPU_VR + MAX_CPU_VR_TEMP_NUM,
    I2C_DEV_CPU_VOL,
    I2C_DEV_CPU_VOL_BUTT = I2C_DEV_CPU_VOL + MAX_CPU_VOL_NUM,
    I2C_DEV_BUTT
} I2C_DEVICE_E;


#define GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index)               (I2C_DEV_SLOT0_OPTIC_IDX_START + (slot_index) * MAX_OPTIC_PER_SLOT)                            //子卡光模块索引开始位置
#define GET_I2C_DEV_EEPROM_IDX_START_SLOT(slot_index)              (I2C_DEV_SLOT0_EEPROM + (slot_index) * MAX_EEPROM_PER_SLOT)                                    //子卡EEPROM索引开始位置
#define GET_I2C_DEV_LM75_IDX_START_SLOT(slot_index)                (I2C_DEV_SLOT0_LM75 + (slot_index) * MAX_LM75_NUM_PER_SLOT)                                    //子卡LM75索引开始位置
#define GET_SLOT_INDEX_BY_I2C_OPTIC_DEV_IDX(i2c_optic_dev_index)   ((i2c_optic_dev_index) < I2C_DEV_SLOT0_OPTIC_IDX_START ? -1 : (((i2c_optic_dev_index) - I2C_DEV_SLOT0_OPTIC_IDX_START) / MAX_OPTIC_PER_SLOT ))                   //获取端口所在槽位


typedef struct __iic_dev_write_cmd_data
{
    u16 write_addr;
    u16 write_val;
    u16 access_len;
} iic_dev_write_cmd_data;

typedef struct __iic_dev_read_cmd_data
{
    u16 read_addr;
    u16 access_len;
} iic_dev_read_cmd_data;


/*
*function macro defination
*/
#define DEBUG_PRINT(switch, level, fmt, args...) \
    do {\
        if(((level) & (switch)) != DEBUG_NO) {\
            char * ___temp_str = strrchr(__FILE__, '/');\
            ___temp_str = (___temp_str == NULL) ? __FILE__ : ___temp_str + 1;\
            if ((level) == DEBUG_NO) {\
                printk(KERN_ERR"BSP %s: "fmt, MODULE_NAME,##args);}\
            else {\
                char ___temp_buf[LOG_STRING_LEN] = {0};\
                int ___s = snprintf(___temp_buf, LOG_STRING_LEN - 1, "BSP %s: "fmt, MODULE_NAME,##args);\
                if (bsp_h3c_localmsg_to_file(___temp_buf, (long)___s, level, ___temp_str, __LINE__) != ERROR_SUCCESS)\
                    printk(KERN_ERR"BSP %s: "fmt, MODULE_NAME,##args);}\
            }\
    } while(0);


#define INIT_PRINT(fmt, args...) \
    do {\
        char * ___temp_str = strrchr(__FILE__, '/');\
        ___temp_str = (___temp_str == NULL) ? __FILE__ : ___temp_str + 1;\
        printk(KERN_INFO"BSP %s: "fmt, MODULE_NAME, ##args);\
        } while(0);

#if 0
//no need SYSLOG
#define SYSLOG(level, fmt, args...) \
    do {\
        char ___temp_buf[LOG_STRING_LEN] = {0};\
        int ___s = snprintf(___temp_buf, LOG_STRING_LEN - 1, "BSP: [SYSLOG-%d]:"fmt, level,##args);\
        bsp_h3c_localmsg_to_file(___temp_buf, (long)___s, level, "SYSLOG", __LINE__);\
    } while(0);

#endif
#define SYSLOG(level, fmt, args...)



//just a attribute without rw
#define SYSFS_ATTR_DEF(field) \
    static struct kobj_attribute field = __ATTR(field, S_IRUGO, NULL, NULL);

//read write sysfs
#define SYSFS_RW_ATTR_DEF(field, _read, _write) \
    static struct kobj_attribute field = __ATTR(field, S_IRUGO|S_IWUSR, _read, _write);

//read only sysfs
#define SYSFS_RO_ATTR_DEF(field, _read) \
    static struct kobj_attribute field = __ATTR(field, S_IRUGO, _read, NULL);

//create attribute node, goto label 'exit' if failed.
#define CHECK_CREATE_SYSFS_FILE(kobj_name, attribute, result) \
    (result) = sysfs_create_file((kobj_name), &((attribute).attr));\
    if (ERROR_SUCCESS != result) \
    {DBG_ECHO(DEBUG_ERR, "sysfs create attribute %s failed!", (&((attribute).attr))->name); \
    goto exit;}



#define CHECK_IF_ERROR_GOTO_EXIT(ret, fmt, args...) \
    do {\
        if((ret) != ERROR_SUCCESS) {\
            char * ___temp_str = strrchr(__FILE__, '/');\
            char ___temp_buf[LOG_STRING_LEN] = {0};\
            int ___s = 0;\
            ___temp_str = (___temp_str == NULL) ? __FILE__ : ___temp_str + 1;\
            ___s = snprintf(___temp_buf, LOG_STRING_LEN - 1, "BSP %s: "fmt, MODULE_NAME,##args);\
            if (bsp_h3c_localmsg_to_file(___temp_buf, (long)___s, DEBUG_ERR, ___temp_str, __LINE__) != ERROR_SUCCESS)\
                printk(KERN_ERR"BSP %s: "fmt, MODULE_NAME,##args);\
            goto exit;}\
        } while(0);

#define CHECK_IF_NULL_GOTO_EXIT(ret, value, fmt, args...) \
        do {\
            if((value) == NULL) {\
                char * ___temp_str = strrchr(__FILE__, '/');\
                char ___temp_buf[LOG_STRING_LEN] = {0};\
                int ___s = 0;\
                ___temp_str = (___temp_str == NULL) ? __FILE__ : ___temp_str + 1;\
                ___s = snprintf(___temp_buf, LOG_STRING_LEN - 1, "BSP %s: "fmt, MODULE_NAME,##args);\
                if (bsp_h3c_localmsg_to_file(___temp_buf, (long)___s, DEBUG_ERR, ___temp_str, __LINE__) != ERROR_SUCCESS)\
                    printk(KERN_ERR"BSP %s: "fmt, MODULE_NAME,##args);\
                ret = ERROR_FAILED;\
                goto exit;}\
            } while(0);

#define CHECK_IF_ZERO_GOTO_EXIT(ret, value, fmt, args...) \
            do {\
                if((value) == 0) {\
                    char * ___temp_str = strrchr(__FILE__, '/');\
                    char ___temp_buf[LOG_STRING_LEN] = {0};\
                    int ___s = 0;\
                    ___temp_str = (___temp_str == NULL) ? __FILE__ : ___temp_str + 1;\
                    ___s = snprintf(___temp_buf, LOG_STRING_LEN - 1, "BSP %s: "fmt, MODULE_NAME,##args);\
                    if (bsp_h3c_localmsg_to_file(___temp_buf, (long)___s, DEBUG_ERR, ___temp_str, __LINE__) != ERROR_SUCCESS)\
                        printk(KERN_ERR"BSP %s: "fmt, MODULE_NAME,##args);\
                    ret = ERROR_FAILED;\
                    goto exit;}\
                } while(0);



