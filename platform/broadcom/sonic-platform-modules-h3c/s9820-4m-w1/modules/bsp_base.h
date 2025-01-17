

#ifndef __BSP_BASE_H
#define __BSP_BASE_H

#include "i2c_dev_reg.h"

#define CPLD_MAX_FAN_SPEED_REG_NUM    2      //最大CPLD风扇速度寄存器数量
#define CPLD_MAX_I2C_SELECT_REG_NUM   5      //最大I2C选择寄存器数量,不包括主通道选择
#define MAX_I2C_SEL_OP_STEPS_COUNT    20      //选通一个i2c器件，最多所需要的步骤, 其中包括STEP_OVER一步
#define MAX_I2C_DIAG_RECORD_COUNT     1024   //i2c访问诊断记录条目数


#define I2C_SELECT_STEPS_VALID        1

#define CPLD_FAN_SPEED_LOW_REG_INDEX  0
#define CPLD_FAN_SPEED_HIGH_REG_INDEX 1

#define ONIE_TLV_VALUE_MAX_LEN        255

#define ALL_POWER_ON_AND_PRERSENT (MAX_OPTIC_COUNT + 8)

//数值定义
#define CODE_LED_ON     0x0
#define CODE_LED_OFF    0x1
#define CODE_FAN_ABSENT 0x1
#define CODE_FAN_GOOD   0x3

#define CODE_FAN_MOTER1_GOOD 0x1
#define CODE_FAN_MOTER2_GOOD 0x2
#define CODE_FAN_MOTER_STOP  0xff

#define BSP_LOG_FILETER_RECENT_LOG_COUNT  70

typedef enum
{
    IO_REMAP,     //使用ioremap映射后访问
    IO_INOUT      //使用in/out函数直接访问
} CPLD_ACCESS_TYPE;


typedef enum
{
    CAGE_TYPE_SFP = 1,
    CAGE_TYPE_QSFP,
    CAGE_TYPE_BUTT
} PORT_CAGE_TYPE;

typedef enum
{
    SPEED_25G,
    SPEED_100G,
    SPEED_400G,
    SPEED_BUTT
} PORT_SPEED;

typedef enum
{
    PSU_TYPE_650W,
    PSU_TYPE_1600W,
    PSU_TYPE_BUTT
} PSU_TYPE;

typedef enum
{
    PSU_IN_VOL_TYPE_NO_INPUT = 0,
    PSU_IN_VOL_TYPE_AC,
    PSU_IN_VOL_TYPE_HVDC,
    PSU_IN_VOL_TYPE_UNKNOWN,
    PSU_IN_VOL_TYPE_NOT_SUPPORT,
    PSU_IN_VOL_TYPE_BUTT
} PSU_IN_VOL_TYPE;

//i2c选通操作类型，决定选通时调用哪个函数
typedef enum
{
    OP_TYPE_NONE = 0,
    OP_TYPE_WR_9545,
    OP_TYPE_WR_9548,
    OP_TYPE_WR_CPLD,
    OP_TYPE_BUTT
} i2c_select_operation_type;



typedef struct
{
    i2c_select_operation_type op_type;        //i2c选通操作动作类型
    union
    {
        u16 i2c_dev_addr;                     //操作i2c器件时使用的地址，如9545/9548的器件地址
        u16 cpld_offset_addr;                 //操作cpld时使用的cpld偏移地址
    };
    u8 op_value;                              //往器件里写哪个值, 9545/9548固定往0地址里写，不用选内部地址

} i2c_select_op_step;

//i2c器件选通表
typedef struct
{
    int valid;
    i2c_select_op_step step[MAX_I2C_SEL_OP_STEPS_COUNT];

} i2c_select_operation_steps;

//i2c debug参数设置
struct i2c_debug_info
{
    bool is_valid;
    int dev_path_id;
    bool is_write;
    u8  i2c_addr;
    u16 inner_addr;
    int rw_mode;

    union
    {
        s16 read_len;
        u16 write_value;
    };
};

