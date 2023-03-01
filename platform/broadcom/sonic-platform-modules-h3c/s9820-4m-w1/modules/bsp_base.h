

#ifndef __BSP_BASE_H
#define __BSP_BASE_H

#include "i2c_dev_reg.h"

#define CPLD_MAX_FAN_SPEED_REG_NUM    2      //���CPLD�����ٶȼĴ�������
#define CPLD_MAX_I2C_SELECT_REG_NUM   5      //���I2Cѡ��Ĵ�������,��������ͨ��ѡ��
#define MAX_I2C_SEL_OP_STEPS_COUNT    20      //ѡͨһ��i2c�������������Ҫ�Ĳ���, ���а���STEP_OVERһ��
#define MAX_I2C_DIAG_RECORD_COUNT     1024   //i2c������ϼ�¼��Ŀ��


#define I2C_SELECT_STEPS_VALID        1

#define CPLD_FAN_SPEED_LOW_REG_INDEX  0
#define CPLD_FAN_SPEED_HIGH_REG_INDEX 1

#define ONIE_TLV_VALUE_MAX_LEN        255

#define ALL_POWER_ON_AND_PRERSENT (MAX_OPTIC_COUNT + 8)

//��ֵ����
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
    IO_REMAP,     //ʹ��ioremapӳ������
    IO_INOUT      //ʹ��in/out����ֱ�ӷ���
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

//i2cѡͨ�������ͣ�����ѡͨʱ�����ĸ�����
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
    i2c_select_operation_type op_type;        //i2cѡͨ������������
    union
    {
        u16 i2c_dev_addr;                     //����i2c����ʱʹ�õĵ�ַ����9545/9548��������ַ
        u16 cpld_offset_addr;                 //����cpldʱʹ�õ�cpldƫ�Ƶ�ַ
    };
    u8 op_value;                              //��������д�ĸ�ֵ, 9545/9548�̶���0��ַ��д������ѡ�ڲ���ַ

} i2c_select_op_step;

//i2c����ѡͨ��
typedef struct
{
    int valid;
    i2c_select_op_step step[MAX_I2C_SEL_OP_STEPS_COUNT];

} i2c_select_operation_steps;

//i2c debug��������
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

