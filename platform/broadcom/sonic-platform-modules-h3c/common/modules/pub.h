#ifndef _PUB_H_
#define _PUB_H_

#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/rtc.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/version.h>

#define DEBUG_VERSION  0
#define IN
#define OUT
#define ERROR_SUCCESS 0
#define ERROR_FAILED 1

#define DBG_STR0x4       "DEBUG"
#define DBG_STR0x2       "ERROR"
#define DBG_STR0x1       "INFO"
#define LOG_FILE_PATH_0    "/var/log/h3c_log0.txt"
#define LOG_FILE_PATH_1    "/var/log/h3c_log1.txt"

#define LOG_FILE_SIZE      5 * 1024 * 1024  
#define LOG_STRING_LEN     256
#define LOG_LEVEL_DEBUG    0
#define LOG_LEVEL_INFO     1
#define LOG_LEVEL_ERR      2
#define LOG_LEVEL_CRIT                  3
#define MAX_FILE_NAME_LEN               255
#define H3C_SWITCH_RECDBG_LINE_MAX      15
#define H3C_SWITCH_RECDBG_PRE_SEC       300
#define H3C_SWITCH_RECDITEM_LEN_MAX     1024
#define PSU_ALARM_VIN_MIN               90
#define PSU_ALARM_VIN_MAX               290
#define PSU_ALARM_VOUT_MAX              16 
#define PSU_ALARM_TEMP_HIGH             70
#define PSU_ALARM_TEMP_LOWER            0
#define PSU_ALARM_FAN_MIN               3000 

enum DBG_LOG_LEVEL {
    DEBUG_NO       = 0x00,
    DEBUG_ERR      = 0x01,
    DEBUG_WAR      = 0x02,
    DEBUG_INFO     = 0x04,
    DEBUG_DBG      = 0x08,
    DEBUG_BUTT    
};

enum led_color {
    LED_COLOR_DARK = 0,
    LED_COLOR_GREEN,
    LED_COLOR_YELLOW,
    LED_COLOR_RED,
    LED_COLOR_GREEN_FLASH,
    LED_COLOR_YELLOW_FLASH,
    LED_COLOR_RED_FLASH,
    LED_COLOR_BLUE
};

enum psu_status {
    PSU_STATUS_ABSENT = 0,
    PSU_STATUS_NORMAL,
    PSU_STATUS_FAULT,
    PSU_STATUS_UNKNOWN
};

enum fan_status {
    FAN_STATUS_ABSENT = 0,
    FAN_STATUS_NORMAL,
    FAN_STATUS_FAULT,
    FAN_STATUS_UNKNOWN
};

enum slot_status {
    SLOT_STATUS_ABSENT = 0,
    SLOT_STATUS_NORMAL,
    SLOT_STATUS_FAULT,
    SLOT_STATUS_UNKNOWN
};

#define ENABLE      1
#define DISABLE     0

#ifndef TRUE
#define TRUE        1
#endif
#ifndef FALSE
#define FALSE       0
#endif

#define MAIN_BOARD_SLOT_INDEX        -1
#define MAX_SLOT_NUM                  5                                  
#define MAX_OPTIC_PER_SLOT            64                                       
#define MAX_OPTIC_COUNT               (MAX_OPTIC_PER_SLOT * (MAX_SLOT_NUM + 1)) 
#define MAX_PSU_NUM                   5              
#define MAX_INA219_NUM                MAX_PSU_NUM    
#define MAX_FAN_NUM                   8
#define MAX_FAN_MOTER_NUM             4              
#define MAX_EEPROM_PER_SLOT           2
#define MAX_EEPROM_NUM                (MAX_EEPROM_PER_SLOT * (MAX_SLOT_NUM + 1)) 
#define MAX_LM75_NUM_PER_SLOT         2
#define MAX_LM75_NUM                  (MAX_LM75_NUM_PER_SLOT * (MAX_SLOT_NUM + 1)) 
#define MAX_MAX6696_NUM_PER_SLOT      2
#define MAX_MAX6696_NUM               (MAX_MAX6696_NUM_PER_SLOT * (MAX_SLOT_NUM + 1)) 
#define MAX_PHY_NUM_PER_SLOT          16              
#define MAX_CPLD_NUM_PER_SLOT         3
#define MAX_CPLD_NUM                  (MAX_CPLD_NUM_PER_SLOT * (MAX_SLOT_NUM + 1))
#define MAX_ISL68127_NUM              2
#define MAX_ADM1166_NUM               2
#define MAX_CURR_RESEVE_NUM           2
#define MAX_PCA9545_NUM               16
#define MAX_PCA9548_NUM               16
#define PORT_NAME_LEN                 64
#define MAX_SUB_PORT_NUM              4
#define INVALID                       -1

enum product_type {
    PDT_TYPE_S9820_4M_W1 = 1,
    PDT_TYPE_S6850_48Y8C_W1,
    PDT_TYPE_LSW1CGQ32B_W1,
    PDT_TYPE_S9850_32C_W1,   
    PDT_TYPE_BUTT
};