//i2c smbus访问失败时记录
struct i2c_failed_record
{
    s32 error_code;               //错误码
    u16 path_id;                  //i2c路径id
    u8  read_write;               //操作方向读/写
    u8  protocol;                 //操作方法
    u16 i2c_addr;                 //i2c器件地址
    u16 inner_addr;               //i2c器件内部地址
    s32 retry_times;              //重试次数
    u64 time_sec;                 //自1970:1:1:0:0:0开始秒数
};

struct i2c_diag_records
{
    bool is_valid;
    int rec_count;
    int curr_index;
    struct i2c_failed_record record[MAX_I2C_DIAG_RECORD_COUNT];
};

struct bsp_log_filter
{
    int curr_record_index;
    int used[BSP_LOG_FILETER_RECENT_LOG_COUNT];
    int line_no[BSP_LOG_FILETER_RECENT_LOG_COUNT];
    int hit_count[BSP_LOG_FILETER_RECENT_LOG_COUNT];
    char filename[BSP_LOG_FILETER_RECENT_LOG_COUNT][MAX_FILE_NAME_LEN];
};

//与板卡硬件设计强相关的静态数据。命名方法为"子模块名_属性"
//此结构不存板卡动态数据、硬件设计不相关的数据
typedef struct  tag_board_static_data
{

    int   product_type;                   //板卡/产品类型
    int   slot_index;                     //子卡索引，从0开始，主板为-1
    int   ext_phy_num;                    //板卡外部phy 数量, 与cpld_phy_reset寄存器实际数量一致
    int   initialized;                    //是否已经完成初始化, 通过initialized判断，不通过子卡是否为NULL判断
    char  onie_platform_name[ONIE_TLV_VALUE_MAX_LEN];     //onie平台名称字串，在syseeprom.c中赋值

    size_t fan_num;
    size_t psu_num;
    size_t cpld_num;                      //cpld数量

    PSU_TYPE psu_type;

    size_t slot_num;
    size_t optic_modlue_num;
    size_t lm75_num;
    size_t max6696_num;
    size_t motors_per_fan;                       //每风扇的马达数量
    size_t isl68127_num;
    size_t adm1166_num;                         //adm116数量
    int    fan_speed_coef;                      //转速转换系数，寄存器->真实值
    int    fan_max_speed;                       //风扇最大最小转速
    int    fan_min_speed;
    u8     fan_min_speed_pwm;                   //最低转速时对应的cpld里pwm
    u8     fan_max_speed_pwm;                   //最高转速时对应的cpld里pwm
    u8     fan_min_pwm_speed_percentage;        //pwm最小时风扇转速百分比
    u8     fan_max_pwm_speed_percentage;        //pwm最大时风扇转速百分比
    u16    fan_temp_low;                        //调温曲线的两个限值
    u16    fan_temp_high;

    u16    mac_rov_min_voltage;                //mac芯片供电电压设置, 单位mV
    u16    mac_rov_max_voltage;
    u16    mac_rov_default_voltage;

    int  fan_target_speed_coef0[MAX_FAN_MOTER_NUM];
    int  fan_target_speed_coef1[MAX_FAN_MOTER_NUM];   //风扇标准转速公式系数，从实验数据拟合一个3阶多项式
    int  fan_target_speed_coef2[MAX_FAN_MOTER_NUM];
    int  fan_target_speed_coef3[MAX_FAN_MOTER_NUM];


    size_t eeprom_used_size;                     //使用eeprom，小于eeprom最大空间。为简化设计，可以使用少量空间
    PORT_CAGE_TYPE cage_type[MAX_OPTIC_PER_SLOT];   //端口的CAGE类型，决定使用的是哪个CPLD寄存器
    PORT_SPEED     port_speed[MAX_OPTIC_PER_SLOT];

    int    smbus_use_index;
    u16    i2c_addr_eeprom;     //如果所有板卡eeprom地址均相同，可使用同一个
    u16    i2c_addr_lm75[MAX_LM75_NUM_PER_SLOT];
    char *lm75_describe[MAX_LM75_NUM_PER_SLOT];        //传感器描述信息
    u16    i2c_addr_max6696[MAX_MAX6696_NUM_PER_SLOT];
    char *max6696_describe[MAX_MAX6696_NUM_PER_SLOT][MAX6696_SPOT_NUM];

    u16    i2c_addr_pca9548;
    u16    i2c_addr_pca9548_2;
    u16    i2c_addr_pca9545;
    u16    i2c_addr_psu[MAX_PSU_NUM];         //电源制造信息
    u16    i2c_addr_psu_pmbus[MAX_PSU_NUM];   //电源电压电流信息
    u16    i2c_addr_ina219[MAX_INA219_NUM];
    u16    i2c_addr_fan[MAX_FAN_NUM];
    u16    i2c_addr_isl68127[MAX_ISL68127_NUM];
    u16    i2c_addr_adm1166[MAX_ADM1166_NUM];
    u16    i2c_addr_cpu_vol[MAX_CPU_VOL_NUM];
    u16    i2c_addr_cpu_vr[MAX_CPU_VR_TEMP_NUM];

    u16    i2c_addr_optic_eeprom[MAX_OPTIC_PER_SLOT];        //0x50   生产信息等
    u16    i2c_addr_optic_eeprom_dom[MAX_OPTIC_PER_SLOT];    //0x51   功率信息等

    int cpld_access_type;                //cpld访问方式
    unsigned long cpld_base_address;     //cpld物理基址, 使用IOREMAP方式访问时有用，IOINOUT方式不用
    u16 cpld_hw_addr_board;              //底板cpld
    u16 cpld_size_board;
    u16 cpld_hw_addr_cpu;                //cpu扣板cpld
    u16 cpld_size_cpu;

    char *cpld_type_describe[MAX_CPLD_NUM_PER_SLOT];
    char *cpld_location_describe[MAX_CPLD_NUM_PER_SLOT];

    //cpld setting for sysled led color
    u8 cpld_value_sys_led_code_green;
    u8 cpld_value_sys_led_code_red;
    u8 cpld_value_sys_led_code_yellow;
    u8 cpld_value_sys_led_code_dark;


    //有子卡的设备需要定义
    u16 cpld_hw_addr_slot[MAX_SLOT_NUM];    //子卡cpld地址基址，每个子卡都有cpld，基埴是主板分配的
    u16 cpld_size_slot[MAX_SLOT_NUM];

    u16 cpld_addr_pcb_type;              //product type的cpld偏移地址
    u16 cpld_mask_pcb_type;              //product type的mask
    u16 cpld_offs_pcb_type;              //右移几bit能得到真实数值

    u16 cpld_addr_pcb_ver;              //底板PCB版本号
    u16 cpld_mask_pcb_ver;              //
    u16 cpld_offs_pcb_ver;              //

    u16 cpld_addr_cpld_ver[MAX_CPLD_NUM_PER_SLOT];     //cpld版本
    u16 cpld_mask_cpld_ver[MAX_CPLD_NUM_PER_SLOT];
    u16 cpld_offs_cpld_ver[MAX_CPLD_NUM_PER_SLOT];

    u16 cpld_addr_max6696_rst[MAX_MAX6696_NUM_PER_SLOT];          //max6696 reset寄存器
    u16 cpld_mask_max6696_rst[MAX_MAX6696_NUM_PER_SLOT];
    u16 cpld_offs_max6696_rst[MAX_MAX6696_NUM_PER_SLOT];

    u16 cpld_addr_slot_absent[MAX_SLOT_NUM];        //子卡不存位寄存器
    u16 cpld_mask_slot_absent[MAX_SLOT_NUM];
    u16 cpld_offs_slot_absent[MAX_SLOT_NUM];

    u16 cpld_addr_slot_power_en[MAX_SLOT_NUM];        //子卡上电寄存器
    u16 cpld_mask_slot_power_en[MAX_SLOT_NUM];
    u16 cpld_offs_slot_power_en[MAX_SLOT_NUM];

    u16 cpld_addr_slot_buff_oe1[MAX_SLOT_NUM];        //子卡buff使能1
    u16 cpld_mask_slot_buff_oe1[MAX_SLOT_NUM];
    u16 cpld_offs_slot_buff_oe1[MAX_SLOT_NUM];

    u16 cpld_addr_slot_buff_oe2[MAX_SLOT_NUM];        //子卡buffer使能2
    u16 cpld_mask_slot_buff_oe2[MAX_SLOT_NUM];
    u16 cpld_offs_slot_buff_oe2[MAX_SLOT_NUM];

    u16 cpld_addr_miim_enable;                      //子卡MIIM使能
    u16 cpld_mask_miim_enable;
    u16 cpld_offs_miim_enable;

    u16 cpld_addr_card_power_ok[MAX_SLOT_NUM];     //子卡上电OK寄存器
    u16 cpld_mask_card_power_ok[MAX_SLOT_NUM];
    u16 cpld_offs_card_power_ok[MAX_SLOT_NUM];

    u16 cpld_addr_slot_reset[MAX_SLOT_NUM];        //子卡复位
    u16 cpld_mask_slot_reset[MAX_SLOT_NUM];
    u16 cpld_offs_slot_reset[MAX_SLOT_NUM];


    //eeprom写保护
    u16 cpld_addr_eeprom_write_protect;
    u16 cpld_mask_eeprom_write_protect;
    u16 cpld_offs_eeprom_write_protect;

    //mac_init_ok
    u16 cpld_addr_mac_init_ok;
    u16 cpld_mask_mac_init_ok;
    u16 cpld_offs_mac_init_ok;

    //mac核心电压
    u16 cpld_addr_mac_rov;
    u16 cpld_mask_mac_rov;
    u16 cpld_offs_mac_rov;

    //指示灯相关
    u16 cpld_addr_pannel_sys_led_ctrl;
    u16 cpld_mask_pannel_sys_led_ctrl;
    u16 cpld_offs_pannel_sys_led_ctrl;
    /*
     u16 cpld_addr_pannel_sys_led_green;
     u16 cpld_mask_pannel_sys_led_green;
     u16 cpld_offs_pannel_sys_led_green;

     u16 cpld_addr_pannel_sys_led_red;
     u16 cpld_mask_pannel_sys_led_red;
     u16 cpld_offs_pannel_sys_led_red;
     */
    u16 cpld_addr_pannel_psu_led_green;
    u16 cpld_mask_pannel_psu_led_green;
    u16 cpld_offs_pannel_psu_led_green;

    u16 cpld_addr_pannel_psu_led_red;
    u16 cpld_mask_pannel_psu_led_red;
    u16 cpld_offs_pannel_psu_led_red;

    u16 cpld_addr_pannel_fan_led_green;
    u16 cpld_mask_pannel_fan_led_green;
    u16 cpld_offs_pannel_fan_led_green;

    u16 cpld_addr_pannel_fan_led_red;
    u16 cpld_mask_pannel_fan_led_red;
    u16 cpld_offs_pannel_fan_led_red;

    u16 cpld_addr_pannel_bmc_led_green;
    u16 cpld_mask_pannel_bmc_led_green;
    u16 cpld_offs_pannel_bmc_led_green;

    u16 cpld_addr_pannel_bmc_led_red;
    u16 cpld_mask_pannel_bmc_led_red;
    u16 cpld_offs_pannel_bmc_led_red;

    u16 cpld_addr_pannel_id_led_blue;
    u16 cpld_mask_pannel_id_led_blue;
    u16 cpld_offs_pannel_id_led_blue;

    //主板上的子卡点灯寄存器
    u16 cpld_addr_slot_sysled[MAX_SLOT_NUM];
    u16 cpld_mask_slot_sysled[MAX_SLOT_NUM];
    u16 cpld_offs_slot_sysled[MAX_SLOT_NUM];

    //电源相关寄存器
    u16 cpld_addr_psu_absent[MAX_PSU_NUM];
    u16 cpld_mask_psu_absent[MAX_PSU_NUM];
    u16 cpld_offs_psu_absent[MAX_PSU_NUM];

    u16 cpld_addr_psu_good[MAX_PSU_NUM];
    u16 cpld_mask_psu_good[MAX_PSU_NUM];
    u16 cpld_offs_psu_good[MAX_PSU_NUM];

    //风扇相关寄存器
    u16 cpld_addr_fan_num;
    u16 cpld_mask_fan_num;
    u16 cpld_offs_fan_num;

    u16 cpld_addr_fan_select;
    u16 cpld_mask_fan_select;
    u16 cpld_offs_fan_select;

    u16 cpld_addr_fan_pwm;
    u16 cpld_mask_fan_pwm;
    u16 cpld_offs_fan_pwm;

    u16 cpld_addr_fan_speed[CPLD_MAX_FAN_SPEED_REG_NUM];
    u16 cpld_mask_fan_speed[CPLD_MAX_FAN_SPEED_REG_NUM];
    u16 cpld_offs_fan_speed[CPLD_MAX_FAN_SPEED_REG_NUM];

    u16 cpld_addr_fan_enable[MAX_FAN_NUM];
    u16 cpld_mask_fan_enable[MAX_FAN_NUM];
    u16 cpld_offs_fan_enable[MAX_FAN_NUM];

    u16 cpld_addr_fan_absent[MAX_FAN_NUM];
    u16 cpld_mask_fan_absent[MAX_FAN_NUM];
    u16 cpld_offs_fan_absent[MAX_FAN_NUM];

    u16 cpld_addr_fan_direction[MAX_FAN_NUM];
    u16 cpld_mask_fan_direction[MAX_FAN_NUM];
    u16 cpld_offs_fan_direction[MAX_FAN_NUM];

    u16 cpld_addr_fan_led_red[MAX_FAN_NUM];
    u16 cpld_mask_fan_led_red[MAX_FAN_NUM];
    u16 cpld_offs_fan_led_red[MAX_FAN_NUM];

    u16 cpld_addr_fan_led_green[MAX_FAN_NUM];
    u16 cpld_mask_fan_led_green[MAX_FAN_NUM];
    u16 cpld_offs_fan_led_green[MAX_FAN_NUM];

    u16 cpld_addr_fan_status[MAX_FAN_NUM];
    u16 cpld_mask_fan_status[MAX_FAN_NUM];
    u16 cpld_offs_fan_status[MAX_FAN_NUM];

    //光模块相关寄存器
    //sfp
    u16 cpld_addr_sfp_present[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_sfp_present[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_sfp_present[MAX_OPTIC_PER_SLOT];

    u16 cpld_addr_sfp_tx_dis[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_sfp_tx_dis[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_sfp_tx_dis[MAX_OPTIC_PER_SLOT];

    u16 cpld_addr_sfp_rx_los[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_sfp_rx_los[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_sfp_rx_los[MAX_OPTIC_PER_SLOT];

    u16 cpld_addr_sfp_tx_fault[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_sfp_tx_fault[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_sfp_tx_fault[MAX_OPTIC_PER_SLOT];

    //qsfp
    u16 cpld_addr_qsfp_present[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_qsfp_present[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_qsfp_present[MAX_OPTIC_PER_SLOT];

    u16 cpld_addr_qsfp_lpmode[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_qsfp_lpmode[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_qsfp_lpmode[MAX_OPTIC_PER_SLOT];

    u16 cpld_addr_qsfp_reset[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_qsfp_reset[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_qsfp_reset[MAX_OPTIC_PER_SLOT];

    u16 cpld_addr_qsfp_interrupt[MAX_OPTIC_PER_SLOT];
    u16 cpld_mask_qsfp_interrupt[MAX_OPTIC_PER_SLOT];
    u16 cpld_offs_qsfp_interrupt[MAX_OPTIC_PER_SLOT];

    //给所有光模块上电
    u16 cpld_addr_cage_power_on;
    u16 cpld_mask_cage_power_on;
    u16 cpld_offs_cage_power_on;

    //板卡外部phy复位寄存器
    u16 cpld_addr_phy_reset[MAX_PHY_NUM_PER_SLOT];
    u16 cpld_mask_phy_reset[MAX_PHY_NUM_PER_SLOT];
    u16 cpld_offs_phy_reset[MAX_PHY_NUM_PER_SLOT];

    //i2c主通道选通寄存器
    u16 cpld_addr_main_i2c_sel;
    u16 cpld_mask_main_i2c_sel;
    u16 cpld_offs_main_i2c_sel;

    //除主通道，其它通道选择
    u16 cpld_addr_i2c_sel[CPLD_MAX_I2C_SELECT_REG_NUM];
    u16 cpld_mask_i2c_sel[CPLD_MAX_I2C_SELECT_REG_NUM];
    u16 cpld_offs_i2c_sel[CPLD_MAX_I2C_SELECT_REG_NUM];

    //9548复位寄存器
    u16 cpld_addr_9548_rst[MAX_PCA9548_NUM];
    u16 cpld_mask_9548_rst[MAX_PCA9548_NUM];
    u16 cpld_offs_9548_rst[MAX_PCA9548_NUM];

    //9545复位寄存器
    u16 cpld_addr_9545_rst[MAX_PCA9545_NUM];
    u16 cpld_mask_9545_rst[MAX_PCA9545_NUM];
    u16 cpld_offs_9545_rst[MAX_PCA9545_NUM];

    //cpu复位寄存器
    u16 cpld_addr_cpu_rst;
    u16 cpld_mask_cpu_rst;
    u16 cpld_offs_cpu_rst;

    //watchdog 寄存器
    u16 cpld_addr_wd_feed;
    u16 cpld_mask_wd_feed;
    u16 cpld_offs_wd_feed;

    u16 cpld_addr_wd_disfeed;
    u16 cpld_mask_wd_disfeed;
    u16 cpld_offs_wd_disfeed;

    u16 cpld_addr_wd_timeout;
    u16 cpld_mask_wd_timeout;
    u16 cpld_offs_wd_timeout;

    u16 cpld_addr_wd_enable;
    u16 cpld_mask_wd_enable;
    u16 cpld_offs_wd_enable;

    u16 cpld_addr_reset_type_cpu_thermal;
    u16 cpld_mask_reset_type_cpu_thermal;
    u16 cpld_offs_reset_type_cpu_thermal;

    u16 cpld_addr_reset_type_power_en;
    u16 cpld_mask_reset_type_power_en;
    u16 cpld_offs_reset_type_power_en;

    u16 cpld_addr_reset_type_wdt;
    u16 cpld_mask_reset_type_wdt;
    u16 cpld_offs_reset_type_wdt;

    u16 cpld_addr_reset_type_boot_sw;
    u16 cpld_mask_reset_type_boot_sw;
    u16 cpld_offs_reset_type_boot_sw;

    u16 cpld_addr_reset_type_soft;
    u16 cpld_mask_reset_type_soft;
    u16 cpld_offs_reset_type_soft;

    u16 cpld_addr_reset_type_cold;
    u16 cpld_mask_reset_type_cold;
    u16 cpld_offs_reset_type_cold;

    u16 cpld_addr_reset_type_mlb;
    u16 cpld_mask_reset_type_mlb;
    u16 cpld_offs_reset_type_mlb;

    u16 cpld_addr_clear_reset_flag;
    u16 cpld_mask_clear_reset_flag;
    u16 cpld_offs_clear_reset_flag;

    //for bmc/cpu i2c
    u16 cpld_addr_i2c_wdt_ctrl;
    u16 cpld_mask_i2c_wdt_ctrl;
    u16 cpld_offs_i2c_wdt_ctrl;

    u16 cpld_addr_cpu_init_ok;
    u16 cpld_mask_cpu_init_ok;
    u16 cpld_offs_cpu_init_ok;

    u16 cpld_addr_i2c_wdt_feed;
    u16 cpld_mask_i2c_wdt_feed;
    u16 cpld_offs_i2c_wdt_feed;

    u16 cpld_addr_cpld_smb_sck_reg;
    u16 cpld_mask_cpld_smb_sck_reg;
    u16 cpld_offs_cpld_smb_sck_reg;

    u16 cpld_addr_cpld_buf_enable;
    u16 cpld_mask_cpld_buf_enable;
    u16 cpld_offs_cpld_buf_enable;


    u16 cpld_addr_gpio_i2c_0;
    u16 cpld_mask_gpio_i2c_0;
    u16 cpld_offs_gpio_i2c_0;

    u16 cpld_addr_gpio_i2c_1;
    u16 cpld_mask_gpio_i2c_1;
    u16 cpld_offs_gpio_i2c_1;


    //i2c器件选通表
    i2c_select_operation_steps i2c_select_table[I2C_DEV_BUTT];

    //带子卡设备，子卡相关信息，初始化后为永不为NULL，通过initialized判断是否数据有效
    struct tag_board_static_data *sub_slot_info[MAX_SLOT_NUM];

    //子卡使用，表示所在的设备数据
    struct tag_board_static_data *mainboard;

    u8  power_on[ALL_POWER_ON_AND_PRERSENT];

} board_static_data;



extern struct kobject *kobj_switch;
extern struct kobject *kobj_debug;


extern board_static_data *bsp_get_board_data(void);
extern board_static_data *bsp_get_slot_data(int slot_idx);

extern int bsp_cpld_read_byte(u8 *value, u16 offset);
extern int bsp_cpld_write_byte(u8 value, u16 offset);
extern int bsp_cpld_read_part(OUT u8 *value, IN u16 offset, IN u8 mask, IN u8 shift_bits);
extern int bsp_cpld_write_part(IN u8 value, IN u16 offset, IN u8 mask, IN u8 shift_bits);


extern int bsp_cpld_get_bit(u16 cpld_offset, u8 bit, u8 *value);
extern int bsp_cpld_set_bit(u16 cpld_offset, u8 bit, u8 value);
extern int bsp_cpu_cpld_write_byte(u8 value, u16 offset);
extern int bsp_cpu_cpld_read_byte(u8 *value, u16 offset);
extern int bsp_slot_cpld_read_byte(int slot_index, OUT u8 *value, IN u16 offset);
extern int bsp_slot_cpld_write_byte(int slot_index, IN  u8 value, IN u16 offset);
extern int bsp_slot_cpld_set_bit(int slot_index, u16 cpld_offset, u8 bit, u8 value);
extern int bsp_slot_cpld_get_bit(int slot_index, u16 cpld_offset, u8 bit, u8 *value);

extern size_t bsp_cpld_get_size(void);
extern size_t bsp_get_cpu_cpld_size(void);
extern size_t bsp_cpld_get_slot_size(int slot_index);

extern size_t bsp_print_memory(u8 *in_buf, ssize_t in_buf_len, s8 *out_string_buf, ssize_t out_string_buf_len, unsigned long start_address, unsigned char addr_print_len);


extern int bsp_get_product_type(int *pdt_type);
extern char *bsp_get_product_name_string(int product_type);


extern int  lock_i2c_path(I2C_DEVICE_E i2c_device_index);
extern void unlock_i2c_path(void);
extern int  bsp_h3c_localmsg_to_file(char *buf, long len, int loglevel, const char *src_file, unsigned int line_no);

extern void bsp_h3c_open_init_log(void);
extern void bsp_h3c_close_init_log(void);


extern int bsp_i2c_24LC128_eeprom_read_bytes(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, OUT u8 *data);
extern int bsp_i2c_24LC128_eeprom_write_byte(u16 dev_i2c_address, u16 inner_address, u8 value);
extern int bsp_i2c_common_eeprom_read_bytes(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, OUT u8 *data);
extern int bsp_i2c_common_eeprom_write_byte(u16 dev_i2c_address, u16 inner_address, u8 data);
extern int bsp_i2c_SFP_read_bytes(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, OUT u8 *data);
extern int bsp_i2c_SFP_write_byte(u16 dev_i2c_address, u16 from_inner_address, u8 data);
extern int bsp_i2c_9545_write_byte(u16 dev_i2c_address, u16 inner_address, u8 value);
extern int bsp_i2c_9548_write_byte(u16 dev_i2c_address, u16 inner_address, u8 value);
extern int bsp_i2c_LM75_get_temp(u16 dev_i2c_address, s16 *value);
extern int bsp_i2c_Max6696_get_temp(u16 dev_i2c_address, MAX6696_SPOT_INDEX spot_index, s8 *value);
extern int bsp_i2c_Max6696_reg_read(u16 dev_i2c_address, u16 inner_address, u8 *value);
extern int bsp_i2c_Max6696_reg_write(u16 dev_i2c_address, u16 inner_address, u8 value);
extern int bsp_i2c_Max6696_limit_rw(REG_RW read_write, u16 dev_i2c_address, MAX6696_LIMIT_INDEX limit_index, s8 *value);
extern int bsp_i2c_isl68127_read_reg(u16 dev_i2c_address, u16 command_code, u16 *value, int read_byte_count);
extern int bsp_i2c_isl68127_write_reg(u16 dev_i2c_address, u16 command_code, u16 value, int write_byte_count);



extern void bsp_cpld_reset_max6696(int max_6696_index);

extern int bsp_enable_slot_all_9548(int slot_index);
extern int bsp_enable_slot_all_9545(int slot_index);


extern int bsp_i2c_power650W_read_current(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_voltage(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_temperstatus(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_temper(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_fan_speed(u16 dev_i2c_address, size_t byte_count, int fan_index, u8 *value);
extern int bsp_i2c_power650W_read_SN(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_powerin(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_powerout(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_pdtname(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_vendorname(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_MFR_ID(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_hw_version(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_fw_version(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power_reg_read(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, u8 *value);


/*
extern int bsp_i2c_power650W_read_voltage_in(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power650W_read_current_in(u16 dev_i2c_address, size_t byte_count, u8 *value);
*/


extern int bsp_i2c_power1600W_read_current(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_voltage(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_SN(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_pdtname(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_fan_speed(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_temper(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_powerin(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_powerout(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_temperstatus(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_voltage_in(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_current_in(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_MFR_ID(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_hw_version(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_fw_version(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_voltage_in_type(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power_read_status_word(u16 dev_i2c_address, size_t byte_count, u8 *value);
extern int bsp_i2c_power1600W_read_vendorname(u16 dev_i2c_address, size_t byte_count, u8 *value);


extern int bsp_i2c_ina219_read_reg(u16 dev_i2c_address, u16 inner_address, u16 *value);
extern int bsp_cpld_set_fan_pwm_reg(IN u8 pwm);
extern int bsp_cpld_get_fan_pwm_reg(OUT u8 *pwm);
extern int bsp_cpld_get_fan_speed(OUT u16 *speed, int fan_index, int moter_index);
extern int bsp_cpld_get_fan_enable(OUT u8 *enable, int fan_index);
extern int bsp_cpld_set_fan_enable(OUT u8 enable, int fan_index);
extern int bsp_cpld_get_fan_absent(OUT u8 *absent, int fan_index);
extern int bsp_cpld_get_fan_status(OUT u8 *status, int fan_index);

extern int bsp_cpld_get_fan_led_red(OUT u8 *led, int fan_index);
extern int bsp_cpld_set_fan_led_red(IN u8 led, int fan_index);
extern int bsp_cpld_get_fan_led_green(OUT u8 *led, int fan_index);
extern int bsp_cpld_set_fan_led_green(IN u8 led, int fan_index);



extern int bsp_cpld_get_psu_absent(OUT u8 *absent, int psu_index);
extern int bsp_cpld_get_psu_good(OUT u8 *good, int psu_index);


extern int bsp_cpld_get_slot_absent(OUT u8 *absent, int slot_index);
extern int bsp_cpld_get_card_power_ok(OUT u8 *power_ok, int slot_index);
extern int bsp_get_secondary_voltage_value(u16 dev_i2c_address, int uiChanNo, int *data);
extern int bsp_reset_smbus_slave(int i2c_device_id);
extern void bsp_send_i2c_reset_signal(void);
extern int bsp_reset_smbus_slave(int i2c_device_id);
extern struct mutex bsp_mac_inner_temp_lock;
extern int bsp_i2c_tps53679_write_reg(u16 dev_i2c_address, u16 command_code, u16 value, int write_byte_count);
extern int bsp_i2c_tps53679_read_reg(u16 dev_i2c_address, u16 command_code, u16 *value, int read_byte_count);
extern int bsp_get_optic_eeprom_raw(int slot_index, int port_index, u8 *buf, u8 offset, int count);
extern int bsp_get_optic_dom_raw(int slot_index, int port_index, u8 *buf, u8 offset, int count);
extern int bsp_optical_eeprom_write_byte(int slot_index, int sfp_index, int temp_value, int offset);
extern int bsp_optical_eeprom_write_bytes(int slot_index, int sfp_index, u8 *data, int offset, u16 cnt);
#endif