//i2c smbus����ʧ��ʱ��¼
struct i2c_failed_record
{
    s32 error_code;               //������
    u16 path_id;                  //i2c·��id
    u8  read_write;               //���������/д
    u8  protocol;                 //��������
    u16 i2c_addr;                 //i2c������ַ
    u16 inner_addr;               //i2c�����ڲ���ַ
    s32 retry_times;              //���Դ���
    u64 time_sec;                 //��1970:1:1:0:0:0��ʼ����
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

//��忨Ӳ�����ǿ��صľ�̬���ݡ���������Ϊ"��ģ����_����"
//�˽ṹ����忨��̬���ݡ�Ӳ����Ʋ���ص�����
typedef struct  tag_board_static_data
{

    int   product_type;                   //�忨/��Ʒ����
    int   slot_index;                     //�ӿ���������0��ʼ������Ϊ-1
    int   ext_phy_num;                    //�忨�ⲿphy ����, ��cpld_phy_reset�Ĵ���ʵ������һ��
    int   initialized;                    //�Ƿ��Ѿ���ɳ�ʼ��, ͨ��initialized�жϣ���ͨ���ӿ��Ƿ�ΪNULL�ж�
    char  onie_platform_name[ONIE_TLV_VALUE_MAX_LEN];     //onieƽ̨�����ִ�����syseeprom.c�и�ֵ

    size_t fan_num;
    size_t psu_num;
    size_t cpld_num;                      //cpld����

    PSU_TYPE psu_type;

    size_t slot_num;
    size_t optic_modlue_num;
    size_t lm75_num;
    size_t max6696_num;
    size_t motors_per_fan;                       //ÿ���ȵ��������
    size_t isl68127_num;
    size_t adm1166_num;                         //adm116����
    int    fan_speed_coef;                      //ת��ת��ϵ�����Ĵ���->��ʵֵ
    int    fan_max_speed;                       //���������Сת��
    int    fan_min_speed;
    u8     fan_min_speed_pwm;                   //���ת��ʱ��Ӧ��cpld��pwm
    u8     fan_max_speed_pwm;                   //���ת��ʱ��Ӧ��cpld��pwm
    u8     fan_min_pwm_speed_percentage;        //pwm��Сʱ����ת�ٰٷֱ�
    u8     fan_max_pwm_speed_percentage;        //pwm���ʱ����ת�ٰٷֱ�
    u16    fan_temp_low;                        //�������ߵ�������ֵ
    u16    fan_temp_high;

    u16    mac_rov_min_voltage;                //macоƬ�����ѹ����, ��λmV
    u16    mac_rov_max_voltage;
    u16    mac_rov_default_voltage;

    int  fan_target_speed_coef0[MAX_FAN_MOTER_NUM];
    int  fan_target_speed_coef1[MAX_FAN_MOTER_NUM];   //���ȱ�׼ת�ٹ�ʽϵ������ʵ���������һ��3�׶���ʽ
    int  fan_target_speed_coef2[MAX_FAN_MOTER_NUM];
    int  fan_target_speed_coef3[MAX_FAN_MOTER_NUM];


    size_t eeprom_used_size;                     //ʹ��eeprom��С��eeprom���ռ䡣Ϊ����ƣ�����ʹ�������ռ�
    PORT_CAGE_TYPE cage_type[MAX_OPTIC_PER_SLOT];   //�˿ڵ�CAGE���ͣ�����ʹ�õ����ĸ�CPLD�Ĵ���
    PORT_SPEED     port_speed[MAX_OPTIC_PER_SLOT];

    int    smbus_use_index;
    u16    i2c_addr_eeprom;     //������а忨eeprom��ַ����ͬ����ʹ��ͬһ��
    u16    i2c_addr_lm75[MAX_LM75_NUM_PER_SLOT];
    char *lm75_describe[MAX_LM75_NUM_PER_SLOT];        //������������Ϣ
    u16    i2c_addr_max6696[MAX_MAX6696_NUM_PER_SLOT];
    char *max6696_describe[MAX_MAX6696_NUM_PER_SLOT][MAX6696_SPOT_NUM];

    u16    i2c_addr_pca9548;
    u16    i2c_addr_pca9548_2;
    u16    i2c_addr_pca9545;
    u16    i2c_addr_psu[MAX_PSU_NUM];         //��Դ������Ϣ
    u16    i2c_addr_psu_pmbus[MAX_PSU_NUM];   //��Դ��ѹ������Ϣ
    u16    i2c_addr_ina219[MAX_INA219_NUM];
    u16    i2c_addr_fan[MAX_FAN_NUM];
    u16    i2c_addr_isl68127[MAX_ISL68127_NUM];
    u16    i2c_addr_adm1166[MAX_ADM1166_NUM];
    u16    i2c_addr_cpu_vol[MAX_CPU_VOL_NUM];
    u16    i2c_addr_cpu_vr[MAX_CPU_VR_TEMP_NUM];

    u16    i2c_addr_optic_eeprom[MAX_OPTIC_PER_SLOT];        //0x50   ������Ϣ��
    u16    i2c_addr_optic_eeprom_dom[MAX_OPTIC_PER_SLOT];    //0x51   ������Ϣ��

    int cpld_access_type;                //cpld���ʷ�ʽ
    unsigned long cpld_base_address;     //cpld�����ַ, ʹ��IOREMAP��ʽ����ʱ���ã�IOINOUT��ʽ����
    u16 cpld_hw_addr_board;              //�װ�cpld
    u16 cpld_size_board;
    u16 cpld_hw_addr_cpu;                //cpu�۰�cpld
    u16 cpld_size_cpu;

    char *cpld_type_describe[MAX_CPLD_NUM_PER_SLOT];
    char *cpld_location_describe[MAX_CPLD_NUM_PER_SLOT];

    //cpld setting for sysled led color
    u8 cpld_value_sys_led_code_green;
    u8 cpld_value_sys_led_code_red;
    u8 cpld_value_sys_led_code_yellow;
    u8 cpld_value_sys_led_code_dark;


    //���ӿ����豸��Ҫ����
    u16 cpld_hw_addr_slot[MAX_SLOT_NUM];    //�ӿ�cpld��ַ��ַ��ÿ���ӿ�����cpld����������������
    u16 cpld_size_slot[MAX_SLOT_NUM];

    u16 cpld_addr_pcb_type;              //product type��cpldƫ�Ƶ�ַ
    u16 cpld_mask_pcb_type;              //product type��mask
    u16 cpld_offs_pcb_type;              //���Ƽ�bit�ܵõ���ʵ��ֵ

    u16 cpld_addr_pcb_ver;              //�װ�PCB�汾��
    u16 cpld_mask_pcb_ver;              //
    u16 cpld_offs_pcb_ver;              //

    u16 cpld_addr_cpld_ver[MAX_CPLD_NUM_PER_SLOT];     //cpld�汾
    u16 cpld_mask_cpld_ver[MAX_CPLD_NUM_PER_SLOT];
    u16 cpld_offs_cpld_ver[MAX_CPLD_NUM_PER_SLOT];

    u16 cpld_addr_max6696_rst[MAX_MAX6696_NUM_PER_SLOT];          //max6696 reset�Ĵ���
    u16 cpld_mask_max6696_rst[MAX_MAX6696_NUM_PER_SLOT];
    u16 cpld_offs_max6696_rst[MAX_MAX6696_NUM_PER_SLOT];

    u16 cpld_addr_slot_absent[MAX_SLOT_NUM];        //�ӿ�����λ�Ĵ���
    u16 cpld_mask_slot_absent[MAX_SLOT_NUM];
    u16 cpld_offs_slot_absent[MAX_SLOT_NUM];

    u16 cpld_addr_slot_power_en[MAX_SLOT_NUM];        //�ӿ��ϵ�Ĵ���
    u16 cpld_mask_slot_power_en[MAX_SLOT_NUM];
    u16 cpld_offs_slot_power_en[MAX_SLOT_NUM];

    u16 cpld_addr_slot_buff_oe1[MAX_SLOT_NUM];        //�ӿ�buffʹ��1
    u16 cpld_mask_slot_buff_oe1[MAX_SLOT_NUM];
    u16 cpld_offs_slot_buff_oe1[MAX_SLOT_NUM];

    u16 cpld_addr_slot_buff_oe2[MAX_SLOT_NUM];        //�ӿ�bufferʹ��2
    u16 cpld_mask_slot_buff_oe2[MAX_SLOT_NUM];
    u16 cpld_offs_slot_buff_oe2[MAX_SLOT_NUM];

    u16 cpld_addr_miim_enable;                      //�ӿ�MIIMʹ��
    u16 cpld_mask_miim_enable;
    u16 cpld_offs_miim_enable;

    u16 cpld_addr_card_power_ok[MAX_SLOT_NUM];     //�ӿ��ϵ�OK�Ĵ���
    u16 cpld_mask_card_power_ok[MAX_SLOT_NUM];
    u16 cpld_offs_card_power_ok[MAX_SLOT_NUM];

    u16 cpld_addr_slot_reset[MAX_SLOT_NUM];        //�ӿ���λ
    u16 cpld_mask_slot_reset[MAX_SLOT_NUM];
    u16 cpld_offs_slot_reset[MAX_SLOT_NUM];


    //eepromд����
    u16 cpld_addr_eeprom_write_protect;
    u16 cpld_mask_eeprom_write_protect;
    u16 cpld_offs_eeprom_write_protect;

    //mac_init_ok
    u16 cpld_addr_mac_init_ok;
    u16 cpld_mask_mac_init_ok;
    u16 cpld_offs_mac_init_ok;

    //mac���ĵ�ѹ
    u16 cpld_addr_mac_rov;
    u16 cpld_mask_mac_rov;
    u16 cpld_offs_mac_rov;

    //ָʾ�����
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

    //�����ϵ��ӿ���ƼĴ���
    u16 cpld_addr_slot_sysled[MAX_SLOT_NUM];
    u16 cpld_mask_slot_sysled[MAX_SLOT_NUM];
    u16 cpld_offs_slot_sysled[MAX_SLOT_NUM];

    //��Դ��ؼĴ���
    u16 cpld_addr_psu_absent[MAX_PSU_NUM];
    u16 cpld_mask_psu_absent[MAX_PSU_NUM];
    u16 cpld_offs_psu_absent[MAX_PSU_NUM];

    u16 cpld_addr_psu_good[MAX_PSU_NUM];
    u16 cpld_mask_psu_good[MAX_PSU_NUM];
    u16 cpld_offs_psu_good[MAX_PSU_NUM];

    //������ؼĴ���
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

    //��ģ����ؼĴ���
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

    //�����й�ģ���ϵ�
    u16 cpld_addr_cage_power_on;
    u16 cpld_mask_cage_power_on;
    u16 cpld_offs_cage_power_on;

    //�忨�ⲿphy��λ�Ĵ���
    u16 cpld_addr_phy_reset[MAX_PHY_NUM_PER_SLOT];
    u16 cpld_mask_phy_reset[MAX_PHY_NUM_PER_SLOT];
    u16 cpld_offs_phy_reset[MAX_PHY_NUM_PER_SLOT];

    //i2c��ͨ��ѡͨ�Ĵ���
    u16 cpld_addr_main_i2c_sel;
    u16 cpld_mask_main_i2c_sel;
    u16 cpld_offs_main_i2c_sel;

    //����ͨ��������ͨ��ѡ��
    u16 cpld_addr_i2c_sel[CPLD_MAX_I2C_SELECT_REG_NUM];
    u16 cpld_mask_i2c_sel[CPLD_MAX_I2C_SELECT_REG_NUM];
    u16 cpld_offs_i2c_sel[CPLD_MAX_I2C_SELECT_REG_NUM];

    //9548��λ�Ĵ���
    u16 cpld_addr_9548_rst[MAX_PCA9548_NUM];
    u16 cpld_mask_9548_rst[MAX_PCA9548_NUM];
    u16 cpld_offs_9548_rst[MAX_PCA9548_NUM];

    //9545��λ�Ĵ���
    u16 cpld_addr_9545_rst[MAX_PCA9545_NUM];
    u16 cpld_mask_9545_rst[MAX_PCA9545_NUM];
    u16 cpld_offs_9545_rst[MAX_PCA9545_NUM];

    //cpu��λ�Ĵ���
    u16 cpld_addr_cpu_rst;
    u16 cpld_mask_cpu_rst;
    u16 cpld_offs_cpu_rst;

    //watchdog �Ĵ���
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


    //i2c����ѡͨ��
    i2c_select_operation_steps i2c_select_table[I2C_DEV_BUTT];

    //���ӿ��豸���ӿ������Ϣ����ʼ����Ϊ����ΪNULL��ͨ��initialized�ж��Ƿ�������Ч
    struct tag_board_static_data *sub_slot_info[MAX_SLOT_NUM];

    //�ӿ�ʹ�ã���ʾ���ڵ��豸����
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