typedef enum {
    REG_READ = 0,
    REG_WRITE,
    REG_BUTT
} REG_RW;

typedef enum 
{
	I2C_DEV_OPTIC_IDX_START = 1 ,                                                    
	I2C_DEV_SLOT0_OPTIC_IDX_START = I2C_DEV_OPTIC_IDX_START + MAX_OPTIC_PER_SLOT,     
	I2C_DEV_OPTIC_BUTT = I2C_DEV_OPTIC_IDX_START + MAX_OPTIC_COUNT,  
	I2C_DEV_EEPROM,
	I2C_DEV_SLOT0_EEPROM = I2C_DEV_EEPROM + MAX_EEPROM_PER_SLOT,            
	I2C_DEV_EEPROM_BUTT =  I2C_DEV_EEPROM + MAX_EEPROM_NUM,
	I2C_DEV_LM75,
	I2C_DEV_SLOT0_LM75 = I2C_DEV_LM75 + MAX_LM75_NUM_PER_SLOT,       
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
	I2C_DEV_BUTT
} I2C_DEVICE_E;

#define GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index)               (I2C_DEV_SLOT0_OPTIC_IDX_START + (slot_index) * MAX_OPTIC_PER_SLOT)                           
#define GET_I2C_DEV_EEPROM_IDX_START_SLOT(slot_index)              (I2C_DEV_SLOT0_EEPROM + (slot_index) * MAX_EEPROM_PER_SLOT)                                    
#define GET_I2C_DEV_LM75_IDX_START_SLOT(slot_index)                (I2C_DEV_SLOT0_LM75 + (slot_index) * MAX_LM75_NUM_PER_SLOT)                                    
#define GET_SLOT_INDEX_BY_I2C_OPTIC_DEV_IDX(i2c_optic_dev_index)   ((i2c_optic_dev_index) < I2C_DEV_SLOT0_OPTIC_IDX_START ? -1 : (((i2c_optic_dev_index) - I2C_DEV_SLOT0_OPTIC_IDX_START) / MAX_OPTIC_PER_SLOT ))       

#define DEBUG_PRINT(switch, level, fmt, args...) \
    do { \
        if ((level == DEBUG_ERR) && (switch & DEBUG_ERR)) { \
            h3c_bsp_dbg_print(__LINE__, __FILE__, (char *)__FUNCTION__, level, fmt, ##args); \
        } else if ((level == DEBUG_WAR) && (switch & DEBUG_WAR)) { \
            h3c_bsp_dbg_print(__LINE__, __FILE__, (char *)__FUNCTION__, level, fmt, ##args); \
        } else if ((level == DEBUG_INFO) && (switch & DEBUG_INFO)) { \
            h3c_bsp_dbg_print(__LINE__, __FILE__, (char *)__FUNCTION__, level, fmt, ##args); \
        } else if ((level == DEBUG_DBG) && (switch & DEBUG_DBG)) { \
            h3c_bsp_dbg_print(__LINE__, __FILE__, (char *)__FUNCTION__, level, fmt, ##args); \
        } else { \
            if ((level < DEBUG_NO) || (level >= DEBUG_BUTT)) \
                h3c_bsp_dbg_print(__LINE__, __FILE__, (char *)__FUNCTION__, DEBUG_INFO, fmt, ##args); \
        } \
    }while(0);

#define SYSLOG(level, fmt, args...) 

#define SYSFS_ATTR_DEF(field) \
	static struct kobj_attribute field = __ATTR(field, S_IRUGO, NULL, NULL);

#define SYSFS_RW_ATTR_DEF(field, _read, _write) \
	static struct kobj_attribute field = __ATTR(field, S_IRUGO|S_IWUSR, _read, _write);

#define SYSFS_RO_ATTR_DEF(field, _read) \
	static struct kobj_attribute field = __ATTR(field, S_IRUGO, _read, NULL);

#define CHECK_CREATE_SYSFS_FILE(kobj_name, attribute, result) \
	(result) = sysfs_create_file((kobj_name), &((attribute).attr));\
    if (ERROR_SUCCESS != result) \
    {DBG_ECHO(DEBUG_ERR, "sysfs create attribute %s failed!", (&((attribute).attr))->name); \
	goto exit;}

#define CHECK_IF_ERROR_GOTO_EXIT(ret, fmt, args...) \
	do {\
		if((ret) != ERROR_SUCCESS) {\
			DBG_ECHO(DEBUG_ERR, fmt, ##args);\
            goto exit;}\
	    } while(0);
            
#define CHECK_IF_NULL_GOTO_EXIT(ret, value, fmt, args...) \
        do {\
            if((value) == NULL) {\
                DBG_ECHO(DEBUG_ERR, fmt, ##args);\
                ret = ERROR_FAILED;\
                goto exit;}\
            } while(0);

#define CHECK_IF_ZERO_GOTO_EXIT(ret, value, fmt, args...) \
            do {\
                if((value) == 0) {\
                    DBG_ECHO(DEBUG_ERR, fmt, ##args);\
                    ret = ERROR_FAILED;\
                    goto exit;}\
                } while(0);
#endif
