/*公有文件引入*/
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/file.h>
#include <linux/pci.h>
/*私有文件*/
#include "i2c_dev_reg.h"
#include "pub.h"
#include "bsp_base.h"

#define MODULE_NAME "bsp_base"

#define MAX_SMBUS_NUM                 5      //初始化时probe的smbus数量
#define EEPROM_WRITE_MAX_DELAY_MS     15     //E2PROM写操作一个字节delay时间
#define I2C_RW_MAX_RETRY_COUNT        3
#define I2C_RW_MAX_RETRY_DELAY_MS     5
#define EEPROM_RW_MAX_RETRY_COUNT     I2C_RW_MAX_RETRY_COUNT
#define EEPROM_RW_RETRY_INTERVAL      I2C_RW_MAX_RETRY_DELAY_MS      //ms


#define COMMON_EEPROM_BLOCK_READ      0      //eeprom读多个数据时，按块读。还是一个个读
#define SFP_EEPROM_BLOCK_READ         COMMON_EEPROM_BLOCK_READ       //读eeprom时采用按块读

#define DEFAULT_CPLD_BOARD_TYPE_OFFSET    0x2202       //默认的板卡CPLD偏移地址
#define DEFAULT_SUBCARD_BOARD_TYPE_OFFSET 0x2
#define DEFAULT_CPLD_BASE_ADDR_IOREMAP    0xdffe0000   //使用IOREMAP方式访问时，默认的基址
#define DIRECT_IO_SUPPORT                 0  //使用inb/outb访问cpld


/******************struct*************************/

static int bsp_debug_level = DEBUG_ERR|DEBUG_INFO;     //控制debug信息开关,默认打开所有的，发布后再改为只打ERR
/*使用内存方式时才使用*/
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(bsp_debug_level, level, fmt,##args)


void release_all_kobjs(void);


/****************data**********************/

struct mutex bsp_i2c_path_lock;   
struct mutex bsp_cpld_lock;
struct mutex bsp_slot_cpld_lock[MAX_SLOT_NUM];
struct mutex bsp_logfile_lock; 
struct mutex bsp_recent_log_lock; 
struct mutex bsp_fan_speed_lock; 
struct mutex bsp_mac_inner_temp_lock;

char * curr_h3c_log_file = LOG_FILE_PATH_0;


struct i2c_adapter *smbus = NULL;     //使用的i2c smbus
struct i2c_adapter *smbus_probe[MAX_SMBUS_NUM] = {0};

struct kobject *kobj_switch = NULL;         //switch根目录, 客户要求的sysfs结构
struct kobject *kobj_debug  = NULL;         //debug根目录，用于内部debug命令
struct kobject *kobj_debug_i2c = NULL;      //i2c debug node


static int bsp_product_type = PDT_TYPE_BUTT;
volatile void * g_u64Cpld_addr = NULL;
volatile void * g_u64CpuCpld_addr = NULL;
volatile void * g_u64SlotCpld_addr[MAX_SLOT_NUM] = {0};

//板卡形态相关的静态数据.无子卡设备是整个设备数据。有子卡设备表示主板数据
board_static_data main_board_data = {0};

//子卡形态相关静态数据，简化设计使用全局变量，不使用动态分配
board_static_data sub_board_data[MAX_SLOT_NUM]; 


struct i2c_debug_info i2c_debug_info_read = {0};
struct i2c_debug_info i2c_debug_info_write = {0};

struct i2c_diag_records i2c_diag_info = {0};
int current_i2c_path_id = 0;

struct bsp_log_filter bsp_recent_log;
bool log_to_private_file = TRUE;
bool log_filter_to_dmesg = TRUE;
int bsp_dmesg_log_level = DEBUG_ERR;

/*********************************************/





board_static_data * bsp_get_board_data(void)
{
    return &main_board_data;
}

board_static_data * bsp_get_slot_data(int slot_index)
{
    
    board_static_data * bd = bsp_get_board_data();

    if (slot_index == MAIN_BOARD_SLOT_INDEX)
    {
        return bd;
    }
    else if ((slot_index >= 0) && (slot_index < bd->slot_num))
    {
        return bd->sub_slot_info[slot_index]; 
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "slot index %d (0~%d expected) is invalid! return NULL!", slot_index, (int)bd->slot_num);
        return NULL;
    }
}

#define CPLD_REG_ASSIGN(func, addr, mask, offset) \
    bd->cpld_addr_##func = addr;\
    bd->cpld_mask_##func = mask;\
    bd->cpld_offs_##func = offset;


#define CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(value, func) \
    (value) = (((value) & bd->cpld_mask_##func) >> bd->cpld_offs_##func)


#define STEP(type, addr, value)   (type),(addr),(value)
#define STEP_OVER                 (OP_TYPE_NONE),0,0
#define STEP_CNT(n)               (n)


int i2c_select_steps_init(i2c_select_operation_steps * i2c_steps, int step_count, ...)
{
    int i;
    int temp_value;
    va_list steps;
    va_start(steps, step_count);

    if (step_count > MAX_I2C_SEL_OP_STEPS_COUNT)
    {
        DBG_ECHO(DEBUG_ERR, "too many i2c select steps, %d exceeded configured MAX_I2C_SEL_OP_STEPS_COUNT %d. ",step_count,  MAX_I2C_SEL_OP_STEPS_COUNT);
        return -EINVAL;
    }
    for (i = 0; i < step_count; i++)
    {
        temp_value = va_arg(steps, int);
        i2c_steps->step[i].op_type = temp_value;
        if ((u32)temp_value >= OP_TYPE_BUTT)
        {
            DBG_ECHO(DEBUG_ERR, "i2c table init op_type error, %d", temp_value);
            return -EINVAL;
        }
        temp_value = va_arg(steps, int);
        if (i2c_steps->step[i].op_type == OP_TYPE_WR_CPLD)
        {
            i2c_steps->step[i].cpld_offset_addr = (u16)temp_value;
        }
        else
        {
            i2c_steps->step[i].i2c_dev_addr = (u16)temp_value;
        }
        i2c_steps->step[i].op_value = (u8)(va_arg(steps, int));
        
        //DBG_ECHO(DEBUG_DBG, "i2c_step={%d, 0x%4x, 0x%4x}", i2c_steps->step[i].op_type, i2c_steps->step[i].i2c_dev_addr, i2c_steps->step[i].op_value);
    }

    
    va_end(steps);
    i2c_steps->valid = I2C_SELECT_STEPS_VALID;

    
    return ERROR_SUCCESS;
    
}


//打印i2c选通表的内容
size_t bsp_print_i2c_select_table(char * buf)
{

    int i = 0;
    int j = 0;
    int len = 0;
    int per_page = 30;
    static int last_from_index = 0;
    int printed = 0;
    int start_from = last_from_index;
    
    
    //board_static_data * bd = &main_board_data;   
    board_static_data * bd = bsp_get_board_data();
    
    for (i = last_from_index; i < I2C_DEV_BUTT; i++)
    {
        if (bd->i2c_select_table[i].valid == I2C_SELECT_STEPS_VALID)
        {
            len += sprintf(buf + len, "i2c idx %3d:", i);
            for (j = 0; j < MAX_I2C_SEL_OP_STEPS_COUNT; j++)
            {
                if (bd->i2c_select_table[i].step[j].op_type != OP_TYPE_NONE)
                    len += sprintf(buf + len, "{t:%d a:0x%02x v:0x%02x}->", bd->i2c_select_table[i].step[j].op_type,  bd->i2c_select_table[i].step[j].i2c_dev_addr, bd->i2c_select_table[i].step[j].op_value);                    
 
            }
            len += sprintf(buf + len, "None\n");    
            if (++printed >= per_page)
            {
                last_from_index = i + 1;
                break;
            }
        }
    }
    if (printed < per_page)
    {
        last_from_index = 0;
    }
    len += sprintf(buf + len, "\n*t:op_type a:op_address v:value\n");
    len += sprintf(buf + len, "op_type %d:%s\n", OP_TYPE_WR_9545, __stringify(OP_TYPE_WR_9545));
    len += sprintf(buf + len, "op_type %d:%s\n", OP_TYPE_WR_9548, __stringify(OP_TYPE_WR_9548));
    len += sprintf(buf + len, "op_type %d:%s\n", OP_TYPE_WR_CPLD, __stringify(OP_TYPE_WR_CPLD));
    len += sprintf(buf + len, "%d Bytes\nshow %d~%d\n ", len, start_from, last_from_index);
    
    return len;
}



int board_static_data_init_TCS83_120F(board_static_data * board_data)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = board_data;
    //无关数据全写0
    memset(bd, 0, sizeof(board_static_data));

    bd->slot_index = MAIN_BOARD_SLOT_INDEX;
    bd->product_type = PDT_TYPE_TCS83_120F_4U;
    bd->mainboard = NULL;


    bd->cpld_num        = 3;
    bd->isl68127_num    = 1;        //设置MAC核心电压
    bd->adm1166_num     = 2;
    bd->fan_num         = 6;        //风扇个数
    bd->motors_per_fan  = 2;        //每风扇2个马达
    bd->fan_speed_coef  = 30000000; //风扇转速转换系数, 9820是 30000000
    bd->fan_max_speed   = 12000;    
    bd->fan_min_speed   = 1500;
    bd->fan_min_speed_pwm = 0xa;
    bd->fan_max_speed_pwm = 0x64;   //最大转速时对应的pwm值
	bd->fan_min_pwm_speed_percentage = 20;
	bd->fan_max_pwm_speed_percentage = 100;
    bd->fan_temp_low    = 30;
    bd->fan_temp_high   = 60;

    bd->fan_target_speed_coef0[0] = 2338000;          //front fan 4 coeffcient of the polynomial
    bd->fan_target_speed_coef1[0] = -13067;
    bd->fan_target_speed_coef2[0] = 2433;
    bd->fan_target_speed_coef3[0] = -13;

    bd->fan_target_speed_coef0[1] = 2123300;          //rear fan
    bd->fan_target_speed_coef1[1] = -9874;
    bd->fan_target_speed_coef2[1] = 2083;
    bd->fan_target_speed_coef3[1] = -11;
    
    
    bd->psu_num         = 4;        //电源个数
    bd->psu_type        = PSU_TYPE_1600W;
    bd->slot_num        = 4;        //子卡个数, 无子卡写0
    bd->smbus_use_index = 0;        //使用的smbus的索引
    bd->max6696_num     = 2;        //max6696数量
    bd->lm75_num        = 0;
    bd->max6696_num     = 2;
    bd->optic_modlue_num = 0;       //光模块数量, 端口在子卡上
    bd->eeprom_used_size = 512; 

    bd->mac_rov_min_voltage     = 750;    //th3芯片适用
    bd->mac_rov_max_voltage     = 900;
    bd->mac_rov_default_voltage = 893;
        
    bd->ext_phy_num = 0;    //外部phy数量，用于解复位


#if 0
no sfp on mainboard
    for (i = 0; i < bd->optic_modlue_num; i++)
    {
        bd->cage_type[i]  = i < 48 ? CAGE_TYPE_SFP : CAGE_TYPE_QSFP;
        bd->port_speed[i] = i < 48 ? SPEED_25G : SPEED_100G;
    }
#endif 

    bd->i2c_addr_isl68127[0] = 0x5c; 
    bd->i2c_addr_adm1166[0]  = 0x34;
    bd->i2c_addr_adm1166[1]  = 0x35;
    
    bd->i2c_addr_eeprom    = 0x50;  //板卡eerom的i2c地址
    
    bd->i2c_addr_max6696[0]= 0x18;  //max6696 i2c地址
    bd->i2c_addr_max6696[1]= 0x18;  
    bd->max6696_describe[0][0] = "U49_Local";  
    bd->max6696_describe[0][1] = "ASIC_Spot1";
    bd->max6696_describe[0][2] = "ASIC_Spot2";            //left 指从前面板方向
    bd->max6696_describe[1][0] = "U50_Local";
    bd->max6696_describe[1][1] = "ASIC_PCB_Back";
    bd->max6696_describe[1][2] = "ASIC_PCB_Front";

    

    bd->i2c_addr_psu[0] = 0x50;
    bd->i2c_addr_psu[1] = 0x50;
    bd->i2c_addr_psu[2] = 0x50;
    bd->i2c_addr_psu[3] = 0x50;
    bd->i2c_addr_psu_pmbus[0] = 0x58;    //与电源i2c地址配对, +0x08
    bd->i2c_addr_psu_pmbus[1] = 0x58;
    bd->i2c_addr_psu_pmbus[2] = 0x58;    //与电源i2c地址配对, +0x08
    bd->i2c_addr_psu_pmbus[3] = 0x58;

    bd->i2c_addr_fan[0] = 0x50;
    bd->i2c_addr_fan[1] = 0x50;
    bd->i2c_addr_fan[2] = 0x50;
    bd->i2c_addr_fan[3] = 0x50;
    bd->i2c_addr_fan[4] = 0x50;
    bd->i2c_addr_fan[5] = 0x50;

	//cpu vr i2c address
	bd->i2c_addr_cpu_vr[0] = 0x61;
	bd->i2c_addr_cpu_vr[1] = 0x66;

	// cpu	voltage
	bd->i2c_addr_cpu_vol[0] = 0x66;//inner address page 0, 0x8B
	bd->i2c_addr_cpu_vol[1] = 0x61;//inner addess page 0,0x8B
	bd->i2c_addr_cpu_vol[2] = 0x61;//inner address page 1, 0x8B

    bd->cpld_access_type   = IO_REMAP;
    
    bd->cpld_base_address  = 0xdffe0000;   //由OM提供的地址
    bd->cpld_hw_addr_board = 0x2200;       //由硬件设计提供的地址
    bd->cpld_size_board    = 256;          //按字节数
    bd->cpld_hw_addr_cpu   = 0x2000;       //CPU扣板CPLD
    bd->cpld_size_cpu      = 256;          //CPU扣板CPLD


    //子卡cpld 相关数据
    bd->cpld_hw_addr_slot[0] = 0x2400;       //slot 1
    bd->cpld_hw_addr_slot[1] = 0x2480;       //slot 2
    bd->cpld_hw_addr_slot[2] = 0x2600;
    bd->cpld_hw_addr_slot[3] = 0x2680;
    //bd->cpld_hw_addr_slot[4] = 0x0;          //test from 0
    
    bd->cpld_size_slot[0] = 0x80; 
    bd->cpld_size_slot[1] = 0x80;
    bd->cpld_size_slot[2] = 0x80;
    bd->cpld_size_slot[3] = 0x80;
    //bd->cpld_size_slot[4] = 0x80;            //test 0


    CPLD_REG_ASSIGN(pcb_type, 0x02, 0xff, 0);
    CPLD_REG_ASSIGN(pcb_ver, 0x00, 0x0f, 0);

    CPLD_REG_ASSIGN(reset_type_cpu_thermal,    0x20, 0x80, 7);
    CPLD_REG_ASSIGN(reset_type_cold,           0x20, 0x40, 6);
    CPLD_REG_ASSIGN(reset_type_power_en,       0x20, 0x10, 4);
    CPLD_REG_ASSIGN(reset_type_boot_sw,        0x20, 0x08, 3);
    CPLD_REG_ASSIGN(reset_type_soft,           0x20, 0x04, 2);
    CPLD_REG_ASSIGN(reset_type_wdt,            0x20, 0x02, 1);
    CPLD_REG_ASSIGN(reset_type_mlb,            0x20, 0x01, 0);
    
    CPLD_REG_ASSIGN(clear_reset_flag,          0x21, 0x02, 1);
    
 	//watchdog cpld 相关数据
    CPLD_REG_ASSIGN(wd_feed,      0x30, 0xff, 0);
	CPLD_REG_ASSIGN(wd_disfeed,   0x31, 0xff, 0);
	CPLD_REG_ASSIGN(wd_timeout,   0x32, 0xff, 0);
	CPLD_REG_ASSIGN(wd_enable,    0x33, 0x01, 0);

    bd->cpld_type_describe[0] = "XO2 1200";
    bd->cpld_type_describe[1] = "XO3 6900";
    bd->cpld_type_describe[2] = "XO3 6900";

    bd->cpld_location_describe[0] = "1st-JTAG-Chain";
    bd->cpld_location_describe[1] = "2nd-JTAG-Chain";
    bd->cpld_location_describe[2] = "3rd-JTAG-Chain";

    CPLD_REG_ASSIGN(max6696_rst[1],  0x15, 0x20, 5);
    CPLD_REG_ASSIGN(max6696_rst[0],  0x15, 0x10, 4);

    //重置cpu寄存器
    CPLD_REG_ASSIGN(cpu_rst, 0x15, 0x08, 3);
    
    //mac 初始完成可点端口灯
    CPLD_REG_ASSIGN(mac_init_ok,  0x0b, 0x01, 0);

    //mac核心电压设置
    CPLD_REG_ASSIGN(mac_rov,     0x3d, 0xff, 0);


    //面板上的系统指示灯
    /*
      CPLD_REG_ASSIGN(pannel_sys_led_yellow, 0x6c, 0x07, 1);
      CPLD_REG_ASSIGN(pannel_sys_led_green,  0x6c, 0x04, 2);
      CPLD_REG_ASSIGN(pannel_sys_led_red,    0x6c, 0x09, 0);
      */
    CPLD_REG_ASSIGN(pannel_sys_led_ctrl,   0x6c, 0x0f, 0);
    CPLD_REG_ASSIGN(pannel_psu_led_green,  0x6b, 0x40, 6);
    CPLD_REG_ASSIGN(pannel_psu_led_red,    0x6b, 0x80, 7);
    CPLD_REG_ASSIGN(pannel_fan_led_green,  0x6b, 0x04, 2);
    CPLD_REG_ASSIGN(pannel_fan_led_red,    0x6b, 0x08, 3);
    CPLD_REG_ASSIGN(pannel_bmc_led_green,  0x6a, 0x02, 1);
    CPLD_REG_ASSIGN(pannel_bmc_led_red,    0x6a, 0x04, 2);
    CPLD_REG_ASSIGN(pannel_id_led_blue,    0x6a, 0x01, 0);

    //子卡sysled灯
    CPLD_REG_ASSIGN(slot_sysled[0],   0x58, 0x0f, 0);
    CPLD_REG_ASSIGN(slot_sysled[1],   0x59, 0x0f, 0);
    CPLD_REG_ASSIGN(slot_sysled[2],   0x5a, 0x0f, 0);
    CPLD_REG_ASSIGN(slot_sysled[3],   0x5b, 0x0f, 0);
    
    //cpld setting for sysled led color
    bd->cpld_value_sys_led_code_green  = 0xf4;
    bd->cpld_value_sys_led_code_red    = 0xf9;
    bd->cpld_value_sys_led_code_yellow = 0xfe;
    bd->cpld_value_sys_led_code_dark   = 0xff;
    
    CPLD_REG_ASSIGN(cpld_ver[0], 0x01, 0x0f, 0);
    CPLD_REG_ASSIGN(cpld_ver[1], 0x03, 0x0f, 0);
    CPLD_REG_ASSIGN(cpld_ver[2], 0x07, 0x0f, 0);


    //电源相关寄存器
    CPLD_REG_ASSIGN(psu_absent[0],0x35, 0x01, 0);
    CPLD_REG_ASSIGN(psu_absent[1],0x35, 0x02, 1);  
    CPLD_REG_ASSIGN(psu_absent[2],0x35, 0x04, 2);  
    CPLD_REG_ASSIGN(psu_absent[3],0x35, 0x08, 3);  
    
    CPLD_REG_ASSIGN(psu_good[0],  0x34, 0x01, 0);
    CPLD_REG_ASSIGN(psu_good[1],  0x34, 0x02, 1); 
    CPLD_REG_ASSIGN(psu_good[2],  0x34, 0x04, 2);
    CPLD_REG_ASSIGN(psu_good[3],  0x34, 0x08, 3); 

    
    //这里开始是寄存器定义
    //风扇相关寄存器定义
    CPLD_REG_ASSIGN(fan_num,         0x70, 0x0f, 0);
    CPLD_REG_ASSIGN(fan_select,      0x70, 0xf0, 4);
    CPLD_REG_ASSIGN(fan_pwm,         0x71, 0xff, 0);
    CPLD_REG_ASSIGN(fan_speed[CPLD_FAN_SPEED_LOW_REG_INDEX],     0x72, 0xff, 0);
    CPLD_REG_ASSIGN(fan_speed[CPLD_FAN_SPEED_HIGH_REG_INDEX],    0x73, 0xff, 0);
    CPLD_REG_ASSIGN(fan_direction[0],0x74, 0x01, 0);
    CPLD_REG_ASSIGN(fan_direction[1],0x74, 0x02, 1);
    CPLD_REG_ASSIGN(fan_direction[2],0x74, 0x04, 2);
    CPLD_REG_ASSIGN(fan_direction[3],0x74, 0x08, 3);
    CPLD_REG_ASSIGN(fan_direction[4],0x74, 0x10, 4);
    CPLD_REG_ASSIGN(fan_direction[5],0x74, 0x20, 5);
    
    CPLD_REG_ASSIGN(fan_enable[0],   0x75, 0x01, 0);
    CPLD_REG_ASSIGN(fan_enable[1],   0x75, 0x02, 1);
    CPLD_REG_ASSIGN(fan_enable[2],   0x75, 0x04, 2);
    CPLD_REG_ASSIGN(fan_enable[3],   0x75, 0x08, 3);
    CPLD_REG_ASSIGN(fan_enable[4],   0x75, 0x10, 4);
    CPLD_REG_ASSIGN(fan_enable[5],   0x75, 0x20, 5);
    
    CPLD_REG_ASSIGN(fan_led_green[0],  0x76, 0x01, 0);
    CPLD_REG_ASSIGN(fan_led_green[1],  0x76, 0x02, 1);
    CPLD_REG_ASSIGN(fan_led_green[2],  0x76, 0x04, 2);
    CPLD_REG_ASSIGN(fan_led_green[3],  0x76, 0x08, 3);
    CPLD_REG_ASSIGN(fan_led_green[4],  0x76, 0x10, 4);
    CPLD_REG_ASSIGN(fan_led_green[5],  0x76, 0x20, 5);

    CPLD_REG_ASSIGN(fan_led_red[0],   0x7d, 0x01, 0);
    CPLD_REG_ASSIGN(fan_led_red[1],   0x7d, 0x02, 1);
    CPLD_REG_ASSIGN(fan_led_red[2],   0x7d, 0x04, 2);
    CPLD_REG_ASSIGN(fan_led_red[3],   0x7d, 0x08, 3);
    CPLD_REG_ASSIGN(fan_led_red[4],   0x7d, 0x10, 4);
    CPLD_REG_ASSIGN(fan_led_red[5],   0x7d, 0x20, 5);


    CPLD_REG_ASSIGN(fan_absent[0],   0x77, 0x01, 0);
    CPLD_REG_ASSIGN(fan_absent[1],   0x77, 0x02, 1);
    CPLD_REG_ASSIGN(fan_absent[2],   0x77, 0x04, 2);
    CPLD_REG_ASSIGN(fan_absent[3],   0x77, 0x08, 3);
    CPLD_REG_ASSIGN(fan_absent[4],   0x77, 0x10, 4);
    CPLD_REG_ASSIGN(fan_absent[5],   0x77, 0x20, 5);

    CPLD_REG_ASSIGN(fan_status[5],   0x79, 0x30, 4);
    CPLD_REG_ASSIGN(fan_status[4],   0x79, 0x0c, 2);
    CPLD_REG_ASSIGN(fan_status[3],   0x79, 0x03, 0);
    CPLD_REG_ASSIGN(fan_status[2],   0x78, 0x30, 4);
    CPLD_REG_ASSIGN(fan_status[1],   0x78, 0x0c, 2);
    CPLD_REG_ASSIGN(fan_status[0],   0x78, 0x03, 0);
    

    //子卡相关寄存器
    CPLD_REG_ASSIGN(slot_absent[0]  ,0x39, 0x01, 0);
    CPLD_REG_ASSIGN(slot_absent[1]  ,0x39, 0x02, 1);
    CPLD_REG_ASSIGN(slot_absent[2]  ,0x39, 0x04, 2);
    CPLD_REG_ASSIGN(slot_absent[3]  ,0x39, 0x08, 3);

    CPLD_REG_ASSIGN(slot_power_en[0],0x3a, 0x01, 0);
    CPLD_REG_ASSIGN(slot_power_en[1],0x3a, 0x02, 1);
    CPLD_REG_ASSIGN(slot_power_en[2],0x3a, 0x04, 2);
    CPLD_REG_ASSIGN(slot_power_en[3],0x3a, 0x08, 3);

    CPLD_REG_ASSIGN(slot_reset[0],0x17, 0x01, 0);
    CPLD_REG_ASSIGN(slot_reset[1],0x17, 0x02, 1);
    CPLD_REG_ASSIGN(slot_reset[2],0x17, 0x04, 2);
    CPLD_REG_ASSIGN(slot_reset[3],0x17, 0x08, 3);

    CPLD_REG_ASSIGN(slot_buff_oe1[0],0x52, 0x03, 0);
    CPLD_REG_ASSIGN(slot_buff_oe1[1],0x52, 0x0c, 2);
    CPLD_REG_ASSIGN(slot_buff_oe1[2],0x52, 0x30, 4);
    CPLD_REG_ASSIGN(slot_buff_oe1[3],0x52, 0xc0, 6);

    CPLD_REG_ASSIGN(slot_buff_oe2[0],0x53, 0x02, 1);
    CPLD_REG_ASSIGN(slot_buff_oe2[1],0x53, 0x08, 3);
    CPLD_REG_ASSIGN(slot_buff_oe2[2],0x53, 0x20, 5);
    CPLD_REG_ASSIGN(slot_buff_oe2[3],0x53, 0x80, 7);

    CPLD_REG_ASSIGN(card_power_ok[0], 0x3b, 0x01, 0);
    CPLD_REG_ASSIGN(card_power_ok[1], 0x3b, 0x02, 1);
    CPLD_REG_ASSIGN(card_power_ok[2], 0x3b, 0x04, 2);
    CPLD_REG_ASSIGN(card_power_ok[3], 0x3b, 0x08, 3);
        
    CPLD_REG_ASSIGN(miim_enable,     0x56, 0x80, 7);


    CPLD_REG_ASSIGN(i2c_wdt_ctrl, 0x32, 0x0f, 0);
    CPLD_REG_ASSIGN(cpu_init_ok,  0xb,  0x80, 7);
    CPLD_REG_ASSIGN(i2c_wdt_feed, 0x33, 0x01, 0);


    CPLD_REG_ASSIGN(cpld_smb_sck_reg, 0x55, 0x10, 4);
    CPLD_REG_ASSIGN(cpld_buf_enable,  0x55, 0x08, 3);

    CPLD_REG_ASSIGN(gpio_i2c_1, 0x41, 0x01, 0);
    CPLD_REG_ASSIGN(gpio_i2c_0, 0x41, 0x02, 1);

    //i2c选通寄存器
    CPLD_REG_ASSIGN(main_i2c_sel, 0x48, 0xff, 0);     //主通道选择对应的cpld寄存器
    CPLD_REG_ASSIGN(i2c_sel[0],   0x49, 0xff, 0);     //temp fan
    CPLD_REG_ASSIGN(i2c_sel[1],   0x4a, 0xff, 0);     //pwr   vr   
    CPLD_REG_ASSIGN(i2c_sel[2],   0x4b, 0xff, 0);     //mgt eep
    CPLD_REG_ASSIGN(i2c_sel[3],   0x4d, 0xff, 0);     //
    CPLD_REG_ASSIGN(i2c_sel[4],   0x4f, 0xff, 0);     //slot eep

    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_EEPROM + 0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x0), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[2], 0), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_EEPROM + 1]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x0), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[2], 1), STEP_OVER);    

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN + 0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x3), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN + 1]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x2), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN + 2]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x4), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN + 3]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x1), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN + 4]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x5), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN + 5]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x0), STEP_OVER);    

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU + 0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x00), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU + 1]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x40), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU + 2]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x80), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU + 3]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xc0), STEP_OVER);    

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_MAX6696+ 0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x00), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_MAX6696+ 1]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x20), STEP_OVER);    

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_ISL68127 + 0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x4), STEP_OVER);    

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_ADM1166 + 0]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x0), STEP_OVER);    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_ADM1166 + 1]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x0), STEP_OVER);    
    
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VR + 0]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xfe), STEP_OVER);	 
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VR + 1]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xfe), STEP_OVER);

	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 0]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xfe), STEP_OVER);	 
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 1]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xfe), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 2]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xfe), STEP_OVER);

#if 0

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_LM75]),   STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_MAX6696]),STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0), STEP_OVER);

    //光模块选通表
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+0]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+1]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
#endif
    
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "failed for ret=%d!", ret);
    }
    else
    {
        bd->initialized = 1;
    }
    
    return ret;
}



int board_static_data_init_TCS83_120F_32H_subcard(int slot_index, board_static_data* mainboard)
{
    int ret = ERROR_SUCCESS;
    int i = 0;  
    board_static_data * bd  = mainboard->sub_slot_info[slot_index];

    int current_slot_optic_start_index  = GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index);
    int current_slot_eeprom_start_index = GET_I2C_DEV_EEPROM_IDX_START_SLOT(slot_index);
    int current_slot_lm75_start_index   = GET_I2C_DEV_LM75_IDX_START_SLOT(slot_index);
    
    //无关数据全写0
    memset(bd, 0, sizeof(board_static_data));

    bd->slot_index = slot_index;
    bd->product_type = PDT_TYPE_TCS83_120F_32H_SUBCARD;
    bd->mainboard = mainboard;
   
    bd->lm75_num         = 2;
    bd->optic_modlue_num = 32;       //光模块数量
    bd->ext_phy_num = 8;             

    bd->i2c_addr_pca9545   = 0x73;        //这里假设所有9545/9548使用的i2c地址都一样
    bd->i2c_addr_pca9548   = 0x77;   
  
    bd->i2c_addr_eeprom    = 0x50;  //子卡eerom的i2c地址
    bd->eeprom_used_size   = 512;   
    
    bd->i2c_addr_lm75[0]   = 0x48;  //lm75  i2c地址
    bd->i2c_addr_lm75[1]   = 0x49;  //lm75  i2c地址
    
    bd->lm75_describe[0]   = "PHY_left";             //left right 指从前面板方向看
    bd->lm75_describe[1]   = "PHY_right";

    //光模块i2c地址初始化
    for (i = 0; i < bd->optic_modlue_num; i++)
    {
        bd->i2c_addr_optic_eeprom[i]= 0x50;
        bd->i2c_addr_optic_eeprom_dom[i]= 0x50;
        bd->cage_type[i]  = CAGE_TYPE_QSFP;   
        bd->port_speed[i] = SPEED_100G;
    }
    
    CPLD_REG_ASSIGN(pcb_type, 0x02, 0xff, 0);
    CPLD_REG_ASSIGN(pcb_ver,  0x00, 0x07, 0);

    //子卡面板指示灯, 子卡指示灯在主板逻辑上点
    //CPLD_REG_ASSIGN(pannel_sys_led_red,  0x35,  0x80,  7);
    //CPLD_REG_ASSIGN(pannel_sys_led_green,0x35,  0x40,  6);

    //phy复位寄存器
    CPLD_REG_ASSIGN(phy_reset[0]      ,0xd, 0x01, 0);
    CPLD_REG_ASSIGN(phy_reset[1]      ,0xd, 0x02, 1);
    CPLD_REG_ASSIGN(phy_reset[2]      ,0xd, 0x04, 2);
    CPLD_REG_ASSIGN(phy_reset[3]      ,0xd, 0x08, 3);
    CPLD_REG_ASSIGN(phy_reset[4]      ,0xd, 0x10, 4);
    CPLD_REG_ASSIGN(phy_reset[5]      ,0xd, 0x20, 5);
    CPLD_REG_ASSIGN(phy_reset[6]      ,0xd, 0x40, 6);
    CPLD_REG_ASSIGN(phy_reset[7]      ,0xd, 0x80, 7);

    CPLD_REG_ASSIGN(cage_power_on     ,0x25,0x01, 0);

    //i2c选通寄存器
    CPLD_REG_ASSIGN(main_i2c_sel, 0x47, 0xff, 0);     //主通道选择对应的cpld寄存器
    CPLD_REG_ASSIGN(i2c_sel[0],   0x48, 0xff, 0);     //非主通道选择对应的cpld寄存器
    CPLD_REG_ASSIGN(i2c_sel[1],   0x49, 0xff, 0);


    //9545 reset寄存器
    CPLD_REG_ASSIGN(9545_rst[0] ,0x0f, 0x01, 0); 
    CPLD_REG_ASSIGN(9545_rst[1] ,0x0f, 0x02, 1); 
    CPLD_REG_ASSIGN(9545_rst[2] ,0x0f, 0x04, 2); 
    CPLD_REG_ASSIGN(9545_rst[3] ,0x0f, 0x08, 3); 
    CPLD_REG_ASSIGN(9545_rst[4] ,0x0f, 0x10, 4); 
    CPLD_REG_ASSIGN(9545_rst[5] ,0x0f, 0x20, 5); 
    CPLD_REG_ASSIGN(9545_rst[6] ,0x0f, 0x40, 6); 
    CPLD_REG_ASSIGN(9545_rst[7] ,0x0f, 0x80, 7); 
    //eeprom select
    CPLD_REG_ASSIGN(9545_rst[8] ,0x11, 0x01, 0);  
    //9548 reset寄存器
    CPLD_REG_ASSIGN(9548_rst[0] ,0x11, 0x02, 1); 


    //在位信号
    //qsfp, 从0开始
    CPLD_REG_ASSIGN(qsfp_present[0], 0x3b, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_present[1], 0x3b, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_present[2], 0x3b, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_present[3], 0x3b, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_present[4], 0x3b, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_present[5], 0x3b, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_present[6], 0x3b, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_present[7], 0x3b, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_present[8], 0x3c, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_present[9], 0x3c, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_present[10],0x3c, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_present[11],0x3c, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_present[12],0x3c, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_present[13],0x3c, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_present[14],0x3c, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_present[15],0x3c, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_present[16],0x3d, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_present[17],0x3d, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_present[18],0x3d, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_present[19],0x3d, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_present[20],0x3d, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_present[21],0x3d, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_present[22],0x3d, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_present[23],0x3d, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_present[24],0x3e, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_present[25],0x3e, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_present[26],0x3e, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_present[27],0x3e, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_present[28],0x3e, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_present[29],0x3e, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_present[30],0x3e, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_present[31],0x3e, 0x40, 6); 

    //qsfp, 从48开始排续. 数组索引与端口索引一致
    CPLD_REG_ASSIGN(qsfp_interrupt[0],0x47, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_interrupt[1],0x47, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_interrupt[2],0x47, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_interrupt[3],0x47, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_interrupt[4],0x47, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_interrupt[5],0x47, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_interrupt[6],0x47, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_interrupt[7],0x47, 0x40, 6);
    CPLD_REG_ASSIGN(qsfp_interrupt[8], 0x48, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_interrupt[9], 0x48, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_interrupt[10],0x48, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_interrupt[11],0x48, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_interrupt[12],0x48, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_interrupt[13],0x48, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_interrupt[14],0x48, 0x80, 7);  
    CPLD_REG_ASSIGN(qsfp_interrupt[15],0x48, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_interrupt[16],0x49, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_interrupt[17],0x49, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_interrupt[18],0x49, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_interrupt[19],0x49, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_interrupt[20],0x49, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_interrupt[21],0x49, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_interrupt[22],0x49, 0x80, 7);  
    CPLD_REG_ASSIGN(qsfp_interrupt[23],0x49, 0x40, 6);
    CPLD_REG_ASSIGN(qsfp_interrupt[24],0x4a, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_interrupt[25],0x4a, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_interrupt[26],0x4a, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_interrupt[27],0x4a, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_interrupt[28],0x4a, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_interrupt[29],0x4a, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_interrupt[30],0x4a, 0x80, 7);  
    CPLD_REG_ASSIGN(qsfp_interrupt[31],0x4a, 0x40, 6);


    CPLD_REG_ASSIGN(qsfp_lpmode[0], 0x3f, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_lpmode[1], 0x3f, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_lpmode[2], 0x3f, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_lpmode[3], 0x3f, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_lpmode[4], 0x3f, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_lpmode[5], 0x3f, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_lpmode[6], 0x3f, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_lpmode[7], 0x3f, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_lpmode[8], 0x40, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_lpmode[9], 0x40, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_lpmode[10],0x40, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_lpmode[11],0x40, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_lpmode[12],0x40, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_lpmode[13],0x40, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_lpmode[14],0x40, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_lpmode[15],0x40, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_lpmode[16],0x41, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_lpmode[17],0x41, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_lpmode[18],0x41, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_lpmode[19],0x41, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_lpmode[20],0x41, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_lpmode[21],0x41, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_lpmode[22],0x41, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_lpmode[23],0x41, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_lpmode[24],0x42, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_lpmode[25],0x42, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_lpmode[26],0x42, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_lpmode[27],0x42, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_lpmode[28],0x42, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_lpmode[29],0x42, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_lpmode[30],0x42, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_lpmode[31],0x42, 0x40, 6); 


    CPLD_REG_ASSIGN(qsfp_reset[0], 0x43, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_reset[1], 0x43, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_reset[2], 0x43, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_reset[3], 0x43, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_reset[4], 0x43, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_reset[5], 0x43, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_reset[6], 0x43, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_reset[7], 0x43, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_reset[8], 0x44, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_reset[9], 0x44, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_reset[10],0x44, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_reset[11],0x44, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_reset[12],0x44, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_reset[13],0x44, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_reset[14],0x44, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_reset[15],0x44, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_reset[16],0x45, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_reset[17],0x45, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_reset[18],0x45, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_reset[19],0x45, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_reset[20],0x45, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_reset[21],0x45, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_reset[22],0x45, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_reset[23],0x45, 0x40, 6); 
    CPLD_REG_ASSIGN(qsfp_reset[24],0x46, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_reset[25],0x46, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_reset[26],0x46, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_reset[27],0x46, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_reset[28],0x46, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_reset[29],0x46, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_reset[30],0x46, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_reset[31],0x46, 0x40, 6); 

    
    //i2c选通表全放到主板数据结构里, 使用mainboard读写主板信息
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_eeprom_start_index + 0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x0), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[2], (0x2 + slot_index)), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0), STEP_OVER);    

    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_lm75_start_index + 0]),   STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x2), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[0], (0x2 + slot_index) << 5), STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_lm75_start_index + 1]),   STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x2), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[0], (0x2 + slot_index) << 5), STEP_OVER);

  
    //光模块选通表, 注意主板上器件使用mainboard操作，子卡上器件使用bd操作
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 0]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 1]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 2]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 3]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 4]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 5]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 6]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 7]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 8]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 9]), STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 10]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 11]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 12]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 13]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 14]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 15]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 16]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 17]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 18]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 19]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 20]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 21]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 22]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 23]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 24]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 25]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 26]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 27]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 28]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<7), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 29]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<7), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 30]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<7), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3),STEP_OVER);
    ret += i2c_select_steps_init(&(mainboard->i2c_select_table[current_slot_optic_start_index + 31]),STEP_CNT(5), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_CPLD, mainboard->cpld_addr_i2c_sel[3], 0x4 * slot_index), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<7), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2),STEP_OVER);

    
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "failed for ret=%d!", ret);
        bd->initialized = 0;
    }
    else
    {
        bd->initialized = 1;
        //mbd->sub_slot_info[slot_index] = bd;
    }
    return ret;
}



int board_static_data_init_TCS81_120F(board_static_data * board_data)
{
    int ret = ERROR_SUCCESS;
    int i = 0;
    board_static_data * bd = board_data;
    //无关数据全写0
    memset(bd, 0, sizeof(board_static_data));

    bd->slot_index = MAIN_BOARD_SLOT_INDEX;
    bd->product_type = PDT_TYPE_TCS81_120F_1U;
    bd->mainboard = NULL;
   
    bd->cpld_num        = 3; 
    bd->fan_num         = 5;        //风扇个数
    bd->motors_per_fan  = 2;        //每风扇2个马达
    bd->fan_speed_coef  = 11718750; //风扇转速转换系数, 9820是 30000000
    bd->fan_max_speed   = 20000;    
    bd->fan_min_speed   = 3000;
    bd->fan_min_speed_pwm = 0x27;
    bd->fan_max_speed_pwm = 0xc2;   //最大转速时对应的pwm值
	bd->fan_min_pwm_speed_percentage = 20;
	bd->fan_max_pwm_speed_percentage = 100;
    bd->fan_temp_low    = 30;
    bd->fan_temp_high   = 70;

    bd->isl68127_num    = 1;
    bd->adm1166_num     = 1;
    bd->psu_num         = 2;        //电源个数
    bd->psu_type        = PSU_TYPE_650W;
    bd->slot_num        = 0;        //子卡个数, 无子卡写0
    bd->smbus_use_index = 0;        //使用的smbus的索引
    bd->lm75_num        = 0;
    bd->max6696_num     = 1;        //max6696数量
    bd->optic_modlue_num= 56;       //可插光模块数量
    bd->eeprom_used_size= 512;      //简化设计eeprom只使用前边512字节, 不够再扩展

    bd->mac_rov_min_voltage     = 750;    //td3芯片适用
    bd->mac_rov_max_voltage     = 1000;
    bd->mac_rov_default_voltage = 890;
    

    bd->fan_target_speed_coef0[0] = 6524500;          //front fan 4 coeffcient of the polynomial
    bd->fan_target_speed_coef1[0] = -112170;
    bd->fan_target_speed_coef2[0] = 4567;
    bd->fan_target_speed_coef3[0] = -22;

    bd->fan_target_speed_coef0[1] = 5400100;          //rear fan
    bd->fan_target_speed_coef1[1] = -57783;
    bd->fan_target_speed_coef2[1] = 3296;
    bd->fan_target_speed_coef3[1] = -15;
    
    
    for (i = 0; i < bd->optic_modlue_num; i++)
    {
        bd->cage_type[i]  = i < 48 ? CAGE_TYPE_SFP : CAGE_TYPE_QSFP;    //前48口是sfp 25G, //后8个口是qsfp 100G
        bd->port_speed[i] = i < 48 ? SPEED_25G : SPEED_100G;
    }

    bd->i2c_addr_isl68127[0] = 0x5c; 
    bd->i2c_addr_eeprom    = 0x50;  //板卡eerom的i2c地址
    //bd->i2c_addr_lm75[0]   = 0x48;  //lm75 i2c地址
    bd->i2c_addr_max6696[0]= 0x18;  //max6696 i2c地址
    bd->max6696_describe[0][0] = "DeviceEnv";
    bd->max6696_describe[0][1] = "ASIC_Front";
    bd->max6696_describe[0][2] = "ASIC_Back";

    bd->i2c_addr_pca9548    = 0x70;        //这里假设所有9545/9548使用的i2c地址都一样
    bd->i2c_addr_pca9548_2  = 0x77;   
    
    bd->i2c_addr_psu[0] = 0x51;
    bd->i2c_addr_psu[1] = 0x50;
    bd->i2c_addr_psu_pmbus[0] = 0x59;    //与电源i2c地址配对, +0x08
    bd->i2c_addr_psu_pmbus[1] = 0x58;

    bd->i2c_addr_ina219[0] = 0x44;
    bd->i2c_addr_ina219[1] = 0x44;

    bd->i2c_addr_adm1166[0]  = 0x34;

    bd->i2c_addr_fan[0] = 0x50;
    bd->i2c_addr_fan[1] = 0x50;
    bd->i2c_addr_fan[2] = 0x50;
    bd->i2c_addr_fan[3] = 0x50;
    bd->i2c_addr_fan[4] = 0x50;
	//cpu vr i2c address
    bd->i2c_addr_cpu_vr[0] = 0x61;
    bd->i2c_addr_cpu_vr[1] = 0x66;

    // cpu  voltage
    bd->i2c_addr_cpu_vol[0] = 0x66;//inner address page 0, 0x8B
    bd->i2c_addr_cpu_vol[1] = 0x61;//inner addess page 0,0x8B
    bd->i2c_addr_cpu_vol[2] = 0x61;//inner address page 1, 0x8B

    //光模块i2c地址初始化
    for (i = 0; i < bd->optic_modlue_num; i++)
    {
        bd->i2c_addr_optic_eeprom[i]= 0x50;
        bd->i2c_addr_optic_eeprom_dom[i]= (bd->cage_type[i] == CAGE_TYPE_SFP) ? 0x51 : 0x50;
    }
  
    bd->cpld_access_type   = IO_REMAP;
    bd->cpld_base_address  = 0xdffe0000;   //由OM提供的地址
    bd->cpld_hw_addr_board = 0x2200;       //由硬件设计提供的地址
    bd->cpld_size_board    = 256;          //按字节数
    bd->cpld_hw_addr_cpu   = 0x2000;       //CPU扣板CPLD
    bd->cpld_size_cpu      = 256;          //CPU扣板CPLD


    //子卡cpld 相关数据
    /*
    bd->cpld_hw_addr_slot[0] = 0x0;          //test from 0
    bd->cpld_size_slot[0] = 0x80;            //test 0
    */


    CPLD_REG_ASSIGN(pcb_type, 0x02, 0xff, 0);

    CPLD_REG_ASSIGN(pcb_ver, 0x00, 0x0f, 0);

    bd->cpld_type_describe[0] = "LCMXO3LF_6900C_5BG400C ";
    bd->cpld_type_describe[1] = "LCMXO2_1200UHC_4FTG256C ";
    bd->cpld_type_describe[2] = "LCMXO2_1200UHC_4FTG256C ";

    bd->cpld_location_describe[0] = "1st-JTAG-Chain";
    bd->cpld_location_describe[1] = "2nd-JTAG-Chain";
    bd->cpld_location_describe[2] = "3rd-JTAG-Chain";

    //0x15 bit4 is for QSPI reset, max6696 reset moved to cpu cpld
    //CPLD_REG_ASSIGN(max6696_rst[0],  0x15, 0x10, 4);

    CPLD_REG_ASSIGN(eeprom_write_protect, 0x55, 0x03, 0);

    //mac 初始完成可点端口灯
    CPLD_REG_ASSIGN(mac_init_ok, 0x0b, 0x01, 0);

    //mac核心电压设置
    CPLD_REG_ASSIGN(mac_rov,     0x3c, 0xff, 0);

    //面板上的系统指示灯


    CPLD_REG_ASSIGN(pannel_sys_led_ctrl,  0x6c, 0x0f, 0);
    /*
      CPLD_REG_ASSIGN(pannel_sys_led_green, 0x6b, 0x10, 4);
      CPLD_REG_ASSIGN(pannel_sys_led_red,   0x6b, 0x20, 5);
      */
    CPLD_REG_ASSIGN(pannel_psu_led_green, 0x6b, 0x40, 6);
    CPLD_REG_ASSIGN(pannel_psu_led_red,   0x6b, 0x80, 7);
    CPLD_REG_ASSIGN(pannel_fan_led_green, 0x6b, 0x04, 2);
    CPLD_REG_ASSIGN(pannel_fan_led_red,   0x6b, 0x08, 3);
    CPLD_REG_ASSIGN(pannel_bmc_led_green, 0x6a, 0x02, 1);
    CPLD_REG_ASSIGN(pannel_bmc_led_red,   0x6a, 0x04, 2);
    CPLD_REG_ASSIGN(pannel_id_led_blue,   0x6a, 0x01, 0);

    //cpld setting for sysled led color
    bd->cpld_value_sys_led_code_green  = 0xf4;
    bd->cpld_value_sys_led_code_red    = 0xf9;
    bd->cpld_value_sys_led_code_yellow = 0xfe;
    bd->cpld_value_sys_led_code_dark   = 0xff;


    //cpld版本
    CPLD_REG_ASSIGN(cpld_ver[0], 0x01, 0x0f, 0);
    CPLD_REG_ASSIGN(cpld_ver[1], 0x03, 0x0f, 0);
    CPLD_REG_ASSIGN(cpld_ver[2], 0x07, 0x0f, 0);

    CPLD_REG_ASSIGN(reset_type_cpu_thermal,    0x20, 0x80, 7);
    CPLD_REG_ASSIGN(reset_type_cold,           0x20, 0x40, 6);
    CPLD_REG_ASSIGN(reset_type_power_en,       0x20, 0x10, 4);
    CPLD_REG_ASSIGN(reset_type_boot_sw,        0x20, 0x08, 3);
    CPLD_REG_ASSIGN(reset_type_soft,           0x20, 0x04, 2);
    CPLD_REG_ASSIGN(reset_type_wdt,            0x20, 0x02, 1);
    CPLD_REG_ASSIGN(reset_type_mlb,            0x20, 0x01, 0);
    CPLD_REG_ASSIGN(clear_reset_flag,          0x21, 0x02, 1);
 
 	//watchdog cpld 相关数据
    CPLD_REG_ASSIGN(wd_feed,      0x30, 0xff, 0);
	CPLD_REG_ASSIGN(wd_disfeed,   0x31, 0xff, 0);
	CPLD_REG_ASSIGN(wd_timeout,   0x32, 0xff, 0);
	CPLD_REG_ASSIGN(wd_enable,    0x33, 0x01, 0);
 
    //电源相关寄存器
    CPLD_REG_ASSIGN(psu_absent[0],0x35, 0x02, 1);
    CPLD_REG_ASSIGN(psu_absent[1],0x35, 0x01, 0);  
    
    CPLD_REG_ASSIGN(psu_good[0],  0x34, 0x02, 1);
    CPLD_REG_ASSIGN(psu_good[1],  0x34, 0x01, 0); 

    //这里开始是寄存器定义
    //风扇相关寄存器定义
    CPLD_REG_ASSIGN(fan_num,      0x70, 0x0f, 0);
    CPLD_REG_ASSIGN(fan_select,   0x70, 0xf0, 4);
    CPLD_REG_ASSIGN(fan_pwm,      0x71, 0xff, 0);
    CPLD_REG_ASSIGN(fan_speed[CPLD_FAN_SPEED_LOW_REG_INDEX],  0x72, 0xff, 0);
    CPLD_REG_ASSIGN(fan_speed[CPLD_FAN_SPEED_HIGH_REG_INDEX], 0x73, 0xff, 0);

    CPLD_REG_ASSIGN(fan_direction[0],0x74, 0x10, 4);
    CPLD_REG_ASSIGN(fan_direction[1],0x74, 0x08, 3);
    CPLD_REG_ASSIGN(fan_direction[2],0x74, 0x04, 2);
    CPLD_REG_ASSIGN(fan_direction[3],0x74, 0x02, 1);
    CPLD_REG_ASSIGN(fan_direction[4],0x74, 0x01, 0);
    
    CPLD_REG_ASSIGN(fan_enable[0],   0x75, 0x10, 4);
    CPLD_REG_ASSIGN(fan_enable[1],   0x75, 0x08, 3);
    CPLD_REG_ASSIGN(fan_enable[2],   0x75, 0x04, 2);
    CPLD_REG_ASSIGN(fan_enable[3],   0x75, 0x02, 1);
    CPLD_REG_ASSIGN(fan_enable[4],   0x75, 0x01, 0);
    
    CPLD_REG_ASSIGN(fan_led_red[0],   0x76, 0x10, 4);
    CPLD_REG_ASSIGN(fan_led_red[1],   0x76, 0x08, 3);
    CPLD_REG_ASSIGN(fan_led_red[2],   0x76, 0x04, 2);
    CPLD_REG_ASSIGN(fan_led_red[3],   0x76, 0x02, 1);
    CPLD_REG_ASSIGN(fan_led_red[4],   0x76, 0x01, 0);

    CPLD_REG_ASSIGN(fan_led_green[0],   0x7d, 0x10, 4);
    CPLD_REG_ASSIGN(fan_led_green[1],   0x7d, 0x08, 3);
    CPLD_REG_ASSIGN(fan_led_green[2],   0x7d, 0x04, 2);
    CPLD_REG_ASSIGN(fan_led_green[3],   0x7d, 0x02, 1);
    CPLD_REG_ASSIGN(fan_led_green[4],   0x7d, 0x01, 0);

    CPLD_REG_ASSIGN(fan_absent[0],   0x77, 0x10, 4);
    CPLD_REG_ASSIGN(fan_absent[1],   0x77, 0x08, 3);
    CPLD_REG_ASSIGN(fan_absent[2],   0x77, 0x04, 2);
    CPLD_REG_ASSIGN(fan_absent[3],   0x77, 0x02, 1);
    CPLD_REG_ASSIGN(fan_absent[4],   0x77, 0x01, 0);
    
    CPLD_REG_ASSIGN(fan_status[0],0x79, 0x0c, 2);
    CPLD_REG_ASSIGN(fan_status[1],0x79, 0x03, 0);
    CPLD_REG_ASSIGN(fan_status[2],0x78, 0x30, 4);
    CPLD_REG_ASSIGN(fan_status[3],0x78, 0x0c, 2);
    CPLD_REG_ASSIGN(fan_status[4],0x78, 0x03, 0);

    //光模块控制相关寄存器 sfp, 按端口索引排续
    CPLD_REG_ASSIGN(cage_power_on, 0x37, 0x20, 5); //所有cage上电
    //在位信号
    CPLD_REG_ASSIGN(sfp_present[0] ,0x89, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_present[1] ,0x89, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_present[2] ,0x89, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_present[3] ,0x89, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_present[4] ,0x89, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_present[5] ,0x89, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_present[6] ,0x89, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_present[7] ,0x89, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_present[8] ,0x8a, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_present[9] ,0x8a, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_present[10],0x8a, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_present[11],0x8a, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_present[12],0x8a, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_present[13],0x8a, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_present[14],0x8a, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_present[15],0x8a, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_present[16],0x8b, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_present[17],0x8b, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_present[18],0x8b, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_present[19],0x8b, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_present[20],0x8b, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_present[21],0x8b, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_present[22],0x8b, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_present[23],0x8b, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_present[24],0x8c, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_present[25],0x8c, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_present[26],0x8c, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_present[27],0x8c, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_present[28],0x8c, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_present[29],0x8c, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_present[30],0x8c, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_present[31],0x8c, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_present[32],0x8d, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_present[33],0x8d, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_present[34],0x8d, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_present[35],0x8d, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_present[36],0x8d, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_present[37],0x8d, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_present[38],0x8d, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_present[39],0x8d, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_present[40],0x8e, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_present[41],0x8e, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_present[42],0x8e, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_present[43],0x8e, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_present[44],0x8e, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_present[45],0x8e, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_present[46],0x8e, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_present[47],0x8e, 0x40, 6); 

    //sfp
    CPLD_REG_ASSIGN(sfp_tx_dis[0] ,0x91, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_dis[1] ,0x91, 0x01, 0);  
    CPLD_REG_ASSIGN(sfp_tx_dis[2] ,0x91, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_dis[3] ,0x91, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_dis[4] ,0x91, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_dis[5] ,0x91, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_dis[6] ,0x91, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_dis[7] ,0x91, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_dis[8] ,0x92, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_dis[9] ,0x92, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_dis[10],0x92, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_dis[11],0x92, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_dis[12],0x92, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_dis[13],0x92, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_dis[14],0x92, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_dis[15],0x92, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_dis[16],0x93, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_dis[17],0x93, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_dis[18],0x93, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_dis[19],0x93, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_dis[20],0x93, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_dis[21],0x93, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_dis[22],0x93, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_dis[23],0x93, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_dis[24],0x94, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_dis[25],0x94, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_dis[26],0x94, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_dis[27],0x94, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_dis[28],0x94, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_dis[29],0x94, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_dis[30],0x94, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_dis[31],0x94, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_dis[32],0x95, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_dis[33],0x95, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_dis[34],0x95, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_dis[35],0x95, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_dis[36],0x95, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_dis[37],0x95, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_dis[38],0x95, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_dis[39],0x95, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_dis[40],0x96, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_dis[41],0x96, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_dis[42],0x96, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_dis[43],0x96, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_dis[44],0x96, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_dis[45],0x96, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_dis[46],0x96, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_dis[47],0x96, 0x40, 6);

    //rx_los
    CPLD_REG_ASSIGN(sfp_rx_los[0] ,0xa1, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_rx_los[1] ,0xa1, 0x01, 0);  
    CPLD_REG_ASSIGN(sfp_rx_los[2] ,0xa1, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_rx_los[3] ,0xa1, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_rx_los[4] ,0xa1, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_rx_los[5] ,0xa1, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_rx_los[6] ,0xa1, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_rx_los[7] ,0xa1, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_rx_los[8] ,0xa2, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_rx_los[9] ,0xa2, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_rx_los[10],0xa2, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_rx_los[11],0xa2, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_rx_los[12],0xa2, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_rx_los[13],0xa2, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_rx_los[14],0xa2, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_rx_los[15],0xa2, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_rx_los[16],0xa3, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_rx_los[17],0xa3, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_rx_los[18],0xa3, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_rx_los[19],0xa3, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_rx_los[20],0xa3, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_rx_los[21],0xa3, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_rx_los[22],0xa3, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_rx_los[23],0xa3, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_rx_los[24],0xa4, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_rx_los[25],0xa4, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_rx_los[26],0xa4, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_rx_los[27],0xa4, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_rx_los[28],0xa4, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_rx_los[29],0xa4, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_rx_los[30],0xa4, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_rx_los[31],0xa4, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_rx_los[32],0xa5, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_rx_los[33],0xa5, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_rx_los[34],0xa5, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_rx_los[35],0xa5, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_rx_los[36],0xa5, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_rx_los[37],0xa5, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_rx_los[38],0xa5, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_rx_los[39],0xa5, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_rx_los[40],0xa6, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_rx_los[41],0xa6, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_rx_los[42],0xa6, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_rx_los[43],0xa6, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_rx_los[44],0xa6, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_rx_los[45],0xa6, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_rx_los[46],0xa6, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_rx_los[47],0xa6, 0x40, 6);


    //tx_fault
    CPLD_REG_ASSIGN(sfp_tx_fault[0] ,0x99, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_fault[1] ,0x99, 0x01, 0);  
    CPLD_REG_ASSIGN(sfp_tx_fault[2] ,0x99, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_fault[3] ,0x99, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_fault[4] ,0x99, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_fault[5] ,0x99, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_fault[6] ,0x99, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_fault[7] ,0x99, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_fault[8] ,0x9a, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_fault[9] ,0x9a, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_fault[10],0x9a, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_fault[11],0x9a, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_fault[12],0x9a, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_fault[13],0x9a, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_fault[14],0x9a, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_fault[15],0x9a, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_fault[16],0x9b, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_fault[17],0x9b, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_fault[18],0x9b, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_fault[19],0x9b, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_fault[20],0x9b, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_fault[21],0x9b, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_fault[22],0x9b, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_fault[23],0x9b, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_fault[24],0x9c, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_fault[25],0x9c, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_fault[26],0x9c, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_fault[27],0x9c, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_fault[28],0x9c, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_fault[29],0x9c, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_fault[30],0x9c, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_fault[31],0x9c, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_fault[32],0x9d, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_fault[33],0x9d, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_fault[34],0x9d, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_fault[35],0x9d, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_fault[36],0x9d, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_fault[37],0x9d, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_fault[38],0x9d, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_fault[39],0x9d, 0x40, 6); 
    CPLD_REG_ASSIGN(sfp_tx_fault[40],0x9e, 0x02, 1); 
    CPLD_REG_ASSIGN(sfp_tx_fault[41],0x9e, 0x01, 0); 
    CPLD_REG_ASSIGN(sfp_tx_fault[42],0x9e, 0x08, 3); 
    CPLD_REG_ASSIGN(sfp_tx_fault[43],0x9e, 0x04, 2); 
    CPLD_REG_ASSIGN(sfp_tx_fault[44],0x9e, 0x20, 5); 
    CPLD_REG_ASSIGN(sfp_tx_fault[45],0x9e, 0x10, 4); 
    CPLD_REG_ASSIGN(sfp_tx_fault[46],0x9e, 0x80, 7); 
    CPLD_REG_ASSIGN(sfp_tx_fault[47],0x9e, 0x40, 6);

    //qsfp, 从48开始排续. 数组索引与端口索引一致
    CPLD_REG_ASSIGN(qsfp_present[48],0xb1, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_present[49],0xb1, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_present[50],0xb1, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_present[51],0xb1, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_present[52],0xb1, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_present[53],0xb1, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_present[54],0xb1, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_present[55],0xb1, 0x40, 6); 

    CPLD_REG_ASSIGN(qsfp_lpmode[48],0xb9, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_lpmode[49],0xb9, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_lpmode[50],0xb9, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_lpmode[51],0xb9, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_lpmode[52],0xb9, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_lpmode[53],0xb9, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_lpmode[54],0xb9, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_lpmode[55],0xb9, 0x40, 6); 


    CPLD_REG_ASSIGN(qsfp_reset[48],0xc1, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_reset[49],0xc1, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_reset[50],0xc1, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_reset[51],0xc1, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_reset[52],0xc1, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_reset[53],0xc1, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_reset[54],0xc1, 0x80, 7);  
    CPLD_REG_ASSIGN(qsfp_reset[55],0xc1, 0x40, 6); 

    CPLD_REG_ASSIGN(qsfp_interrupt[48],0xc9, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_interrupt[49],0xc9, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_interrupt[50],0xc9, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_interrupt[51],0xc9, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_interrupt[52],0xc9, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_interrupt[53],0xc9, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_interrupt[54],0xc9, 0x80, 7);  
    CPLD_REG_ASSIGN(qsfp_interrupt[55],0xc9, 0x40, 6); 


    //9548 reset寄存器
    CPLD_REG_ASSIGN(9548_rst[0] ,0x16, 0x01, 0); 
    CPLD_REG_ASSIGN(9548_rst[1] ,0x16, 0x02, 1); 
    CPLD_REG_ASSIGN(9548_rst[2] ,0x16, 0x04, 2); 
    CPLD_REG_ASSIGN(9548_rst[3] ,0x16, 0x08, 3); 
    CPLD_REG_ASSIGN(9548_rst[4] ,0x16, 0x10, 4); 
    CPLD_REG_ASSIGN(9548_rst[5] ,0x16, 0x20, 5); 
    CPLD_REG_ASSIGN(9548_rst[6] ,0x16, 0x40, 6); 
    CPLD_REG_ASSIGN(9548_rst[7] ,0x16, 0x80, 7); 

    //重置cpu寄存器
    CPLD_REG_ASSIGN(cpu_rst, 0x15, 0x08, 3);

    CPLD_REG_ASSIGN(i2c_wdt_ctrl, 0x32, 0x0f, 0);
    CPLD_REG_ASSIGN(cpu_init_ok,  0xb,  0x80, 7);
    CPLD_REG_ASSIGN(i2c_wdt_feed, 0x33, 0x01, 0);

    CPLD_REG_ASSIGN(gpio_i2c_1, 0x41, 0x01, 0);
    CPLD_REG_ASSIGN(gpio_i2c_0, 0x41, 0x02, 1);


    //i2c选通寄存器
    CPLD_REG_ASSIGN(main_i2c_sel, 0x48, 0xff, 0);     //主通道选择对应的cpld寄存器
    CPLD_REG_ASSIGN(i2c_sel[0],   0x49, 0xff, 0);     //非主通道选择对应的cpld寄存器
    CPLD_REG_ASSIGN(i2c_sel[1],   0x4a, 0xff, 0);
    CPLD_REG_ASSIGN(i2c_sel[2],   0x4b, 0xff, 0);
    CPLD_REG_ASSIGN(i2c_sel[3],   0x4c, 0xff, 0);
    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_MAX6696]),  STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_MAX6696+1]),  STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 2), STEP_OVER);
       

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_EEPROM]),   STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0), STEP_OVER);    
    //ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_LM75]),   STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 2), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+0]),    STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x04),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+1]),    STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x03),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+2]),    STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x02),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+3]),    STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x01),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+4]),    STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 5), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[0], 0x00),STEP_OVER);

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU+0]),    STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x6F),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU+1]),    STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x2F),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_INA219+0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xAF),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_INA219+1]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 1), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0xEF),STEP_OVER);

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_ISL68127]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x04),STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_ADM1166]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x00),STEP_OVER);

	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VR + 0]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x08), STEP_OVER);	 
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VR + 1]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x08), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 0]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x08), STEP_OVER);	  
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 1]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x08), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 2]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_i2c_sel[1], 0x08), STEP_OVER);

    //I2C DEV
    //ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_I350]),     STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 6), STEP(OP_TYPE_WR_CPLD, 0x4b, 0xee), STEP_OVER);
    
    //光模块选通表
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+0] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+1] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+2] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+3] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+4] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+5] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+6] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+7] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+8] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+9] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+10]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+11]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+12]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+13]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+14]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+15]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+16]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+17]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+18]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+19]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+20]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+21]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+22]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+23]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+24]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+25]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+26]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+27]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+28]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+29]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+30]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+31]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+32]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+33]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+34]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+35]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+36]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+37]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+38]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+39]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<4), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+40]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+41]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+42]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+43]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+44]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+45]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+46]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+47]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<5), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+48]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+49]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+50]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+51]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+52]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+53]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+54]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+55]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x7), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<6), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);

     
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "failed for ret=%d!", ret);
        bd->initialized = 0;
    }
    else
    {
        bd->initialized = 1;
    }
    return ret;
}

/* TCS82设备初始化add by scg */
int board_static_data_init_TCS82_120F(board_static_data * board_data)
{
    int ret = ERROR_SUCCESS;
    int i = 0;
    board_static_data * bd = board_data;
    //无关数据全写0
    memset(bd, 0, sizeof(board_static_data));

    bd->slot_index = MAIN_BOARD_SLOT_INDEX;
    bd->product_type = PDT_TYPE_TCS82_120F_1U;
    bd->mainboard = NULL;
   
    bd->cpld_num        = 2; 		//TCS82的逻辑个数是2
    bd->fan_num         = 5;        //风扇个数
    bd->motors_per_fan  = 2;        //每风扇2个马达
    bd->fan_speed_coef  = 11718750; //风扇转速转换系数, 9820是 30000000  
    bd->fan_max_speed   = 20000;    
    bd->fan_min_speed   = 3000;
    bd->fan_min_speed_pwm = 0x27;
    bd->fan_max_speed_pwm = 0xc2;   //最大转速时对应的pwm值
	bd->fan_min_pwm_speed_percentage = 20;
	bd->fan_max_pwm_speed_percentage = 100;
    bd->fan_temp_low    = 30;
    bd->fan_temp_high   = 70;

    bd->isl68127_num    = 1;		/* 待确认*/
    bd->adm1166_num     = 1;		/* 待确认*/	
    bd->psu_num         = 2;        //电源个数
    bd->psu_type        = PSU_TYPE_650W;
    bd->slot_num        = 0;        //子卡个数, 无子卡写0
    bd->smbus_use_index = 0;        /* 待确认*///使用的smbus的索引	
    bd->lm75_num        = 0;		/* 待确认*/
    bd->max6696_num     = 1;        /* 待确认*///max6696数量
    bd->optic_modlue_num= 32;       //可插光模块数量
    bd->eeprom_used_size= 512;      /* 待确认*/ //简化设计eeprom只使用前边512字节, 不够再扩展

	//沿用TCS81的数据，同为TD3芯片
    bd->mac_rov_min_voltage     = 750;    //td3芯片适用
    bd->mac_rov_max_voltage     = 1000;
    bd->mac_rov_default_voltage = 890;
    

    
    for (i = 0; i < bd->optic_modlue_num; i++)
    {
        bd->cage_type[i]  = CAGE_TYPE_QSFP;    // 32口100G 设备
        bd->port_speed[i] = SPEED_100G;		// 初始化都设置为100G
    }

    bd->i2c_addr_isl68127[0] = 0x5c; 
    bd->i2c_addr_eeprom    = 0x50;  //板卡eerom的i2c地址
    //bd->i2c_addr_lm75[0]   = 0x48;  //lm75 i2c地址
    bd->i2c_addr_max6696[0]= 0x18;  //max6696 i2c地址
    bd->max6696_describe[0][0] = "DeviceEnv";
    bd->max6696_describe[0][1] = "ASIC_Front";
    bd->max6696_describe[0][2] = "ASIC_Back";

    bd->i2c_addr_pca9548    = 0x70;        //这里假设所有9545/9548使用的i2c地址都一样
    bd->i2c_addr_pca9548_2  = 0x77;   
	bd->i2c_addr_pca9545    = 0x73;
	
    bd->i2c_addr_psu[0] = 0x50;
    bd->i2c_addr_psu[1] = 0x50;
    bd->i2c_addr_psu_pmbus[0] = 0x58;    //与电源i2c地址配对, +0x08
    bd->i2c_addr_psu_pmbus[1] = 0x58;

    bd->i2c_addr_ina219[0] = 0x44;
    bd->i2c_addr_ina219[1] = 0x44;

    bd->i2c_addr_adm1166[0]  = 0x34;

    bd->i2c_addr_fan[0] = 0x50;
    bd->i2c_addr_fan[1] = 0x50;
    bd->i2c_addr_fan[2] = 0x50;
    bd->i2c_addr_fan[3] = 0x50;
    bd->i2c_addr_fan[4] = 0x50;

    //cpu vr i2c address
    bd->i2c_addr_cpu_vr[0] = 0x61;
    bd->i2c_addr_cpu_vr[1] = 0x66;

     // cpu  voltage
    bd->i2c_addr_cpu_vol[0] = 0x66;//inner address page 0, 0x8B
    bd->i2c_addr_cpu_vol[1] = 0x61;//inner addess page 0,0x8B
    bd->i2c_addr_cpu_vol[2] = 0x61;//inner address page 1, 0x8B
    //光模块i2c地址初始化
    for (i = 0; i < bd->optic_modlue_num; i++)
    {
        bd->i2c_addr_optic_eeprom[i]= 0x50;
        bd->i2c_addr_optic_eeprom_dom[i]= (bd->cage_type[i] == CAGE_TYPE_SFP) ? 0x51 : 0x50;
    }
  
    bd->cpld_access_type   = IO_REMAP;
    bd->cpld_base_address  = 0xdffe0000;   //由OM提供的地址
    bd->cpld_hw_addr_board = 0x2200;       //由硬件设计提供的地址
    bd->cpld_size_board    = 256;          //按字节数
    bd->cpld_hw_addr_cpu   = 0x2000;       //CPU扣板CPLD
    bd->cpld_size_cpu      = 256;          //CPU扣板CPLD


    CPLD_REG_ASSIGN(pcb_type, 0x02, 0xff, 0);

    CPLD_REG_ASSIGN(pcb_ver, 0x00, 0x0f, 0);

    bd->cpld_type_describe[0] = "LCMXO3LF_6900C_5BG400C ";
    bd->cpld_type_describe[1] = "LCMXO2_1200UHC_4FTG256C ";
    //bd->cpld_type_describe[2] = "LCMXO2_1200UHC_4FTG256C ";		TCS82有两个CPLD

    bd->cpld_location_describe[0] = "1st-JTAG-Chain";
    bd->cpld_location_describe[1] = "2nd-JTAG-Chain";
    //bd->cpld_location_describe[2] = "3rd-JTAG-Chain";		//TCS82有两个CPLD

	// 在综合复位寄存器的bit 4中
	CPLD_REG_ASSIGN(max6696_rst[0],  0x15, 0x10, 4);

    CPLD_REG_ASSIGN(eeprom_write_protect, 0x55, 0x07, 0);

    //mac 初始完成可点端口灯
    CPLD_REG_ASSIGN(mac_init_ok, 0x0b, 0x01, 0);

    //mac核心电压设置
    CPLD_REG_ASSIGN(mac_rov,     0x3d, 0xff, 0);

    //系统指示灯寄存器
    CPLD_REG_ASSIGN(pannel_psu_led_red,   0x6b, 0x80, 7);
    CPLD_REG_ASSIGN(pannel_psu_led_green, 0x6b, 0x40, 6);
    CPLD_REG_ASSIGN(pannel_sys_led_ctrl,  0x6c, 0x0f, 0);
    /*
    CPLD_REG_ASSIGN(pannel_sys_led_red,   0x6b, 0x20, 5);
    CPLD_REG_ASSIGN(pannel_sys_led_green, 0x6b, 0x10, 4);
    */
    CPLD_REG_ASSIGN(pannel_fan_led_red,   0x6b, 0x08, 3);
    CPLD_REG_ASSIGN(pannel_fan_led_green, 0x6b, 0x04, 2);
    CPLD_REG_ASSIGN(pannel_bmc_led_red,   0x6b, 0x02, 1);
    CPLD_REG_ASSIGN(pannel_bmc_led_green, 0x6b, 0x01, 0);
    CPLD_REG_ASSIGN(pannel_id_led_blue,   0x6a, 0x01, 0);

    //cpld setting for sysled led color
    bd->cpld_value_sys_led_code_green  = 0xf4;
    bd->cpld_value_sys_led_code_red    = 0xf9;
    bd->cpld_value_sys_led_code_yellow = 0xfe;
    bd->cpld_value_sys_led_code_dark   = 0xff;


    //cpld版本
    CPLD_REG_ASSIGN(cpld_ver[0], 0x01, 0x0f, 0);
    CPLD_REG_ASSIGN(cpld_ver[1], 0x03, 0x0f, 0);
    //CPLD_REG_ASSIGN(cpld_ver[2], 0x07, 0x0f, 0);

    CPLD_REG_ASSIGN(reset_type_cpu_thermal,    0x20, 0x80, 7);
    CPLD_REG_ASSIGN(reset_type_cold,           0x20, 0x40, 6);
    CPLD_REG_ASSIGN(reset_type_power_en,       0x20, 0x10, 4);
    CPLD_REG_ASSIGN(reset_type_boot_sw,        0x20, 0x08, 3);
    CPLD_REG_ASSIGN(reset_type_soft,           0x20, 0x04, 2);
    CPLD_REG_ASSIGN(reset_type_wdt,            0x20, 0x02, 1);
    CPLD_REG_ASSIGN(reset_type_mlb,            0x20, 0x01, 0);
    CPLD_REG_ASSIGN(clear_reset_flag,          0x21, 0x02, 1);
 
 	//watchdog cpld 相关数据
    CPLD_REG_ASSIGN(wd_feed,      0x30, 0xff, 0);
	CPLD_REG_ASSIGN(wd_disfeed,   0x31, 0xff, 0);
	CPLD_REG_ASSIGN(wd_timeout,   0x32, 0xff, 0);
	CPLD_REG_ASSIGN(wd_enable,    0x33, 0x01, 0);
 
    //电源相关寄存器
    CPLD_REG_ASSIGN(psu_absent[1],0x35, 0x02, 1);
    CPLD_REG_ASSIGN(psu_absent[0],0x35, 0x01, 0);  
    
    CPLD_REG_ASSIGN(psu_good[1],  0x34, 0x02, 1);
    CPLD_REG_ASSIGN(psu_good[0],  0x34, 0x01, 0); 

    //这里开始是寄存器定义
    //风扇相关寄存器定义
    CPLD_REG_ASSIGN(fan_num,      0x70, 0x0f, 0);
    CPLD_REG_ASSIGN(fan_select,   0x70, 0xf0, 4);
    CPLD_REG_ASSIGN(fan_pwm,      0x71, 0xff, 0);
    CPLD_REG_ASSIGN(fan_speed[CPLD_FAN_SPEED_LOW_REG_INDEX],  0x72, 0xff, 0);
    CPLD_REG_ASSIGN(fan_speed[CPLD_FAN_SPEED_HIGH_REG_INDEX], 0x73, 0xff, 0);

    CPLD_REG_ASSIGN(fan_direction[4],0x74, 0x10, 4);
    CPLD_REG_ASSIGN(fan_direction[3],0x74, 0x08, 3);
    CPLD_REG_ASSIGN(fan_direction[2],0x74, 0x04, 2);
    CPLD_REG_ASSIGN(fan_direction[1],0x74, 0x02, 1);
    CPLD_REG_ASSIGN(fan_direction[0],0x74, 0x01, 0);
    
    CPLD_REG_ASSIGN(fan_enable[4],   0x75, 0x10, 4);
    CPLD_REG_ASSIGN(fan_enable[3],   0x75, 0x08, 3);
    CPLD_REG_ASSIGN(fan_enable[2],   0x75, 0x04, 2);
    CPLD_REG_ASSIGN(fan_enable[1],   0x75, 0x02, 1);
    CPLD_REG_ASSIGN(fan_enable[0],   0x75, 0x01, 0);
    
    CPLD_REG_ASSIGN(fan_led_red[4],   0x76, 0x10, 4);
    CPLD_REG_ASSIGN(fan_led_red[3],   0x76, 0x08, 3);
    CPLD_REG_ASSIGN(fan_led_red[2],   0x76, 0x04, 2);
    CPLD_REG_ASSIGN(fan_led_red[1],   0x76, 0x02, 1);
    CPLD_REG_ASSIGN(fan_led_red[0],   0x76, 0x01, 0);

    CPLD_REG_ASSIGN(fan_led_green[4],   0x7d, 0x10, 4);
    CPLD_REG_ASSIGN(fan_led_green[3],   0x7d, 0x08, 3);
    CPLD_REG_ASSIGN(fan_led_green[2],   0x7d, 0x04, 2);
    CPLD_REG_ASSIGN(fan_led_green[1],   0x7d, 0x02, 1);
    CPLD_REG_ASSIGN(fan_led_green[0],   0x7d, 0x01, 0);

    CPLD_REG_ASSIGN(fan_absent[4],   0x77, 0x10, 4);
    CPLD_REG_ASSIGN(fan_absent[3],   0x77, 0x08, 3);
    CPLD_REG_ASSIGN(fan_absent[2],   0x77, 0x04, 2);
    CPLD_REG_ASSIGN(fan_absent[1],   0x77, 0x02, 1);
    CPLD_REG_ASSIGN(fan_absent[0],   0x77, 0x01, 0);
    
    CPLD_REG_ASSIGN(fan_status[0],0x78, 0x03, 0);
    CPLD_REG_ASSIGN(fan_status[1],0x78, 0x0c, 2);
    CPLD_REG_ASSIGN(fan_status[2],0x78, 0x30, 4);
    CPLD_REG_ASSIGN(fan_status[3],0x78, 0xc0, 6);
    CPLD_REG_ASSIGN(fan_status[4],0x79, 0x03, 0);

    //光模块控制相关寄存器 sfp, 按端口索引排续
    CPLD_REG_ASSIGN(cage_power_on, 0x37, 0x20, 5); //所有cage上电
    
    //qsfp, 从48开始排续. 数组索引与端口索引一致
    CPLD_REG_ASSIGN(qsfp_present[0],0xb1, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_present[1],0xb1, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_present[2],0xb1, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_present[3],0xb1, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_present[4],0xb1, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_present[5],0xb1, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_present[6],0xb1, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_present[7],0xb1, 0x40, 6); 
	CPLD_REG_ASSIGN(qsfp_present[8],0xb2, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_present[9],0xb2, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_present[10],0xb2, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_present[11],0xb2, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_present[12],0xb2, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_present[13],0xb2, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_present[14],0xb2, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_present[15],0xb2, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_present[16],0xb3, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_present[17],0xb3, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_present[18],0xb3, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_present[19],0xb3, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_present[20],0xb3, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_present[21],0xb3, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_present[22],0xb3, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_present[23],0xb3, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_present[24],0xb4, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_present[25],0xb4, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_present[26],0xb4, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_present[27],0xb4, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_present[28],0xb4, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_present[29],0xb4, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_present[30],0xb4, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_present[31],0xb4, 0x40, 6);


    CPLD_REG_ASSIGN(qsfp_lpmode[0],0xb9, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_lpmode[1],0xb9, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_lpmode[2],0xb9, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_lpmode[3],0xb9, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_lpmode[4],0xb9, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_lpmode[5],0xb9, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_lpmode[6],0xb9, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_lpmode[7],0xb9, 0x40, 6); 
	CPLD_REG_ASSIGN(qsfp_lpmode[8],0xba, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_lpmode[9],0xba, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_lpmode[10],0xba, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_lpmode[11],0xba, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_lpmode[12],0xba, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_lpmode[13],0xba, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_lpmode[14],0xba, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_lpmode[15],0xba, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_lpmode[16],0xbb, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_lpmode[17],0xbb, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_lpmode[18],0xbb, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_lpmode[19],0xbb, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_lpmode[20],0xbb, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_lpmode[21],0xbb, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_lpmode[22],0xbb, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_lpmode[23],0xbb, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_lpmode[24],0xbc, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_lpmode[25],0xbc, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_lpmode[26],0xbc, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_lpmode[27],0xbc, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_lpmode[28],0xbc, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_lpmode[29],0xbc, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_lpmode[30],0xbc, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_lpmode[31],0xbc, 0x40, 6);


    CPLD_REG_ASSIGN(qsfp_reset[0],0xc1, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_reset[1],0xc1, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_reset[2],0xc1, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_reset[3],0xc1, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_reset[4],0xc1, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_reset[5],0xc1, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_reset[6],0xc1, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_reset[7],0xc1, 0x40, 6); 
	CPLD_REG_ASSIGN(qsfp_reset[8],0xc2, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_reset[9],0xc2, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_reset[10],0xc2, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_reset[11],0xc2, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_reset[12],0xc2, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_reset[13],0xc2, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_reset[14],0xc2, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_reset[15],0xc2, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_reset[16],0xc3, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_reset[17],0xc3, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_reset[18],0xc3, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_reset[19],0xc3, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_reset[20],0xc3, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_reset[21],0xc3, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_reset[22],0xc3, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_reset[23],0xc3, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_reset[24],0xc4, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_reset[25],0xc4, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_reset[26],0xc4, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_reset[27],0xc4, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_reset[28],0xc4, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_reset[29],0xc4, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_reset[30],0xc4, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_reset[31],0xc4, 0x40, 6);


    CPLD_REG_ASSIGN(qsfp_interrupt[0],0xc9, 0x02, 1); 
    CPLD_REG_ASSIGN(qsfp_interrupt[1],0xc9, 0x01, 0); 
    CPLD_REG_ASSIGN(qsfp_interrupt[2],0xc9, 0x08, 3); 
    CPLD_REG_ASSIGN(qsfp_interrupt[3],0xc9, 0x04, 2); 
    CPLD_REG_ASSIGN(qsfp_interrupt[4],0xc9, 0x20, 5); 
    CPLD_REG_ASSIGN(qsfp_interrupt[5],0xc9, 0x10, 4); 
    CPLD_REG_ASSIGN(qsfp_interrupt[6],0xc9, 0x80, 7); 
    CPLD_REG_ASSIGN(qsfp_interrupt[7],0xc9, 0x40, 6); 
	CPLD_REG_ASSIGN(qsfp_interrupt[8],0xca, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_interrupt[9],0xca, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_interrupt[10],0xca, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_interrupt[11],0xca, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_interrupt[12],0xca, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_interrupt[13],0xca, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_interrupt[14],0xca, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_interrupt[15],0xca, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_interrupt[16],0xcb, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_interrupt[17],0xcb, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_interrupt[18],0xcb, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_interrupt[19],0xcb, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_interrupt[20],0xcb, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_interrupt[21],0xcb, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_interrupt[22],0xcb, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_interrupt[23],0xcb, 0x40, 6);
	CPLD_REG_ASSIGN(qsfp_interrupt[24],0xcc, 0x02, 1); 
	CPLD_REG_ASSIGN(qsfp_interrupt[25],0xcc, 0x01, 0); 
	CPLD_REG_ASSIGN(qsfp_interrupt[26],0xcc, 0x08, 3); 
	CPLD_REG_ASSIGN(qsfp_interrupt[27],0xcc, 0x04, 2); 
	CPLD_REG_ASSIGN(qsfp_interrupt[28],0xcc, 0x20, 5); 
	CPLD_REG_ASSIGN(qsfp_interrupt[29],0xcc, 0x10, 4); 
	CPLD_REG_ASSIGN(qsfp_interrupt[30],0xcc, 0x80, 7); 
	CPLD_REG_ASSIGN(qsfp_interrupt[31],0xcc, 0x40, 6);


    //9548 reset寄存器
    CPLD_REG_ASSIGN(9548_rst[0] ,0x16, 0x01, 0); 
    CPLD_REG_ASSIGN(9548_rst[1] ,0x16, 0x02, 1); 
    CPLD_REG_ASSIGN(9548_rst[2] ,0x16, 0x04, 2); 
    CPLD_REG_ASSIGN(9548_rst[3] ,0x16, 0x08, 3); 
    CPLD_REG_ASSIGN(9548_rst[4] ,0x16, 0x10, 4); 

	//9545 reset寄存器
	CPLD_REG_ASSIGN(9545_rst[0] ,0x16, 0x20, 5); 

    //重置cpu寄存器
    CPLD_REG_ASSIGN(cpu_rst, 0x15, 0x08, 3);

    CPLD_REG_ASSIGN(i2c_wdt_ctrl, 0x32, 0x0f, 0);
    CPLD_REG_ASSIGN(cpu_init_ok,  0xb,  0x80, 7);
    CPLD_REG_ASSIGN(i2c_wdt_feed, 0x33, 0x01, 0);

    CPLD_REG_ASSIGN(gpio_i2c_1, 0x41, 0x01, 0);
    CPLD_REG_ASSIGN(gpio_i2c_0, 0x41, 0x02, 1);
    

    //i2c选通寄存器
    CPLD_REG_ASSIGN(main_i2c_sel, 0x48, 0xff, 0);     //主通道选择对应的cpld寄存器
    //CPLD_REG_ASSIGN(i2c_sel[0],   0x49, 0xff, 0);     //非主通道选择对应的cpld寄存器
    //CPLD_REG_ASSIGN(i2c_sel[1],   0x4a, 0xff, 0);
    //CPLD_REG_ASSIGN(i2c_sel[2],   0x4b, 0xff, 0);
    //CPLD_REG_ASSIGN(i2c_sel[3],   0x4c, 0xff, 0);
    
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_MAX6696]),  STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x18), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_EEPROM]),   STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x08), STEP_OVER); 

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU + 0]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x10), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<0), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_PSU + 1]),  STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x10), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<1), STEP_OVER);

	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_INA219+0]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x10), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<2), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_INA219+1]), STEP_CNT(3), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x10), STEP(OP_TYPE_WR_9545, bd->i2c_addr_pca9545, 1<<3), STEP_OVER);


	
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+0]),    STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x30), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+1]),    STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x31), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+2]),    STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x32), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+3]),    STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x33), STEP_OVER);
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_FAN+4]),    STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x34), STEP_OVER);

    //ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_ADM1166]),  STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x28), STEP_OVER);
    
    //I2C DEV
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_I350]), STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x38), STEP_OVER);

    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_ISL68127]), STEP_CNT(2), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x29), STEP_OVER);
        
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VR + 0]),  STEP_CNT(2),  STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2a), STEP_OVER); 
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VR + 1]),  STEP_CNT(2),  STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2a), STEP_OVER); 
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 0]),  STEP_CNT(2),  STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2a), STEP_OVER);     
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 1]),  STEP_CNT(2),  STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2a), STEP_OVER); 
    ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_CPU_VOL + 2]),  STEP_CNT(2),  STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x2a), STEP_OVER); 

     
    //光模块选通表
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+0] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+1] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+2] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+3] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+4] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+5] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+6] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+7] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<0), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+8] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+9] ),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+10]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+11]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+12]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+13]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+14]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+15]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<1), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+16]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+17]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+18]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+19]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+20]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+21]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+22]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+23]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<2), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+24]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<1), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+25]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<0), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+26]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<3), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+27]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<2), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+28]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<5), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+29]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<4), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+30]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<7), STEP_OVER);
	ret += i2c_select_steps_init(&(bd->i2c_select_table[I2C_DEV_OPTIC_IDX_START+31]),STEP_CNT(4), STEP(OP_TYPE_WR_CPLD, bd->cpld_addr_main_i2c_sel, 0x40), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548, 1<<3), STEP(OP_TYPE_WR_9548, bd->i2c_addr_pca9548_2, 1<<6), STEP_OVER);

	 
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "failed for ret=%d!", ret);
        bd->initialized = 0;
    }
    else
    {
        bd->initialized = 1;
    }
    return ret;
}



/**************************************/
//cpld on switch board
size_t bsp_cpld_get_size(void)
{
    return bsp_get_board_data()->cpld_size_board;
    //return main_board_data.cpld_size_board;
}

//sub slot cpld size, slot_index from 0
size_t bsp_cpld_get_slot_size(int slot_index)
{
    if (slot_index >= MAX_SLOT_NUM)
    {
        DBG_ECHO(DEBUG_ERR, "invalid slot_index %d", slot_index);
        return 0;
    }
    return bsp_get_board_data()->cpld_size_slot[slot_index];
    //return main_board_data.cpld_size_slot[slot_index];
}

//cpld on cpu board
size_t bsp_get_cpu_cpld_size(void)
{
    return bsp_get_board_data()->cpld_size_cpu;
    //return main_board_data.cpld_size_cpu;
}

//只能由 read_byte/write_byte/read_part/write_part/set_bit/get_bit调用，不能由其它上层调用。
//read_write_byte和read_write_part是同一级函数
static u8 bsp_cpld_read_absolue_address(volatile void * addr)
{
    if (bsp_get_board_data()->cpld_access_type == IO_REMAP)
    {
        return readb(addr);
    }
    else
    {
#if DIRECT_IO_SUPPORT
        return inb((long)addr);
#endif
    }
    return (u8)0;
}

static void bsp_cpld_write_absolue_address(volatile void * addr, u8 value)
{
    if (bsp_get_board_data()->cpld_access_type == IO_REMAP)
    {
        writeb(value, addr);
    }
    else
    {
#if DIRECT_IO_SUPPORT
        outb(value, (unsigned long)addr);
#endif
    }
    return;
}

//access board cpld, param offset is the offset bytes which based on 0x2200.
int bsp_cpld_read_byte(OUT u8 *value, IN u16 offset)
{
    if (offset > bsp_get_board_data()->cpld_size_board)
    {
        DBG_ECHO(DEBUG_ERR, "Invalid cpld offset 0x%x (> 0x%x)", offset, bsp_get_board_data()->cpld_size_board);
        return ERROR_FAILED;
    }
    
    *value = bsp_cpld_read_absolue_address(offset + g_u64Cpld_addr);

    return ERROR_SUCCESS;
}

//write board cpld
int bsp_cpld_write_byte(IN u8 value, IN u16 offset)
{
    if (offset > bsp_get_board_data()->cpld_size_board)
    {
        DBG_ECHO(DEBUG_ERR, "Invalid cpld offset 0x%x (> 0x%x)", offset, bsp_get_board_data()->cpld_size_board);
        return ERROR_FAILED;
    }
    
    mutex_lock(&bsp_cpld_lock);
    bsp_cpld_write_absolue_address(offset + g_u64Cpld_addr, value);
    mutex_unlock(&bsp_cpld_lock);
    
    return ERROR_SUCCESS;
}

//读取cpld寄存器部分域，value = (v & mask) >> shift_bits
int bsp_cpld_read_part(OUT u8 *value, IN u16 offset, IN u8 mask, IN u8 shift_bits)
{
    if (offset > bsp_get_board_data()->cpld_size_board)
    {
        DBG_ECHO(DEBUG_ERR, "Invalid cpld offset 0x%x (> 0x%x)", offset, bsp_get_board_data()->cpld_size_board);
        return ERROR_FAILED;
    }
    
    *value = bsp_cpld_read_absolue_address(offset + g_u64Cpld_addr);    
    *value = (*value & mask) >> shift_bits;

    return ERROR_SUCCESS;
}

//写cpld寄存器部分域
int bsp_cpld_write_part(IN u8 value, IN u16 offset, IN u8 mask, IN u8 shift_bits)
{
    u8 temp = 0;
    int ret = ERROR_SUCCESS;

    if (offset > bsp_get_board_data()->cpld_size_board || shift_bits > 7)
    {
        DBG_ECHO(DEBUG_ERR, "Invalid param: cpld offset 0x%x, shift_bits %d", offset, shift_bits);
        ret = ERROR_FAILED;
        goto exit_no_lock;
    }
    mutex_lock(&bsp_cpld_lock);
    
    temp = bsp_cpld_read_absolue_address(offset + g_u64Cpld_addr);
    temp &= 0xff;
    temp &= (~mask);
    temp |= (u8)((u32)value << shift_bits);
    bsp_cpld_write_absolue_address(offset + g_u64Cpld_addr, temp);
    
    mutex_unlock(&bsp_cpld_lock);
    
exit_no_lock:
    
    return ret;
}


// board cpld
int bsp_cpld_get_bit(u16 cpld_offset, u8 bit, u8 * value)
{
    int ret = ERROR_SUCCESS;

    if (bit > 7)
    {   
        DBG_ECHO(DEBUG_DBG, "param error, offset 0x%x bit %d > 7", cpld_offset, bit);
        return ERROR_FAILED;
    }

    ret = bsp_cpld_read_part(value, cpld_offset, 1 << bit, bit);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "failed");
    
exit:
    return ret;
}

//bsp_cpld_set_bit调用bsp_cpld_write_part，不用加锁
int bsp_cpld_set_bit(u16 cpld_offset, u8 bit, u8 value)
{
    //u8 val = 0;
    int ret = ERROR_SUCCESS;
  
    if (((0 != value) && (1 != value)) || (bit > 7))
    {
        DBG_ECHO(DEBUG_ERR, "param error, cpld_offset=0x%x bit=%d value=0x%x", cpld_offset, bit, value);
        return ERROR_FAILED;
    }
    ret = bsp_cpld_write_part(value, cpld_offset, 1 << bit, bit);
    
    return ret;
}


//read cpu card cpld space
int bsp_cpu_cpld_read_byte(OUT u8 *value, IN u16 offset)
{
    if (offset > bsp_get_board_data()->cpld_size_cpu)
    {
        DBG_ECHO(DEBUG_ERR, "Invalid cpld offset 0x%x (> 0x%x)", offset, bsp_get_board_data()->cpld_size_cpu);
        return ERROR_FAILED;
    }
    
    *value = bsp_cpld_read_absolue_address(offset + g_u64CpuCpld_addr);

    //DBG_ECHO(DEBUG_DBG, "cpu cpld read offset 0x%x value 0x%x", offset, *value);
    return ERROR_SUCCESS;
}
//write cpu card cpld space
int bsp_cpu_cpld_write_byte(IN  u8 value, IN u16 offset)
{
    if (offset > bsp_get_board_data()->cpld_size_cpu)
    {
        DBG_ECHO(DEBUG_ERR, "Invalid cpld offset 0x%x (> 0x%x)", offset, bsp_get_board_data()->cpld_size_cpu);
        return ERROR_FAILED;
    }
    //DBG_ECHO(DEBUG_DBG, "cpu cpld write offset 0x%x value 0x%x", offset, value);

    bsp_cpld_write_absolue_address(offset + g_u64CpuCpld_addr, value);
    
    return ERROR_SUCCESS;
}

//read subcard cpld space, slot_index from 0
int bsp_slot_cpld_read_byte(int slot_index, OUT u8 *value, IN u16 offset)
{
    if (offset > bsp_get_board_data()->cpld_size_slot[slot_index])
    {
        DBG_ECHO(DEBUG_ERR, "Invalid cpld offset 0x%x (> 0x%x)", offset, bsp_get_board_data()->cpld_size_slot[slot_index]);
        return ERROR_FAILED;
    }
    
    *value = bsp_cpld_read_absolue_address(offset + g_u64SlotCpld_addr[slot_index]);

    //DBG_ECHO(DEBUG_DBG, "slot %d cpld read offset 0x%x value 0x%x", slot_index + 1, offset, *value);
    return ERROR_SUCCESS;
}
//write subcard cpld space,  slot_index from 0 
int bsp_slot_cpld_write_byte(int slot_index, IN  u8 value, IN u16 offset)
{
    if (offset > bsp_get_board_data()->cpld_size_slot[slot_index])
    {
        DBG_ECHO(DEBUG_ERR, "Invalid cpld offset 0x%x (> 0x%x)", offset, bsp_get_board_data()->cpld_size_slot[slot_index]);
        return ERROR_FAILED;
    }
    //DBG_ECHO(DEBUG_DBG, "slot %d cpld write base/offset 0x%x/0x%x value 0x%x", slot_index + 1,  g_u64SlotCpld_addr[slot_index], offset, value);
    
    bsp_cpld_write_absolue_address(offset + g_u64SlotCpld_addr[slot_index], value);
    
    return ERROR_SUCCESS;
}


// board cpld
int bsp_slot_cpld_get_bit(int slot_index, u16 cpld_offset, u8 bit, u8 * value)
{
    int ret = ERROR_SUCCESS;

    if (bit > 7)
    {   
        DBG_ECHO(DEBUG_DBG, "param error, offset 0x%x bit %d > 7", cpld_offset, bit);
        return ERROR_FAILED;
    }

    ret = bsp_slot_cpld_read_byte(slot_index, value, cpld_offset);    
    CHECK_IF_ERROR_GOTO_EXIT(ret, "slot index %d cpld read 0x%x failed", slot_index, cpld_offset);
    *value = (*value >> bit) & 0x1;
    
exit:
    return ret;
}

//bsp_cpld_set_bit调用bsp_cpld_write_part，不用加锁
int bsp_slot_cpld_set_bit(int slot_index, u16 cpld_offset, u8 bit, u8 value)
{
    u8 temp_value = 0;
    int ret = ERROR_SUCCESS;
  
    if (((0 != value) && (1 != value)) || (bit > 7))
    {
        DBG_ECHO(DEBUG_ERR, "param error, cpld_offset=0x%x bit=%d value=0x%x", cpld_offset, bit, value);
        return ERROR_FAILED;
    }
    
    mutex_lock(&bsp_slot_cpld_lock[slot_index]);
    ret = bsp_slot_cpld_read_byte(slot_index, &temp_value, cpld_offset); 
    CHECK_IF_ERROR_GOTO_EXIT(ret, "slot index %d read cpld offset 0x%x failed", slot_index, cpld_offset);
    temp_value = (value == 1) ? ((u8)(1 << bit) | temp_value) : ((~((u8)(1 << bit))) & temp_value);

    ret = bsp_slot_cpld_write_byte(slot_index, temp_value, cpld_offset);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "slot index %d write cpld offset 0x%x failed", slot_index, cpld_offset);    
exit:
    mutex_unlock(&bsp_slot_cpld_lock[slot_index]);    
    return ret;
}


//子卡cpld初始化, slot  是子卡索引，从0开始
int bsp_slot_cpld_init(int slot_index)
{
    board_static_data *bd = bsp_get_board_data();
    if (slot_index >= MAX_SLOT_NUM)
    {
        DBG_ECHO(DEBUG_ERR, "failed, invalid slot index %d (max %d)", slot_index, MAX_SLOT_NUM);
        return ERROR_FAILED;
    }
    if (bd->cpld_size_slot[slot_index] <= 0)
    {
        DBG_ECHO(DEBUG_ERR, "failed, cpld size for slot %d is %d, invalid size.", slot_index, bd->cpld_size_slot[slot_index]);
        return ERROR_FAILED;
    }

    mutex_init(&bsp_slot_cpld_lock[slot_index]);

    if (bd->cpld_access_type == IO_REMAP)
    {
        g_u64SlotCpld_addr[slot_index] = ioremap(bd->cpld_base_address + bd->cpld_hw_addr_slot[slot_index], bd->cpld_size_slot[slot_index]);
    }
    else
    {
#if DIRECT_IO_SUPPORT
        g_u64SlotCpld_addr[slot_index] = (void *)bd->cpld_hw_addr_slot[slot_index];
#endif
    }
    DBG_ECHO(DEBUG_INFO, "slot %d cpld_address set to %p~%p\n", slot_index + 1, g_u64SlotCpld_addr[slot_index], g_u64SlotCpld_addr[slot_index] + bd->cpld_size_slot[slot_index]);

    return ERROR_SUCCESS;
}


int bsp_cpld_init(void)
{
    int slot_index = 0;
    char * access_type = NULL;
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    
    mutex_init(&bsp_cpld_lock);
    mutex_init(&bsp_mac_inner_temp_lock);

    if (bd->cpld_access_type == IO_REMAP)
    {
        g_u64Cpld_addr = ioremap(bd->cpld_base_address + bd->cpld_hw_addr_board, bd->cpld_size_board);
        g_u64CpuCpld_addr = ioremap(bd->cpld_base_address + bd->cpld_hw_addr_cpu, bd->cpld_size_cpu);
        access_type = "ioremap";
    }
    else
    {
#if DIRECT_IO_SUPPORT
        g_u64Cpld_addr = (void *)bd->cpld_hw_addr_board;
        g_u64CpuCpld_addr = (void *)bd->cpld_hw_addr_cpu; 
        access_type = "inout set";
#endif
    }
    
    DBG_ECHO(DEBUG_INFO, "  cpu cpld %s to %p~%p\n", access_type, g_u64CpuCpld_addr, g_u64CpuCpld_addr + bd->cpld_size_cpu);
    DBG_ECHO(DEBUG_INFO, "board cpld %s to %p~%p\n", access_type, g_u64Cpld_addr, g_u64Cpld_addr + bd->cpld_size_board);
    //每子卡上的cpld初始化
    for (slot_index = 0; slot_index < bd->slot_num; slot_index++)
    {
        ret = bsp_slot_cpld_init(slot_index);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "slot %d cpld init failed!", slot_index + 1);
    }
    
exit:

    return ret;
}



int bsp_cpld_deinit(void)
{
    int slot_index;
    iounmap(g_u64Cpld_addr);
    iounmap(g_u64CpuCpld_addr);

    DBG_ECHO(DEBUG_INFO, "board cpld iounmaped!");
    
    for (slot_index = 0; slot_index < bsp_get_board_data()->slot_num; slot_index++)
    {
        if (g_u64SlotCpld_addr[slot_index] != NULL)
        {
            iounmap(g_u64SlotCpld_addr[slot_index]);
            DBG_ECHO(DEBUG_INFO, "slot %d cpld iounmaped!", slot_index + 1);
        }
    }
    
    return ERROR_SUCCESS;
}

//cpld 还没有初始化时，尝试从0x2202位置获取板卡PCB类型数据
static int bsp_try_get_product_type_from_cpld(OUT u8 * pcb_type)
{
    u8 value_io_remap = 0x0;
    
    void * temp_remap = NULL;

#if DIRECT_IO_SUPPORT
    u8 value_io_inout = 0x0;
    value_io_inout = inb(DEFAULT_CPLD_BOARD_TYPE_OFFSET);
    if ((value_io_inout != 0) && (value_io_inout != 0xff))
    {
        *pcb_type = value_io_inout;
        DBG_ECHO(DEBUG_INFO, "try to get product type success by inb(), pcb_type=0x%x", value_io_inout)
        return ERROR_SUCCESS;
    }
#endif

    temp_remap = ioremap(DEFAULT_CPLD_BASE_ADDR_IOREMAP + DEFAULT_CPLD_BOARD_TYPE_OFFSET, 0x2);
    value_io_remap = readb(temp_remap);
    iounmap(temp_remap);
    if ((value_io_remap != 0) && (value_io_remap != 0xff))
    {
        *pcb_type = value_io_remap;
        DBG_ECHO(DEBUG_INFO, "try to get product type success by ioremap, pcb_type=0x%x", value_io_remap)
        return ERROR_SUCCESS;
    }
    DBG_ECHO(DEBUG_ERR, "try to get product type failed!")

    return ERROR_FAILED;
}
//get product type
int bsp_get_product_type(OUT int * pdt_type)
{   
    int ret = ERROR_SUCCESS;
    u8 cpld_pcb_type = 0;
    

    if (bsp_product_type == PDT_TYPE_BUTT)
    {
    //cpld还没有初始化, 尝试获取板卡PCB类型
        ret = bsp_try_get_product_type_from_cpld(&cpld_pcb_type);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "try get product from cpld failed, ret=%d", ret);
        DBG_ECHO(DEBUG_INFO, "got cpld pcb type 0x%x successed!", cpld_pcb_type);
        switch (cpld_pcb_type)
        {
            case 0x1:
            {
                bsp_product_type = PDT_TYPE_TCS81_120F_1U;
                break;
            }
            case 0x2:
            {
                bsp_product_type = PDT_TYPE_TCS83_120F_4U;
                break;
            }
            case 0x5:
            {
                bsp_product_type = PDT_TYPE_TCS82_120F_1U;
                break;
            }
            default:
            {
                bsp_product_type = PDT_TYPE_TCS81_120F_1U;
                break;
            }
        }
        
    }
    
    * pdt_type = bsp_product_type;
exit:
    
    return ret;
}


//获取子卡类型
int bsp_get_card_product_type(OUT int * card_pdt_type, int slot_index)
{   
    int ret = ERROR_SUCCESS;
    u8 cpld_pdt_type = 0;

    //board_static_data *bd = bsp_get_board_data();
    
    if (bsp_slot_cpld_read_byte(slot_index, &cpld_pdt_type, DEFAULT_SUBCARD_BOARD_TYPE_OFFSET) != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "get card product type failed for slot index %d", slot_index);
        return ERROR_FAILED;
    }
    switch (cpld_pdt_type)
    {
    case 0x0:
        {
            *card_pdt_type = PDT_TYPE_TCS83_120F_32H_SUBCARD;
            ret = ERROR_SUCCESS;
            break;
        }
        default:
        {
            DBG_ECHO(DEBUG_ERR, "unknown card type %d for slot index %d", cpld_pdt_type, slot_index);
            ret = ERROR_FAILED;
            break;
        }
    }
    return ret;
}


char * bsp_get_product_name_string(int product_type)
{
    char * pdt_name = NULL;
    
    switch (product_type)
    {
        case PDT_TYPE_TCS81_120F_1U:
        {
            pdt_name = "TCS81-120F";
            break;
        }
        case PDT_TYPE_TCS83_120F_4U:
        {
            pdt_name = "TCS83-120F";
            break;
        }
        case PDT_TYPE_TCS83_120F_32H_SUBCARD:
        {
            pdt_name = "TCS83-120-32CQ";
            break;
        }
        default:
        {
            pdt_name = "Unknown";
            break;
        }
    }
    return pdt_name;
}


int bsp_select_i2c_device_with_device_table(I2C_DEVICE_E i2c_device_index)
{
    int step_count = 0;
    i2c_select_op_step * temp_step;
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    
    if (i2c_device_index >= I2C_DEV_BUTT)
    {
        DBG_ECHO(DEBUG_ERR, "error for i2c device index %d, exceeded %d", i2c_device_index, I2C_DEV_BUTT);
        ret = ERROR_FAILED;
        goto exit;
    }
    if (bd->i2c_select_table[i2c_device_index].valid != I2C_SELECT_STEPS_VALID)
    {
        DBG_ECHO(DEBUG_ERR, "i2c select device %d steps is invalid!", i2c_device_index);
        ret = ERROR_FAILED;
        goto exit;
    }
    
    temp_step = bd->i2c_select_table[i2c_device_index].step;


    for (step_count = 0; step_count < MAX_I2C_SEL_OP_STEPS_COUNT; step_count++)
    {
        switch(temp_step[step_count].op_type)
        {
        case OP_TYPE_WR_9545:
            {
                ret = bsp_i2c_9545_write_byte(temp_step[step_count].i2c_dev_addr, 0, temp_step[step_count].op_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "operate 9545 failed, ret=%d", ret);
                break;
            }
            case OP_TYPE_WR_9548:
            {
                ret = bsp_i2c_9548_write_byte(temp_step[step_count].i2c_dev_addr, 0, temp_step[step_count].op_value);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "operate 9548 failed, ret=%d", ret);
                break;
            }
            case OP_TYPE_WR_CPLD:
            {
                ret = bsp_cpld_write_byte(temp_step[step_count].op_value, temp_step[step_count].cpld_offset_addr);
                CHECK_IF_ERROR_GOTO_EXIT(ret, "operate cpld failed, ret=%d", ret);
                break;
            }
            case OP_TYPE_NONE:
            {
                goto exit;
                break;
            }
            default:
            {
                DBG_ECHO(DEBUG_ERR, "error for op_type %d, failed", temp_step[step_count].op_type);
                goto exit;
                break;
            }
        }
    }

exit:
    if (ret != ERROR_SUCCESS)
    {
        u8 main_temp, t9548_rst, temp0, temp1, temp2, temp3;
        int ret2;
        ret2 = bsp_cpld_read_byte(&t9548_rst, bd->cpld_addr_9548_rst[0]);
        ret2 += bsp_cpld_read_byte(&main_temp, bd->cpld_addr_main_i2c_sel);
        ret2 += bsp_cpld_read_byte(&temp0, bd->cpld_addr_i2c_sel[0]);
        ret2 += bsp_cpld_read_byte(&temp1, bd->cpld_addr_i2c_sel[1]);
        ret2 += bsp_cpld_read_byte(&temp2, bd->cpld_addr_i2c_sel[2]);
        ret2 += bsp_cpld_read_byte(&temp3, bd->cpld_addr_i2c_sel[3]);
        
        DBG_ECHO(DEBUG_ERR, "i2c select device %d failed, ret=%d 9548_rst=0x%02x main_se=0x%02x sel[0]=0x%02x sel[1]=0x%02x sel[2]=0x%02x sel[3]=0x%02x" , 
            i2c_device_index, ret2,
            t9548_rst,
            main_temp,
            temp0,
            temp1,
            temp2,
            temp3
        );
        
    }
    
    return ret;
}




//lock i2c path, when access i2c
int lock_i2c_path(I2C_DEVICE_E i2c_device_index)
{
    int ret = ERROR_SUCCESS;
    int slot_index = -1;
    mutex_lock(&bsp_i2c_path_lock);
    current_i2c_path_id = i2c_device_index;

    msleep(1);

    //reset for optic i2c hang up
    if ((i2c_device_index >= I2C_DEV_OPTIC_IDX_START) && (i2c_device_index < I2C_DEV_OPTIC_BUTT))
    {
        slot_index = GET_SLOT_INDEX_BY_I2C_OPTIC_DEV_IDX(i2c_device_index);   
    
        if (bsp_enable_slot_all_9545(slot_index) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "reset 9545 failed for i2c dev %d", i2c_device_index);
        }
        if (bsp_enable_slot_all_9548(slot_index) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "reset 9548 failed for i2c dev %d", i2c_device_index);
        }
    }
    
    ret = bsp_select_i2c_device_with_device_table(i2c_device_index);
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "i2c path switching to device index=%d failed", i2c_device_index);
    }

    msleep(1);
    
    return ret;
}
//unlock i2c path ,after access i2c
void unlock_i2c_path(void)
{
    current_i2c_path_id = 0;
    mutex_unlock(&bsp_i2c_path_lock);
    return;
}

int bsp_i2c_get_smbus_status(void)
{
    void __iomem *smba = 0;
    u8 temp_value = 0;
    int ret = 0;
    struct pci_dev *pdev = pci_get_domain_bus_and_slot(0, 0x00, PCI_DEVFN(0x1f, 0x03));
    
    smba = pci_ioremap_bar(pdev, 0);
    if (!smba) {
       DBG_ECHO(DEBUG_ERR, "SMBus base address uninitialized, upgrade BIOS\n");
       return -ENOMEM;
    }
    DBG_ECHO(DEBUG_ERR, "smbus HST_STS Host Status  Register %#x,  HST_CNT Host Control Register %#x\n", readb(smba+0x00), readb(smba+0x02));    
    pci_iounmap(pdev, smba);
    ret = pci_read_config_byte(pdev, 0x40, &temp_value);
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "pci_read_config_byte failed ret=%d", ret);
        return -1;
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "Host Configuration Register=%#x", temp_value);
    }
    return 0;
}


void bsp_i2c_reset_smbus_host(void)
{
    int ret = 0;
    u8 temp_value = 0;
    struct pci_dev *pdev = pci_get_domain_bus_and_slot(0, 0x00, PCI_DEVFN(0x1f, 0x03));

    ret = pci_read_config_byte(pdev, 0x40, &temp_value);
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "pci_read_config_byte failed ret=%d", ret);
        return;
    }
    temp_value |= 8;
    ret = pci_write_config_byte(pdev, 0x40,temp_value);
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "pci_write_config_byte value=%d failed ret=%d", temp_value, ret);
        return;
    }
    udelay(1000);
    return;
}

/*******************************
start:
basic i2c device read/write method
eeprom, 954x, lm75, max669x, sfp...

********************************/
//包装i2c_smbus_xfer，用于记录i2c访问记录
int bsp_h3c_i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr, unsigned short flags, char read_write, u8 command, int protocol, union i2c_smbus_data *data)
{
    int ret = ERROR_SUCCESS;
    int retrys = I2C_RW_MAX_RETRY_COUNT + 1;
    int curr_index = 0;
    int try_count = 0;

    if ((adapter == NULL) || (data == NULL))
    {
        DBG_ECHO(DEBUG_ERR, "param error adapter == NULL or data == NULL");
        return ERROR_FAILED;
    }

    while (retrys-- > 0)
    {
        ret = i2c_smbus_xfer(adapter, addr, flags, read_write, command, protocol, data);
        try_count++;

        if (ret == ERROR_SUCCESS)
        {
            break;
        }
        else
        {
            bsp_i2c_reset_smbus_host();
            bsp_send_i2c_reset_signal();
            DBG_ECHO(DEBUG_INFO, "send i2c reset signal, try_count=%d addr=0x%x flags=%d, read_write=%d, command=0x%x", try_count, addr, flags, read_write, command);
            msleep(I2C_RW_MAX_RETRY_DELAY_MS);
        }
    }
    
    if (ret != ERROR_SUCCESS || ((ret == ERROR_SUCCESS) && (retrys < I2C_RW_MAX_RETRY_COUNT - 1)))
    {
        curr_index = i2c_diag_info.curr_index;
        i2c_diag_info.record[curr_index].error_code = ret;
        i2c_diag_info.record[curr_index].i2c_addr = addr;
        i2c_diag_info.record[curr_index].path_id = current_i2c_path_id;
        i2c_diag_info.record[curr_index].inner_addr = command;
        i2c_diag_info.record[curr_index].protocol = protocol;
        i2c_diag_info.record[curr_index].read_write = read_write;
        i2c_diag_info.record[curr_index].retry_times = try_count;
        i2c_diag_info.record[curr_index].time_sec = get_seconds();
        i2c_diag_info.rec_count = (i2c_diag_info.rec_count < MAX_I2C_DIAG_RECORD_COUNT) ? (i2c_diag_info.rec_count + 1) : MAX_I2C_DIAG_RECORD_COUNT;
        i2c_diag_info.curr_index = (curr_index  < MAX_I2C_DIAG_RECORD_COUNT - 1) ? (curr_index + 1) : 0;
        i2c_diag_info.is_valid = TRUE;

        if (ret != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "i2c_smbus_xfer failed(%d), addr=0x%x flags=%d, read_write=%d, command=0x%x, protocol=%d", ret, addr, flags, read_write, command, protocol);
        }
    }
    
    return ret;
}



//从eeprom设备读取内容。 dev_i2c_address, i2c总线地址, from_inner
//适用于 类似24LC128  的eeprom 
int bsp_i2c_24LC128_eeprom_read_bytes(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, OUT u8 * data)
{
    union i2c_smbus_data temp_data;
    size_t i;
    int status;
    temp_data.byte = (u8)(from_inner_address & 0xff);  //for 24LC128, the inner address is combined by 2 bytes(not 1). temp_data contains the lower address byte.
    
    //是否用一个block read 就搞定？有优化空间。由于内部地址使用双字节，比较麻烦
    //i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, 0, I2C_SMBUS_READ
    
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)(from_inner_address >> 8), I2C_SMBUS_BYTE_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "write eeprom address 0x%x inner 0x%x failed", dev_i2c_address, from_inner_address);

    for (i = 0; i < byte_count; i++)
    {
        status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &temp_data);
        data[i] = temp_data.byte;
        CHECK_IF_ERROR_GOTO_EXIT(status, "eeprom read %d failed %d", (int)i, status);
    }
    
exit:
    
    return status;
}

//eeprom for 24LC128,  地址采用2字节编址，读写需要特殊处理
int bsp_i2c_24LC128_eeprom_write_byte(u16 dev_i2c_address, u16 inner_address, u8 value)
{
    union i2c_smbus_data temp_data;
    int ret = ERROR_SUCCESS;
    int max_retry = EEPROM_WRITE_MAX_DELAY_MS;
    
    temp_data.word = (((u16)value) << 8) | (inner_address & 0xff);
    ret = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)(inner_address >> 8), I2C_SMBUS_WORD_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "failed to eeprom write 0x%x inner 0x%x", dev_i2c_address, inner_address);
    
    temp_data.byte = 0;
    while (max_retry-- > 0 && 0 > i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)(inner_address >> 8), I2C_SMBUS_BYTE_DATA, &temp_data))
    {
        //等待完全写入后继
        udelay(1000);
    }
    if (max_retry <= 0)
    {
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_ERR, "i2c_eeprom_write_byte reaches max retry %d, ret=%d", EEPROM_WRITE_MAX_DELAY_MS,ret);
    }
exit:
    
    return ret;
}

//一般的eeprom读, eeprom内部地址采用一字节
int bsp_i2c_common_eeprom_read_bytes(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, OUT u8 * data)
{
    union i2c_smbus_data temp_data = {0};
    size_t i;
    int status = ERROR_SUCCESS;
    int retry;

#if COMMON_EEPROM_BLOCK_READ
    //整块读取可以用
    
        int loop_count = byte_count / I2C_SMBUS_BLOCK_MAX;       //每次只能读block max个字节，32个
        int mod = byte_count % I2C_SMBUS_BLOCK_MAX;
        int temp_read_bytes = 0;
        u16 temp_from_address = from_inner_address;
        u8* date_pointer = data;
        
        for (i = 0; i < loop_count + 1; i++)
        {
            temp_read_bytes = (i == loop_count) ? mod : I2C_SMBUS_BLOCK_MAX;
            temp_data.block[0] = temp_read_bytes;
    
            retry = EEPROM_RW_MAX_RETRY_COUNT;
            while (retry-- > 0)
            {
                status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, temp_from_address, I2C_SMBUS_I2C_BLOCK_DATA, &temp_data);
                if (status == ERROR_SUCCESS)
                    break;
                
                DBG_ECHO(DEBUG_DBG, "eeprom read retry %d", retry);
                msleep(EEPROM_RW_RETRY_INTERVAL);
            }
    
            CHECK_IF_ERROR_GOTO_EXIT(status, "eeprom read address 0x%x inner 0x%x failed", dev_i2c_address, temp_from_address);
    
            memcpy(date_pointer, &temp_data.block[1], temp_read_bytes);
    
            temp_from_address += temp_read_bytes;
            date_pointer += temp_read_bytes;
        }
        
#else
    
    //单字节读
    
        status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)from_inner_address, I2C_SMBUS_BYTE, &temp_data);
        CHECK_IF_ERROR_GOTO_EXIT(status, "write eeprom address 0x%x inner 0x%x failed", dev_i2c_address, from_inner_address);
        
        for (i = 0; i < byte_count; i++)
        {
            retry = EEPROM_RW_MAX_RETRY_COUNT;
            while (retry-- > 0)
            {     
                temp_data.byte = 0;
                status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &temp_data);
                if (status == ERROR_SUCCESS)
                {
                    data[i] = temp_data.byte;
                    //printk(KERN_DEBUG"addr%d 0x%x(%c)",from_inner_address + i, data[i], data[i]);
                    break;
                }
                DBG_ECHO(DEBUG_DBG, "eeprom read byte %d failed retry=%d", (int)i, retry);
                msleep(EEPROM_RW_RETRY_INTERVAL);
            }
            if (status != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_INFO, "eeprom read byte %d failed ret=%d, retry=%d", (int)i, status, EEPROM_RW_MAX_RETRY_COUNT);
            }
        }
        
#endif

exit:
    return status;

}


int bsp_i2c_common_eeprom_write_byte(u16 dev_i2c_address, u16 inner_address, u8 data)
{
    union i2c_smbus_data temp_data;
    int ret = ERROR_SUCCESS;
    int max_retry = EEPROM_WRITE_MAX_DELAY_MS;
        
    temp_data.byte = data;
    ret = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)(inner_address), I2C_SMBUS_BYTE_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "failed to eeprom write 0x%x inner 0x%x", dev_i2c_address, inner_address);
     
    temp_data.byte = 0;
    while (max_retry-- > 0 && 0 > i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)(inner_address >> 8), I2C_SMBUS_BYTE_DATA, &temp_data))
    {//等待完全写入后继
        udelay(1000);
    }
    if (max_retry <= 0)
    {
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_ERR, "i2c_eeprom_write_byte reaches max retry %d, ret=%d", EEPROM_WRITE_MAX_DELAY_MS, ret);
    }
exit:
        
    return ret;

}

int bsp_i2c_common_eeprom_write_byte_adm1166(u16 dev_i2c_address, u16 inner_address, u8 data)
{
    union i2c_smbus_data temp_data;
    int ret = 0;
    temp_data.block[0] = 1;
    memcpy(&temp_data.block[1],&data,temp_data.block[0]);

    ret = bsp_h3c_i2c_smbus_xfer(smbus,dev_i2c_address,0,I2C_SMBUS_WRITE,inner_address,I2C_SMBUS_I2C_BLOCK_DATA,&temp_data);
    if(ret < 0)
    {
        return ERROR_FAILED;
    }
        
    return ERROR_SUCCESS;

}




//SFP eeprom
int bsp_i2c_SFP_read_bytes(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, OUT u8 * data)
{
    return bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, from_inner_address, byte_count, data);
}


//SFP eeprom
int bsp_i2c_SFP_write_byte(u16 dev_i2c_address, u16 from_inner_address, u8 data)
{
    return bsp_i2c_common_eeprom_write_byte(dev_i2c_address, from_inner_address, data);
}


//9545
int bsp_i2c_9545_write_byte(u16 dev_i2c_address, u16 inner_address, u8 value)
{
    union i2c_smbus_data temp_data;
    int status;
    (void)inner_address;
    
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, &temp_data);
    
    CHECK_IF_ERROR_GOTO_EXIT(status, "9545 write address 0x%x value 0x%x", dev_i2c_address, value);
    

exit:
    return status;
}

//9548
int bsp_i2c_9548_write_byte(u16 dev_i2c_address, u16 inner_address, u8 value)
{
    union i2c_smbus_data temp_data;
    int status;
    (void)inner_address;
    
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, &temp_data);

    CHECK_IF_ERROR_GOTO_EXIT(status, "9548 write address 0x%x value 0x%x", dev_i2c_address, value);
exit:
    return status;
}


//LM75 temp
int bsp_i2c_LM75_get_temp(u16 dev_i2c_address, s16 *value)
{
    int status;
    s16 temp_value;
    union i2c_smbus_data temp_data;

    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, REG_ADDR_LM75_TEMP, I2C_SMBUS_WORD_DATA, &temp_data);
    
    CHECK_IF_ERROR_GOTO_EXIT(status, "failed dev_addr:0x%x", dev_i2c_address); 
    
    //返回的字节高低是反的, 每个bit 0.5度
    temp_value = ((temp_data.block[1] << 0) | (temp_data.block[0] << 8)) >> 7;
    //bit 8是符号位, 正数直接取低8bit ，负数扩展称号位
    temp_value = ((temp_value & 0x100) == 0) ? (0xff & temp_value) : (0xff00 | temp_value); 

    //返回温度为0.5精度有符号数
    * value = temp_value;
    //*value = temp_data.word;

exit:
    return status;
}


//读6696寄存器
int bsp_i2c_Max6696_reg_read(u16 dev_i2c_address, u16 inner_address, u8 *value)
{
    int status;
    union i2c_smbus_data temp_data;
    //bsp_cpld_reset_max6696();
    
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, inner_address, I2C_SMBUS_BYTE_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "failed dev_add:0x%x  inner_addr:0x%x", dev_i2c_address, inner_address);
    *value = temp_data.byte;
    
exit:
    return status;
}

//写6696配置寄存器
int bsp_i2c_Max6696_reg_write(u16 dev_i2c_address, u16 inner_address, u8 value)
{
    int status;
    union i2c_smbus_data temp_data;
    temp_data.byte = value;
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)inner_address, I2C_SMBUS_BYTE_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "failed dev_add:0x%x  inner_addr:0x%x", dev_i2c_address, inner_address);
    
exit:
    return status;
}

//选择max6696远端2个温度点中的一个
int bsp_i2c_Max6696_select_remote_spot_channel(u16 dev_i2c_address, MAX6696_SPOT_INDEX channel_index)
{
    u8 config = 0;
    int status = ERROR_SUCCESS;
    if (channel_index != MAX6696_REMOTE_CHANNEL1_SOPT_INDEX && channel_index != MAX6696_REMOTE_CHANNEL2_SOPT_INDEX)
    {
        CHECK_IF_ERROR_GOTO_EXIT((status=ERROR_FAILED), "remote channel %d index error", channel_index);
    }
    status = bsp_i2c_Max6696_reg_read(dev_i2c_address, REG_ADDR_MAX6696_READ_CONFIG, &config);
    CHECK_IF_ERROR_GOTO_EXIT(status, "max 6696 reg read failed, dev addr:0x%x, channel_index:0x%x", dev_i2c_address, channel_index);   
    config = channel_index == MAX6696_REMOTE_CHANNEL1_SOPT_INDEX ? (config & (~((u8)1 << MAX6696_SPOT_SELECT_BIT)) ): (config | ((u8)1 << MAX6696_SPOT_SELECT_BIT));
    status = bsp_i2c_Max6696_reg_write(dev_i2c_address, REG_ADDR_MAX6696_WRITE_CONFIG, config);
    CHECK_IF_ERROR_GOTO_EXIT(status, "max 6696 reg write failed, dev addr:0x%x, channel_index:0x%x", dev_i2c_address, channel_index);

exit:
    
    return status;   
}

//max 6696 读其中spot index的温度点寄存器值，spot index = [0,1,2]
int bsp_i2c_Max6696_get_temp(u16 dev_i2c_address, MAX6696_SPOT_INDEX spot_index, s8 *value)
{
    int status;
    union i2c_smbus_data temp_data;
    //u8 config = 0;
    u16 inner_address = 0;
    switch(spot_index)
    {
    case MAX6696_LOCAL_SOPT_INDEX:
        {
            inner_address = REG_ADDR_MAX6696_TEMP_LOCAL;
            break;
        }
        case MAX6696_REMOTE_CHANNEL1_SOPT_INDEX:
        case MAX6696_REMOTE_CHANNEL2_SOPT_INDEX:
        {
            status = bsp_i2c_Max6696_select_remote_spot_channel(dev_i2c_address, spot_index);
            CHECK_IF_ERROR_GOTO_EXIT(status, "select remote spot %d channel failed", spot_index);
            inner_address = REG_ADDR_MAX6696_TEMP_REMOTE;
            break;
        }
        default:
        {
            status = ERROR_FAILED;
            CHECK_IF_ERROR_GOTO_EXIT(status, "max6696 has only %d spot, not support spot index %d", MAX6696_SPOT_NUM, spot_index);
            break;
        }
    }
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, inner_address, I2C_SMBUS_BYTE_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "failed dev_add:0x%x  inner_addr:0x%x", dev_i2c_address, inner_address);
    
    *value = temp_data.byte;
    
exit:
    return status;
}

//读写max6696的温度门限寄存器
int bsp_i2c_Max6696_limit_rw(REG_RW read_write, u16 dev_i2c_address, MAX6696_LIMIT_INDEX limit_index, s8 *value)
{
    u8 inner_addr = 0;
    int status = ERROR_SUCCESS;
    int select_channel = -1;
    u8 temp_value = 0;
    switch(limit_index)
    {
        case MAX6696_LOCAL_HIGH_ALERT:
        {
            inner_addr = REG_ADDR_MAX6696_READ_ALERT_HI_LOCAL;
            break;
        }
        case MAX6696_LOCAL_LOW_ALERT:
        {
            inner_addr = REG_ADDR_MAX6696_READ_ALERT_LO_LOCAL;
            break;
        }
        case MAX6696_LOCAL_OT2_LIMIT:
        case SET_MAX6696_LOCAL_OT2_LIMIT:
        {
            inner_addr = REG_ADDR_MAX6696_RW_OT2_LOCAL;
            break;
        }
        case MAX6696_REMOTE_CHANNEL1_HIGH_ALERT:
        case MAX6696_REMOTE_CHANNEL2_HIGH_ALERT:
        {
            select_channel = (limit_index == MAX6696_REMOTE_CHANNEL1_HIGH_ALERT) ? MAX6696_REMOTE_CHANNEL1_SOPT_INDEX : MAX6696_REMOTE_CHANNEL2_SOPT_INDEX;
            inner_addr = REG_ADDR_MAX6696_READ_ALERT_HI_REMOTE;
            break;
        }
        case MAX6696_REMOTE_CHANNEL1_LOW_ALERT:
        case MAX6696_REMOTE_CHANNEL2_LOW_ALERT:
        {
            select_channel = (limit_index == MAX6696_REMOTE_CHANNEL1_LOW_ALERT) ? MAX6696_REMOTE_CHANNEL1_SOPT_INDEX : MAX6696_REMOTE_CHANNEL2_SOPT_INDEX;
            inner_addr = REG_ADDR_MAX6696_READ_ALERT_LO_REMOTE;
            break;
        }
        case MAX6696_REMOTE_CHANNEL1_OT2_LIMIT:
        case MAX6696_REMOTE_CHANNEL2_OT2_LIMIT:
        case SET_MAX6696_REMOTE_CHANNEL1_OT2_LIMIT:
        case SET_MAX6696_REMOTE_CHANNEL2_OT2_LIMIT:
        {
            select_channel = ((limit_index == MAX6696_REMOTE_CHANNEL1_OT2_LIMIT) || (limit_index == SET_MAX6696_REMOTE_CHANNEL1_OT2_LIMIT)) ? MAX6696_REMOTE_CHANNEL1_SOPT_INDEX : MAX6696_REMOTE_CHANNEL2_SOPT_INDEX;
            inner_addr = REG_ADDR_MAX6696_RW_OT2_REMOTE;
            break;
        }
        case SET_MAX6696_LOCAL_HIGH_ALERT:
        {
            inner_addr = REG_ADDR_MAX6696_WRITE_ALERT_HI_LOCAL;
            break;
        }
        case SET_MAX6696_LOCAL_LOW_ALERT:
        {
            inner_addr = REG_ADDR_MAX6696_WRITE_ALERT_LO_LOCAL;
            break;
        }
        case SET_MAX6696_REMOTE_CHANNEL1_HIGH_ALERT:
        case SET_MAX6696_REMOTE_CHANNEL2_HIGH_ALERT:
        {
            select_channel = (limit_index == SET_MAX6696_REMOTE_CHANNEL1_HIGH_ALERT) ? MAX6696_REMOTE_CHANNEL1_SOPT_INDEX : MAX6696_REMOTE_CHANNEL2_SOPT_INDEX;
            inner_addr = REG_ADDR_MAX6696_WRITE_ALERT_HI_REMOTE;
            break;
        }
        case SET_MAX6696_REMOTE_CHANNEL1_LOW_ALERT:
        case SET_MAX6696_REMOTE_CHANNEL2_LOW_ALERT:
        {
            select_channel = (limit_index == SET_MAX6696_REMOTE_CHANNEL1_LOW_ALERT) ? MAX6696_REMOTE_CHANNEL1_SOPT_INDEX : MAX6696_REMOTE_CHANNEL2_SOPT_INDEX;
            inner_addr = REG_ADDR_MAX6696_WRITE_ALERT_LO_REMOTE;
            break;
        }
        default:
        {
            status = ERROR_FAILED;
            CHECK_IF_ERROR_GOTO_EXIT(status, "not support limit index %d", limit_index);
            break;
        }
    }
    
    if (select_channel != -1)
    {
        status = bsp_i2c_Max6696_select_remote_spot_channel(dev_i2c_address, select_channel);
        CHECK_IF_ERROR_GOTO_EXIT(status, "select remote spot %d channel failed", select_channel);
    }
    if (read_write == REG_READ)
    {
        status = bsp_i2c_Max6696_reg_read(dev_i2c_address, inner_addr, &temp_value);
        CHECK_IF_ERROR_GOTO_EXIT(status, "max6696 reg read failed, i2c_addr=0x%x, inner_addr=0x%x", dev_i2c_address, inner_addr);
        *value = (s8)temp_value;
    } 
    else if (read_write == REG_WRITE)
    {
        temp_value = (u8)(*value);
        status = bsp_i2c_Max6696_reg_write(dev_i2c_address, inner_addr, temp_value);
        CHECK_IF_ERROR_GOTO_EXIT(status, "max6696 reg write failed, i2c_addr=0x%x, inner_addr=0x%x, value=0x%x", dev_i2c_address, inner_addr, temp_value); 
    }
    //DBG_ECHO(DEBUG_DBG, "select_channel=%d inner_addr=0x%x value=0x%x", select_channel, inner_addr, *value);
exit:
    return status;
}

//psu 650w method
int bsp_i2c_power_reg_read(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, u8 *value)
{

#if 0
    union i2c_smbus_data temp_data;
    size_t i;
    int status;
    
    status = i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, from_inner_address>>8, I2C_SMBUS_WORD_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "write eeprom address 0x%x inner 0x%x failed", dev_i2c_address, from_inner_address);
    value[0] = temp_data.word & 0xff;
    value[1] = (temp_data.word & 0xff00) >> 8;

exit:
    return status;
#else
    return bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, from_inner_address, byte_count, value);
#endif
}

//读取电源输出电流
int bsp_i2c_power650W_read_current(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW650W_IOUT, byte_count, value);
}
//读取电源输出电压
int bsp_i2c_power650W_read_voltage(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW650W_VOUT, byte_count, value);
}
int bsp_i2c_power650W_read_temperstatus(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_TSTATUS, byte_count, value);
}
int bsp_i2c_power650W_read_temper(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_TEMPER, byte_count, value);
}
int bsp_i2c_power650W_read_fan_speed(u16 dev_i2c_address, size_t byte_count, int fan_index, u8 *value)
{
    if (fan_index == 0)
        return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_FAN_1, byte_count, value);
    else
        return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_FAN_2, byte_count, value);
}
int bsp_i2c_power650W_read_SN(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_SN, byte_count, value);
}
int bsp_i2c_power650W_read_pdtname(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
     return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_PRONUMB, byte_count, value);
}
int bsp_i2c_power650W_read_vendorname(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_VENDOR, byte_count, value);
}

int bsp_i2c_power650W_read_powerin(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW650W_PIN, byte_count, value);
}
int bsp_i2c_power650W_read_powerout(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW650W_POUT, byte_count, value);
}
int bsp_i2c_power650W_read_MFR_ID(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW650W_MFR_ID, byte_count, value);
}
int bsp_i2c_power650W_read_hw_version(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW650W_HW_VER, byte_count, value);
}
int bsp_i2c_power650W_read_fw_version(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW650W_FW_VER, byte_count, value);
}



/*
int bsp_i2c_power650W_read_voltage_in(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_VIN, byte_count, value);
}

int bsp_i2c_power650W_read_current_in(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_IIN, byte_count, value);
}
*/


//1600W电源接口

//读取电源输出电压
int bsp_i2c_power1600W_read_current(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW1600W_IOUT, byte_count, value);
}
//读取电源输出电压
int bsp_i2c_power1600W_read_voltage(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address, REG_ADDR_PW1600W_VOUT, byte_count, value);    
}
int bsp_i2c_power1600W_read_SN(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600_SN, byte_count, value);
}
int bsp_i2c_power1600W_read_pdtname(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_PDTNAME, byte_count, value);
}
int bsp_i2c_power1600W_read_fan_speed(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_FAN, byte_count, value);
}
int bsp_i2c_power1600W_read_temperstatus(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_TSTATUS, byte_count, value);
}
int bsp_i2c_power1600W_read_temper(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_TEMPER, byte_count, value);
}
int bsp_i2c_power1600W_read_powerin(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_PIN, byte_count, value);
}
int bsp_i2c_power1600W_read_powerout(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_POUT, byte_count, value);
}
int bsp_i2c_power1600W_read_voltage_in(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_VIN, byte_count, value);
}
int bsp_i2c_power1600W_read_current_in(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_IIN, byte_count, value);
}

int bsp_i2c_power1600W_read_MFR_ID(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_MFR_ID, byte_count, value);
}

int bsp_i2c_power1600W_read_hw_version(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_HW_VER, byte_count, value);
}
int bsp_i2c_power1600W_read_fw_version(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_FW_VER, byte_count, value);
}
int bsp_i2c_power1600W_read_voltage_in_type(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_INVOL_TYPE, byte_count, value);
}
int bsp_i2c_power1600W_read_vendorname(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW1600W_VENDOR, byte_count, value);
}
int bsp_i2c_power_read_status_word(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_power_reg_read(dev_i2c_address,REG_ADDR_PW650W_WORDTATUS, byte_count, value);
}

int bsp_i2c_adm116x_rrctrl_operation(u16 dev_i2c_address,u32 uiOperation)
{
    u8 ucData = 0;
    int ulRet = ERROR_SUCCESS;
   // int uiDatalen = sizeof(ucData);

    ulRet = bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, REG_ADDR_ADM1166_RRCTRL, 1, &ucData);
   
    CHECK_IF_ERROR_GOTO_EXIT(ulRet, "failed i2c read dev_addr:0x%x reg_addr:0x%x", dev_i2c_address,REG_ADDR_ADM1166_RRCTRL); 
    
    if(ADM1166_RRCTRL_OPERATION_ENABLE == uiOperation)
    {
        ucData = (ADM1166_RRCTRL_REG_GO | ADM1166_RRCTRL_REG_ENABLE);
    }
    
    else if(ADM1166_RRCTRL_OPERATION_STOPWRITE == uiOperation)
    {
        ucData = (ADM1166_RRCTRL_REG_GO | ADM1166_RRCTRL_REG_STOPWRITE);
    }
    //DBG_ECHO(DEBUG_ERR, "drv_ft_adm116x_rrctrl_operation: ucData =%d", ucData);
    ulRet = bsp_i2c_common_eeprom_write_byte_adm1166(dev_i2c_address, REG_ADDR_ADM1166_RRCTRL, ucData);
   
    CHECK_IF_ERROR_GOTO_EXIT(ulRet, "failed i2c write dev_addr:0x%x reg_addr:0x%x", dev_i2c_address,REG_ADDR_ADM1166_RRCTRL); 
    
    return ERROR_SUCCESS;
exit:
    return ulRet;
}


int bsp_get_secondary_voltage_value(u16 dev_i2c_address,int uiChanNo,int* data)
{
    int ret = 0;
    u8 ucData[2] = {0};
    ret = bsp_i2c_adm116x_rrctrl_operation(dev_i2c_address,ADM1166_RRCTRL_OPERATION_STOPWRITE);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "drv_ft_adm116x_rrctrl_operation failed");
    
    ret = bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, REG_ADDR_DEV_ADM1166_BASE+uiChanNo*2, 2, ucData);  
    CHECK_IF_ERROR_GOTO_EXIT(ret, "failed i2c read dev_addr:0x%x reg_addr:0x%x", dev_i2c_address,REG_ADDR_ADM1166_RRCTRL); 
    
    
    
    *data = (((u32)(ucData[0]))<<8) + ((u32)(ucData[1]));
    //DBG_ECHO(DEBUG_ERR, "get_secondary_voltage_value: ucData =%d", *data);
exit:
    bsp_i2c_adm116x_rrctrl_operation(dev_i2c_address,ADM1166_RRCTRL_OPERATION_ENABLE);
    return ret;
}


/*
//风扇
 int bsp_i2c_fan_reg_read(u16 dev_i2c_address, u16 from_inner_address, size_t byte_count, u8 *value)
 {
 
#if 0
     union i2c_smbus_data temp_data;
     size_t i;
     int status;
     
     status = i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, from_inner_address>>8, I2C_SMBUS_WORD_DATA, &temp_data);
     CHECK_IF_ERROR_GOTO_EXIT(status, "write eeprom address 0x%x inner 0x%x failed", dev_i2c_address, from_inner_address);
     value[0] = temp_data.word & 0xff;
     value[1] = (temp_data.word & 0xff00) >> 8;
 
 exit:
     return status;
#else
     return bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, from_inner_address, byte_count, value);
#endif
 }


  int bsp_i2c_fan_read_SN(u16 dev_i2c_address, size_t byte_count, u8 *value)
{
    return bsp_i2c_fan_reg_read(dev_i2c_address,REG_ADDR_FAN_SN, byte_count, value);
}

 int bsp_i2c_fan_read_pdtname(u16 dev_i2c_address, size_t byte_count, u8 *value)
 {
     return bsp_i2c_fan_reg_read(dev_i2c_address,REG_ADDR_FAN_PRONUMB, byte_count, value);
 }
 */


//读ina219 //未完成
int bsp_i2c_ina219_read_reg(u16 dev_i2c_address, u16 inner_address, u16 *value)
{

    int status;
    union i2c_smbus_data temp_data;
    
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_READ, inner_address, I2C_SMBUS_WORD_DATA, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "failed dev_addr:0x%x", dev_i2c_address); 

    *value = temp_data.word;
    
exit:
    return status;
    //return bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, inner_address,byte_count,value);
}




//读isl68127
int bsp_i2c_isl68127_read_reg(u16 dev_i2c_address, u16 command_code , u16 *value, int read_byte_count)
{
    u8 temp_value[ISL68127_REG_VALUE_MAX_LEN] = {0};
    int ret = ERROR_SUCCESS;
    if (read_byte_count == 0 || read_byte_count > ISL68127_REG_VALUE_MAX_LEN)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "isl68127 i2c_addr 0x%x read command 0x%x failed for byte count %d", dev_i2c_address, command_code, read_byte_count);   
    }
    
    CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, command_code, read_byte_count, temp_value), "isl68127 i2c_addr 0x%x read command 0x%x failed", dev_i2c_address, command_code);
    *value = read_byte_count == 2 ? (temp_value[0] | ((u16)temp_value[1] << 8)) : temp_value[0];
exit:
    return ret;
}

//写isl68127
int bsp_i2c_isl68127_write_reg(u16 dev_i2c_address, u16 command_code ,u16 value, int write_byte_count)
{
    int status = ERROR_SUCCESS;
    int smbus_write_size = 0;
    union i2c_smbus_data temp_data;
    
    if (write_byte_count == 1)
    {
        temp_data.byte = (u8)value;
        smbus_write_size = I2C_SMBUS_BYTE_DATA;
    }
    else if (write_byte_count == 2)
    {
        temp_data.word = value;
        smbus_write_size = I2C_SMBUS_WORD_DATA;
    }
    else
    {
        CHECK_IF_ERROR_GOTO_EXIT(status = ERROR_FAILED, "invalid once write byte count %d", write_byte_count);
        goto exit;
    }
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)command_code, smbus_write_size, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "failed dev_add:0x%x  inner_addr:0x%x", dev_i2c_address, command_code);

exit:
    return status;
}


//设置mac电压
int bsp_set_mac_rov(void)
{
#define RETRY_COUNT 3

    int i = 0;
    int ret = ERROR_SUCCESS;
    u8 mac_rov_cpld_value = 0;
    u16 voltage_to_set = 0;
    board_static_data *bd = bsp_get_board_data();

    
    for (i = 0; i < RETRY_COUNT; i ++)
    {
        ret = bsp_cpld_read_byte(&mac_rov_cpld_value, bd->cpld_addr_mac_rov);
        if (ret != ERROR_SUCCESS)
        {
             DBG_ECHO(DEBUG_ERR, "cpld read mac rov (0x%x) failed", bd->cpld_addr_mac_rov);
             continue;
        }
        voltage_to_set = (u16)((160000 - (mac_rov_cpld_value - 2) * 625) / 100);
        if ((voltage_to_set > bd->mac_rov_max_voltage) || (voltage_to_set < bd->mac_rov_min_voltage))
        {
            //从cpld里读出数值不在合理范围，设置成默认值
            DBG_ECHO(DEBUG_ERR, "cpld mac rov calc voltage = %d, set to default %d", voltage_to_set, bd->cpld_addr_mac_rov);
            voltage_to_set = bd->mac_rov_default_voltage;
        }
        else
        {
            break;
        }
    }
    
    if (lock_i2c_path(I2C_DEV_ISL68127) == ERROR_SUCCESS)
    {
        for (i = 0; i < RETRY_COUNT; i ++)
        {
            if ((ret=bsp_i2c_isl68127_write_reg(bd->i2c_addr_isl68127[0], REG_ADDR_ISL68127_CMD_PAGE, 1, 1)) == ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_INFO, "isl68127 write page 1 success! i=%d", i);
                break;    
            }
            else
            {
                DBG_ECHO(DEBUG_ERR, "isl68127 write page 1 failed! i=%d", i);
            }
        }
        CHECK_IF_ERROR_GOTO_EXIT(ret, "isl68127 page select failed, abort to set voltage!");
        
        for (i = 0; i < RETRY_COUNT; i ++)
        {
            if ((ret=bsp_i2c_isl68127_write_reg(bd->i2c_addr_isl68127[0], REG_ADDR_ISL68127_CMD_VOUT, voltage_to_set, 2)) == ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_INFO, "isl68127 write voltage 0x%x success! i=%d", (int)voltage_to_set, i);
                break;    
            }
            else
            {
                DBG_ECHO(DEBUG_ERR, "isl68127 write voltage 0x%x failed! i=%d", (int)voltage_to_set, i);
            }
        }
    }
exit:
    unlock_i2c_path();
    
    return ret;

#undef RETRY_COUNT
}
//EXPORT_SYMBOL(bsp_set_mac_rov);


int bsp_i2c_tps53679_write_reg(u16 dev_i2c_address, u16 command_code ,u16 value, int write_byte_count)
{
    int status = ERROR_SUCCESS;
    int smbus_write_size = 0;
    union i2c_smbus_data temp_data;
    
    if (write_byte_count == 1)
    {
        temp_data.byte = (u8)value;
        smbus_write_size = I2C_SMBUS_BYTE_DATA;
    }
    else if (write_byte_count == 2)
    {
        temp_data.word = value;
        smbus_write_size = I2C_SMBUS_WORD_DATA;
    }
    else
    {
        CHECK_IF_ERROR_GOTO_EXIT(status = ERROR_FAILED, "invalid once write byte count %d", write_byte_count);
        goto exit;
    }
 	
    status = bsp_h3c_i2c_smbus_xfer(smbus, dev_i2c_address, 0, I2C_SMBUS_WRITE, (u8)command_code, smbus_write_size, &temp_data);
    CHECK_IF_ERROR_GOTO_EXIT(status, "failed dev_add:0x%x  inner_addr:0x%x", dev_i2c_address, command_code);
exit:
    return status;
}

EXPORT_SYMBOL (bsp_i2c_tps53679_write_reg);

int bsp_i2c_tps53679_read_reg(u16 dev_i2c_address, u16 command_code , u16 *value, int read_byte_count)
{

    u8 temp_value[ISL68127_REG_VALUE_MAX_LEN] = {0};
    int ret = ERROR_SUCCESS;
    
    if (read_byte_count == 0 || read_byte_count > ISL68127_REG_VALUE_MAX_LEN)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "tps53679 i2c_addr 0x%x read command 0x%x failed for byte count %d", dev_i2c_address, command_code, read_byte_count);   
    }
    
    CHECK_IF_ERROR_GOTO_EXIT(ret = bsp_i2c_common_eeprom_read_bytes(dev_i2c_address, command_code, read_byte_count, temp_value), "isl68127 i2c_addr 0x%x read command 0x%x failed", dev_i2c_address, command_code);
    *value = read_byte_count == 2 ? (temp_value[0] | ((u16)temp_value[1] << 8)) : temp_value[0];
exit:
    return ret;
    
}

EXPORT_SYMBOL (bsp_i2c_tps53679_read_reg);

int bsp_set_cpu_init_ok(u8 bit)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_cpu_init_ok, "mainboard cpu_init_ok reg is not defined!");
    ret = bsp_cpld_set_bit(bd->cpld_addr_cpu_init_ok, bd->cpld_offs_cpu_init_ok, bit);

exit:
    return ret;
}


/*******************************
end for basic i2c device method

********************************/



/*************************************
start for cpld device method
**************************************/


int bsp_cpld_get_fan_pwm_reg(OUT u8 * pwm)
{
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_read_byte(pwm, bd->cpld_addr_fan_pwm);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "pwm read error"); 
    CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(*pwm, fan_pwm);
    //DBG_ECHO(DEBUG_DBG, "pwm = 0x%x", *pwm);
    
exit:
    return ret;
}


int bsp_cpld_set_fan_pwm_reg(IN u8 pwm)
{
    board_static_data *bd = bsp_get_board_data();
    return bsp_cpld_write_byte(pwm, bd->cpld_addr_fan_pwm);
}

//获取风扇对应的马达转速
int bsp_cpld_get_fan_speed(OUT u16 * speed, int fan_index, int moter_index)
{
    u8 speed_low = 0;
    u8 speed_high = 0;
    u16 temp_speed = 0;
    int moter_abs_index = 0;
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();

    if (bd->product_type == PDT_TYPE_TCS81_120F_1U)
    {
        //cpld高位对应风扇小编号
        moter_abs_index = (bd->fan_num - fan_index - 1) * bd->motors_per_fan + moter_index;
    }
    else if (bd->product_type == PDT_TYPE_TCS82_120F_1U)
    {
        moter_abs_index = fan_index * bd->motors_per_fan + moter_index;
    }
    else
    {
        moter_abs_index = fan_index * bd->motors_per_fan + (!moter_index);
    }
    mutex_lock(&bsp_fan_speed_lock);
    
    //选风扇
    bsp_cpld_write_part(moter_abs_index, bd->cpld_addr_fan_select, bd->cpld_mask_fan_select, bd->cpld_offs_fan_select);
        
    ret = bsp_cpld_read_byte(&speed_low, bd->cpld_addr_fan_speed[CPLD_FAN_SPEED_LOW_REG_INDEX]);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "speed_low read error"); 
    ret = bsp_cpld_read_byte(&speed_high, bd->cpld_addr_fan_speed[CPLD_FAN_SPEED_HIGH_REG_INDEX]);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "speed_high read error"); 
    CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(speed_low, fan_speed[CPLD_FAN_SPEED_LOW_REG_INDEX]);
    CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(speed_high, fan_speed[CPLD_FAN_SPEED_HIGH_REG_INDEX]);

    if ((speed_high == CODE_FAN_MOTER_STOP) && (speed_low == CODE_FAN_MOTER_STOP))
    {
        *speed = 0;
    }
    else
    {
        temp_speed = ((((u16) speed_high) << 8 ) | ((u16)speed_low));
        *speed = temp_speed == 0 ? 0 : bd->fan_speed_coef / temp_speed;         
    }
exit:
    mutex_unlock(&bsp_fan_speed_lock);
    return ret;
}


int bsp_cpld_get_fan_enable(OUT u8 * enable, int fan_index)
{
    //int ret = bsp_cpld_read_byte(enable, main_board_data.cpld_addr_fan_enable[fan_index]);
    //CHECK_IF_ERROR_GOTO_EXIT(ret, "speed_enable read error"); 
    //CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(*enable, fan_enable[fan_index]);

    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_get_bit(bd->cpld_addr_fan_enable[fan_index], bd->cpld_offs_fan_enable[fan_index], enable);
    return ret;
}


int bsp_cpld_set_fan_enable(IN u8 enable, int fan_index)
{
    //int ret = bsp_cpld_write_byte(enable, main_board_data.cpld_addr_fan_enable);
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_set_bit(bd->cpld_addr_fan_enable[fan_index], bd->cpld_offs_fan_enable[fan_index], enable);
    return ret;
}


int bsp_cpld_get_fan_led_red(OUT u8 * led, int fan_index)
{
    ///=int ret = bsp_cpld_read_byte(led, main_board_data.cpld_addr_fan_led);
    //CHECK_IF_ERROR_GOTO_EXIT(ret, "led reg read error"); 
    //CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(*led, fan_led);
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_get_bit(bd->cpld_addr_fan_led_red[fan_index], bd->cpld_offs_fan_led_red[fan_index], led);

    return ret;
}

int bsp_cpld_set_fan_led_red(IN u8 led, int fan_index)
{
    //int ret = bsp_cpld_write_byte(led, main_board_data.cpld_addr_fan_led);
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_set_bit(bd->cpld_addr_fan_led_red[fan_index], bd->cpld_offs_fan_led_red[fan_index], led);
    
    return ret;
}


int bsp_cpld_get_fan_led_green(OUT u8 * led, int fan_index)
{
    ///=int ret = bsp_cpld_read_byte(led, main_board_data.cpld_addr_fan_led);
    //CHECK_IF_ERROR_GOTO_EXIT(ret, "led reg read error"); 
    //CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(*led, fan_led);
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_get_bit(bd->cpld_addr_fan_led_green[fan_index], bd->cpld_offs_fan_led_green[fan_index], led);

    return ret;
}

int bsp_cpld_set_fan_led_green(IN u8 led, int fan_index)
{
    //int ret = bsp_cpld_write_byte(led, main_board_data.cpld_addr_fan_led);
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_set_bit(bd->cpld_addr_fan_led_green[fan_index], bd->cpld_offs_fan_led_green[fan_index], led);
    
    return ret;
}


int bsp_cpld_get_fan_absent(OUT u8 * absent, int fan_index)
{
    //int ret = bsp_cpld_read_byte(absent, main_board_data.cpld_addr_fan_absent);
    //CHECK_IF_ERROR_GOTO_EXIT(ret, "speed_low read error"); 
    //CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(*absent, fan_absent);
    board_static_data *bd = bsp_get_board_data();
    int ret = bsp_cpld_get_bit(bd->cpld_addr_fan_absent[fan_index], bd->cpld_offs_fan_absent[fan_index], absent);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get_fan_absent failed");
    
exit:

    return ret;
}

//只支持2个状态寄存器,返回2个寄存器的合成值
int bsp_cpld_get_fan_status(OUT u8 * status, int fan_index)
{
    u8 status_0 = 0;
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    ret = bsp_cpld_read_byte(&status_0, bd->cpld_addr_fan_status[fan_index]);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get reg failed");
    CPLD_TRANS_VALUE_WITH_MASK_AND_OFFSET(status_0, fan_status[fan_index]);

    *status = status_0;
exit:
    return ret;
}


//电源
int bsp_cpld_get_psu_absent(OUT u8 *absent, int psu_index)
{
    //u8 status_0 = 0;
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();

    if (psu_index >= bd->psu_num)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "psu_index %d >= psu_num %d", psu_index, (int)bd->psu_num);
    }
    ret = bsp_cpld_get_bit(bd->cpld_addr_psu_absent[psu_index], bd->cpld_offs_psu_absent[psu_index], absent);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get psu absent bit failed!");

    DBG_ECHO(DEBUG_DBG, "psu %d absent=%d", psu_index + 1, *absent);
exit:
    return ret;
}


int bsp_cpld_get_psu_good(OUT u8 *good, int psu_index)
{
    //u8 status_0 = 0;
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();

    if (psu_index >= bd->psu_num)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "psu_index %d >= psu_num %d", psu_index, (int)bd->psu_num);
    }
    ret = bsp_cpld_get_bit(bd->cpld_addr_psu_good[psu_index], bd->cpld_offs_psu_good[psu_index], good);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get psu absent bit failed!");

    DBG_ECHO(DEBUG_DBG, "psu %d good=%d", psu_index + 1, *good);
exit:
    return ret;
}


/****************sub slot ***************************/

int bsp_cpld_get_slot_absent(OUT u8 *absent, int slot_index)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();

    if (slot_index == MAIN_BOARD_SLOT_INDEX)
    {
        *absent = 0;
        goto exit;
    }

    if (slot_index >= bd->slot_num)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "slot_index %d >= slot_num %d", slot_index, (int)bd->slot_num);
    }
    ret = bsp_cpld_get_bit(bd->cpld_addr_slot_absent[slot_index], bd->cpld_offs_slot_absent[slot_index], absent);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get slot absent bit failed!");

    DBG_ECHO(DEBUG_DBG, "slot %d absent=%d", slot_index + 1, *absent);

exit:
    return ret;
}

//power_on = TRUE, power on; power_on = FALSE, power off
int bsp_cpld_slot_power_enable(int slot_index, int power_on)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();

    if (slot_index >= bd->slot_num)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "slot_index %d >= slot_num %d", slot_index, (int)bd->slot_num);
    }
    ret = bsp_cpld_set_bit(bd->cpld_addr_slot_power_en[slot_index], bd->cpld_offs_slot_power_en[slot_index], power_on);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "set slot power_on bit failed!");

exit:
    return ret;
        
}

int bsp_cpld_get_card_power_ok(OUT u8 *power_ok, int slot_index)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();

    if (slot_index == MAIN_BOARD_SLOT_INDEX)
    {
        *power_ok = 1;
        goto exit;
    }

    if (slot_index >= bd->slot_num)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "slot_index %d >= slot_num %d", slot_index, (int)bd->slot_num);
    }
    ret = bsp_cpld_get_bit(bd->cpld_addr_card_power_ok[slot_index], bd->cpld_offs_card_power_ok[slot_index], power_ok);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get slot power_ok bit failed!");

    DBG_ECHO(DEBUG_DBG, "slot %d power_ok=%d", slot_index + 1, *power_ok);

exit:
    return ret;
}



//enable 255buffer, enable=TRUE, open; enable=FALSE, closed
int bsp_cpld_slot_buffer_enable(int slot_index, int enable)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();
    u8 enable1 = 0;
    u8 enable2 = 0;

    if (slot_index >= bd->slot_num)
    {
        CHECK_IF_ERROR_GOTO_EXIT(ret=ERROR_FAILED, "slot_index %d >= slot_num %d", slot_index, (int)bd->slot_num);
    }
    enable1 = (enable == TRUE) ? 0 : 0x3;
    enable2 = (enable == TRUE) ? 0 : 0x1;
    
    ret = bsp_cpld_write_part(enable1, bd->cpld_addr_slot_buff_oe1[slot_index], bd->cpld_mask_slot_buff_oe1[slot_index], bd->cpld_offs_slot_buff_oe1[slot_index]);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "set slot buffer1 enable bit failed!");
    
    ret = bsp_cpld_set_bit(bd->cpld_addr_slot_buff_oe2[slot_index], bd->cpld_offs_slot_buff_oe2[slot_index], enable2);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "set slot buffer2 enable bit failed!");

exit:
    return ret;
}


int bsp_cpld_miim_enable(int enable)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();
    ret = bsp_cpld_set_bit(bd->cpld_addr_miim_enable, bd->cpld_offs_miim_enable, enable);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "miim enable failed!");
exit:
    return ret;
}

//子卡复位，写1解复位，0复位
int bsp_cpld_set_slot_reset(int slot_index, int reset)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data();
    if (bd->cpld_addr_slot_reset[slot_index] != 0)
    {
        int ret = bsp_cpld_set_bit(bd->cpld_addr_slot_reset[slot_index], bd->cpld_offs_slot_reset[slot_index], reset);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "slot index %d set reset %d failed", slot_index, reset);
    }
exit:
    return ret;
}


void bsp_cpld_reset_max6696(int max_6696_index)
{
    board_static_data *bd = bsp_get_board_data();

    if ((max_6696_index >=0) && (max_6696_index < bd->max6696_num))
    {
        if (bd->cpld_addr_max6696_rst[max_6696_index] != 0)
        {
            bsp_cpld_set_bit(bd->cpld_addr_max6696_rst[max_6696_index], bd->cpld_offs_max6696_rst[max_6696_index], 1);
            mdelay(100);
            bsp_cpld_set_bit(bd->cpld_addr_max6696_rst[max_6696_index], bd->cpld_offs_max6696_rst[max_6696_index], 0);
            mdelay(100);
            
        }
        else
        {
            DBG_ECHO(DEBUG_INFO, "max6696 %d reset reg not defined!", max_6696_index);
        }
    } 
    else 
    {
        DBG_ECHO(DEBUG_ERR, "max6696 index %d error!", max_6696_index);
    }
    return;
}

int bsp_enable_slot_all_9548(int slot_index)
{
    int ret = ERROR_SUCCESS;
    int i = 0;
    board_static_data * bd = bsp_get_slot_data(slot_index);
    for (i = 0; i < MAX_PCA9548_NUM; i++)
    {
        if (bd->cpld_addr_9548_rst[i] == 0)
        {
            continue;
        }
        if (slot_index == MAIN_BOARD_SLOT_INDEX)
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_set_bit(bd->cpld_addr_9548_rst[i], bd->cpld_offs_9548_rst[i], 0), "reset 9548[%d] failed!", i); 
        }
        else
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_9548_rst[i], bd->cpld_offs_9548_rst[i], 0),  "reset slot index %d 9548[%d] failed!", slot_index, i);   
        }
    }

    udelay(1000);

    for (i = 0; i < MAX_PCA9548_NUM; i++)
    {
        if (bd->cpld_addr_9548_rst[i] == 0)
        {
            continue;
        }
        if (slot_index == MAIN_BOARD_SLOT_INDEX)
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_set_bit(bd->cpld_addr_9548_rst[i], bd->cpld_offs_9548_rst[i], 1), "reset 9548[%d] failed!", i); 
        }
        else
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_9548_rst[i], bd->cpld_offs_9548_rst[i], 1),  "reset slot index %d 9548[%d] failed!", slot_index, i); 
        }
    }
    
    udelay(1000);
exit:

    return ret;
}

int bsp_enable_slot_all_9545(int slot_index)
{
    int ret = ERROR_SUCCESS;
    int i = 0;
    board_static_data * bd = bsp_get_slot_data(slot_index);
    for (i = 0; i < MAX_PCA9545_NUM; i++)
    {
        if (bd->cpld_addr_9545_rst[i] == 0)
        {
            continue;
        }
        if (slot_index == MAIN_BOARD_SLOT_INDEX)
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_set_bit(bd->cpld_addr_9545_rst[i], bd->cpld_offs_9545_rst[i], 0), "reset 9545[%d] failed!", i); 
        }
        else
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_9545_rst[i], bd->cpld_offs_9545_rst[i], 0),  "reset slot index %d 9545[%d] failed!", slot_index, i);     
        }
    }
    
    udelay(1000);

    for (i = 0; i < MAX_PCA9545_NUM; i++)
    {
        if (bd->cpld_addr_9545_rst[i] == 0)
        {
            continue;
        }
        if (slot_index == MAIN_BOARD_SLOT_INDEX)
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_set_bit(bd->cpld_addr_9545_rst[i], bd->cpld_offs_9545_rst[i], 1), "reset 9545[%d] failed!", i); 
        }
        else
        {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_9545_rst[i], bd->cpld_offs_9545_rst[i], 1),  "reset slot index %d 9545[%d] failed!", slot_index, i);       
        }
    }
    
    udelay(1000);
    
exit:

    return ret;
}


/*************************************
end for cpld device method
**************************************/



/*初始化i2c总线*/
int i2c_init(void)
{
    u8 i = 0;
    int ret = -ENODEV;
    int use_smbus_index = bsp_get_board_data()->smbus_use_index;

    memset(&i2c_diag_info, 0, sizeof(i2c_diag_info));
    

    DBG_ECHO(DEBUG_INFO, "I2C SMBus init started...")

    for (i = 0; i < MAX_SMBUS_NUM; i++)
    {
        smbus_probe[i] = i2c_get_adapter(i);
        if (NULL != smbus_probe[i])
        {
            DBG_ECHO(DEBUG_INFO, "I2C adapter[%d] %s (ptr=0x%lx)", i, smbus_probe[i]->name, (unsigned long)smbus_probe[i]);
        }
    }

    if (NULL != smbus_probe[use_smbus_index])
    {
        smbus = smbus_probe[use_smbus_index];
        DBG_ECHO(DEBUG_INFO, "use smbus %d = %s", use_smbus_index ,smbus->name);
        ret = ERROR_SUCCESS;
    } 
    else 
    {
        DBG_ECHO(DEBUG_INFO, "I2C SMBus[%d] is NULL", use_smbus_index);
        ret = ERROR_FAILED;
    }
    smbus->retries = 0;
    mutex_init(&bsp_i2c_path_lock);

    DBG_ECHO(DEBUG_INFO, "I2C SMBus init end. ret = %d", ret)
    return ret;
}


int i2c_deinit(void)
{
    i2c_put_adapter(smbus);
    return ERROR_SUCCESS;
}

//i2c选通表
ssize_t bsp_sysfs_print_i2c_select_table(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    return bsp_print_i2c_select_table(buf);
}



//打印in_buf的内容，整理后的字符中存在out_buf中
size_t bsp_print_memory(u8 * in_buf, ssize_t in_buf_len, s8 * out_string_buf, ssize_t out_string_buf_len, unsigned long start_address,unsigned char addr_print_len)
{
#define BYTES_PER_LINE      16
#define MAX_OUT_BUF_LEN     PAGE_SIZE      //最大一页4096
#define TEMP_BUF_LEN        MAX_OUT_BUF_LEN * 10   //临时存字符串的缓冲区最大用10倍，防止字符太多溢出
#define LOW_MASK            (BYTES_PER_LINE-1)
#define MAX_LEN_PERLINE     256

    s32 i, j, len=0, len_temp=0;
    unsigned long temp_start_address = 0;
    unsigned long blank_count = 0;
    unsigned long blank_count_tail = 0;
    u8 temp_char = '\0';
    u8 * temp_buffer = NULL;
    u8 address_format[50] = {0};
    u8 temp2[MAX_LEN_PERLINE] = {0};
                    
 
    if (out_string_buf_len > MAX_OUT_BUF_LEN)
    {
        DBG_ECHO(DEBUG_ERR, "out_string_buf_len(%d) larger than MAX_OUT_BUF_LEN(%ld)", (int)out_string_buf_len, MAX_OUT_BUF_LEN);
        return 0;
    }
    if (NULL == (temp_buffer = (s8 *)kmalloc( TEMP_BUF_LEN, GFP_KERNEL)))
    {
        DBG_ECHO(DEBUG_ERR, "kmalloc failed for temp_buffer");
        return 0;
    }
    memset(temp_buffer, 0, TEMP_BUF_LEN);
    
    temp_start_address = start_address & (~LOW_MASK);
    blank_count = start_address & LOW_MASK;
    blank_count_tail = LOW_MASK - ((start_address + in_buf_len) & LOW_MASK);
    //一行全是空格，不用再显示这行
    blank_count_tail = blank_count_tail == LOW_MASK ? 0 : blank_count_tail;
    
    //地址显示格式
    sprintf(address_format, "%c%02dx: ", '%', addr_print_len);
    
    for (i = 0; i < in_buf_len + blank_count + blank_count_tail; i += BYTES_PER_LINE)
    {
            len += sprintf(temp_buffer + len, address_format, i + temp_start_address);
            len_temp = 0;
            for (j = 0; j < BYTES_PER_LINE; j++)
            {
                if ((i + j) < blank_count || i + j >= in_buf_len + blank_count)
                {
                    temp_char = ' ';

                    len += sprintf(temp_buffer + len, "   ");
            }
            else
            {
                    temp_char = in_buf[i + j - blank_count];
                    len += sprintf(temp_buffer + len, "%02x ", temp_char);
                }
                if (j == BYTES_PER_LINE / 2 - 1)
                {
                    len += sprintf(temp_buffer + len, " ");
                }
                len_temp += sprintf(temp2 + len_temp, "%c", (temp_char >= ' ' && temp_char <= '~') ? temp_char : '.' );

                if (len_temp >= MAX_LEN_PERLINE)
                {
                    DBG_ECHO(DEBUG_INFO, "current line string %d reaches max %d, break line.", len_temp, MAX_LEN_PERLINE);
                    break;
                }
            }

            temp2[len_temp] = '\0';
            len += sprintf(temp_buffer + len, " * %s *\n", temp2);
    }
    
    len_temp = len >= (out_string_buf_len - 1) ? out_string_buf_len - 1 : len;
    memcpy(out_string_buf, temp_buffer, len_temp);
    out_string_buf[len_temp] = '\0';
                                                                   
    kfree(temp_buffer);
                                                                                          
    return len_temp + 1;


#undef BYTES_PER_LINE  
#undef MAX_OUT_BUF_LEN 
#undef TEMP_BUF_LEN
#undef LOW_MASK       
#undef MAX_LEN_PERLINE 

}


ssize_t bsp_sysfs_debug_dump_i2c_mem(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    u8 temp_buffer[1024] = {0};
    u16 tempu16 = 0;
    ssize_t len = 0;
    int i = 0;
    int ret = ERROR_SUCCESS;
    len += sprintf(buf, "Read dev id 0x%x address 0x%x from 0x%x length 0x%x\n", i2c_debug_info_read.dev_path_id, i2c_debug_info_read.i2c_addr, i2c_debug_info_read.inner_addr, i2c_debug_info_read.read_len);

    ret = lock_i2c_path(i2c_debug_info_read.dev_path_id);
    if (ret == ERROR_SUCCESS)
    {
        switch(i2c_debug_info_read.rw_mode)
        {
        case 0x1:
            {
                ret = bsp_i2c_common_eeprom_read_bytes(i2c_debug_info_read.i2c_addr, i2c_debug_info_read.inner_addr, i2c_debug_info_read.read_len, temp_buffer);
                if (ret == ERROR_SUCCESS)
                {
                    len += bsp_print_memory(temp_buffer, i2c_debug_info_read.read_len, buf + len, 4096 - len, i2c_debug_info_read.inner_addr, 4);
                }
                break;
            }
            case 0x2:
            {
                ret = bsp_i2c_24LC128_eeprom_read_bytes(i2c_debug_info_read.i2c_addr, i2c_debug_info_read.inner_addr, i2c_debug_info_read.read_len, temp_buffer);
                if (ret == ERROR_SUCCESS)
                {
                    len += bsp_print_memory(temp_buffer, i2c_debug_info_read.read_len, buf + len, 4096 - len, i2c_debug_info_read.inner_addr, 4);
                }
                break;
            }
            case 0x3:
            {
                for (i = 0; i < i2c_debug_info_read.read_len; i++)
                {
                    ret = bsp_i2c_isl68127_read_reg(i2c_debug_info_read.i2c_addr, i2c_debug_info_read.inner_addr + i, &tempu16, 2);
                    if (ret == ERROR_SUCCESS)
                    {
                        len += sprintf(buf + len, "0x%02x: 0x%04x\n", i2c_debug_info_read.inner_addr + i, tempu16);
                    }
                    else
                    {
                        len += sprintf(buf + len, "0x%02x: Failed!\n", i2c_debug_info_read.inner_addr + i);
                    }
                }
                break;
            }
        }
    }
    unlock_i2c_path();
    
    if (ret != ERROR_SUCCESS)
    {
        len += sprintf(buf + len, "\nFailed!\n"); 
    }
    return len;
}

ssize_t bsp_sysfs_debug_i2c_do_write(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    int ret = ERROR_SUCCESS;

    if (i2c_debug_info_write.is_valid != 1)
    {
        len = sprintf(buf, "param is not set for writing, nothing to do.\n"); 
        goto exit;
    }

    len += sprintf(buf, "write dev id 0x%x address 0x%x inner 0x%x value 0x%x\n", i2c_debug_info_write.dev_path_id, i2c_debug_info_write.i2c_addr, i2c_debug_info_write.inner_addr, i2c_debug_info_write.write_value);
    

    ret = lock_i2c_path(i2c_debug_info_write.dev_path_id);
    if (ret == ERROR_SUCCESS)
    {
        //借用相似函数
        switch(i2c_debug_info_write.rw_mode)
        {
        case 0x1:
            {
                ret = bsp_i2c_common_eeprom_write_byte(i2c_debug_info_write.i2c_addr, i2c_debug_info_write.inner_addr, (u8)i2c_debug_info_write.write_value);
                break;
            }
            case 0x2:
            {
                ret = bsp_i2c_24LC128_eeprom_write_byte(i2c_debug_info_write.i2c_addr, i2c_debug_info_write.inner_addr, i2c_debug_info_write.write_value);
                break;
            }
            case 0x3:
            {
                ret = bsp_i2c_isl68127_write_reg(i2c_debug_info_write.i2c_addr, i2c_debug_info_write.inner_addr, i2c_debug_info_write.write_value, 2);
                break;
            }
        }
    }
    unlock_i2c_path();

    len += sprintf(buf + len, "%s", ret == ERROR_SUCCESS ? "success!\n" : "failed!\n"); 

exit:
    return len;
}

ssize_t bsp_sysfs_i2c_debug_op_param_get(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    int i = 0;
    board_static_data *bd = bsp_get_board_data();

    len += sprintf(buf + len, "\nExample :\n");
    len += sprintf(buf + len, "    Turn i2c path to path_id 0x1, I2C device address is 0x50, read 0x10 bytes starts from inner address 0x0\n");
    len += sprintf(buf + len, "        echo 'path 0x1 addr 0x50 read from 0x0 len 0x10 mode 0x1' > param \n");
    len += sprintf(buf + len, "    Turn i2c path to path_id 0x1, I2C device address is 0x50, write 0x1 to inner address 0x0\n");
    len += sprintf(buf + len, "        echo 'path 0x1 addr 0x50 write inner 0x0 value 0x1 mode 0x1' > param \n");
    len += sprintf(buf + len, "    *all integer must be hex. \n");
    len += sprintf(buf + len, "    mode 0x1: inner address is  8 bit, data width  8 bit\n");
    len += sprintf(buf + len, "    mode 0x2: inner address is 16 bit, data width  8 bit\n");
    len += sprintf(buf + len, "    mode 0x3: inner address is  8 bit, data width 16 bit\n");
    
    len += sprintf(buf + len, "\nCurrent read settings:\n");
    len += sprintf(buf + len, "    Path_ID    : %d\n",   i2c_debug_info_read.dev_path_id);
    len += sprintf(buf + len, "    DevI2CAddr : 0x%x\n", i2c_debug_info_read.i2c_addr);
    len += sprintf(buf + len, "    InnerAddr  : 0x%x\n", i2c_debug_info_read.inner_addr);
    len += sprintf(buf + len, "    ReadLength : 0x%x\n", i2c_debug_info_read.read_len);
    len += sprintf(buf + len, "    ReadMode   : 0x%x\n", i2c_debug_info_read.rw_mode);

    len += sprintf(buf + len, "\nCurrent write settings:\n");
    len += sprintf(buf + len, "    Path_ID    : %d\n",   i2c_debug_info_write.dev_path_id);
    len += sprintf(buf + len, "    DevI2CAddr : 0x%x\n", i2c_debug_info_write.i2c_addr);
    len += sprintf(buf + len, "    InnerAddr  : 0x%x\n", i2c_debug_info_write.inner_addr);
    len += sprintf(buf + len, "    WriteValue : 0x%x\n", i2c_debug_info_write.write_value);
    len += sprintf(buf + len, "    WriteMode  : 0x%x\n", i2c_debug_info_write.rw_mode);
    
    len += sprintf(buf + len, "\nI2C Path_ID defination:\n");
    len += sprintf(buf + len, "    %-25s :%4d~%d (%2d/slot)\n", __stringify(I2C_DEV_OPTIC_IDX_START), I2C_DEV_OPTIC_IDX_START, I2C_DEV_OPTIC_BUTT - 1, MAX_OPTIC_PER_SLOT);    
    len += sprintf(buf + len, "    %-25s :%4d~%d (%2d/slot)\n", __stringify(I2C_DEV_EEPROM), I2C_DEV_EEPROM, I2C_DEV_EEPROM_BUTT - 1, MAX_EEPROM_PER_SLOT);
    len += sprintf(buf + len, "    %-25s :%4d~%d (%2d/slot)\n", __stringify(I2C_DEV_LM75), I2C_DEV_LM75, I2C_DEV_LM75_BUTT - 1, MAX_LM75_NUM_PER_SLOT);
    len += sprintf(buf + len, "    %-25s :%4d~%d (%2d/slot)\n", __stringify(I2C_DEV_MAX6696), I2C_DEV_MAX6696, I2C_DEV_MAX6696_BUTT - 1, MAX_MAX6696_NUM_PER_SLOT);
    len += sprintf(buf + len, "    %-25s :%4d~%d\n",  __stringify(I2C_DEV_PSU), I2C_DEV_PSU, I2C_DEV_PSU_BUTT - 1);
    len += sprintf(buf + len, "    %-25s :%4d~%d\n", __stringify(I2C_DEV_INA219), I2C_DEV_INA219, I2C_DEV_INA219_BUTT - 1);
    len += sprintf(buf + len, "    %-25s :%4d~%d\n", __stringify(I2C_DEV_I350), I2C_DEV_I350, I2C_DEV_I350);
    len += sprintf(buf + len, "    %-25s :%4d~%d\n", __stringify(I2C_DEV_FAN), I2C_DEV_FAN, I2C_DEV_FAN_BUTT - 1);
    len += sprintf(buf + len, "    %-25s :%4d~%d\n", __stringify(I2C_DEV_ISL68127), I2C_DEV_ISL68127, I2C_DEV_ISL68127_BUTT - 1);
    len += sprintf(buf + len, "    %-25s :%4d~%d\n", __stringify(I2C_DEV_ADM1166), I2C_DEV_ADM1166, I2C_DEV_ADM1166_BUTT - 1);


    len += sprintf(buf + len, "\nI2C Device address:\n");
        len += sprintf(buf + len, "    EEPROM     : 0x%x\n", bd->i2c_addr_eeprom);
    for (i = 0; i < bd->lm75_num; i++)
        len += sprintf(buf + len, "    LM75(%d)    : 0x%x\n", i + 1, bd->i2c_addr_lm75[i]); 
    for (i = 0; i < bd->max6696_num; i++)
        len += sprintf(buf + len, "    MAX6696(%d) : 0x%x\n", i + 1, bd->i2c_addr_max6696[i]); 
    for (i = 0; i < bd->psu_num; i++)
    {
        len += sprintf(buf + len, "    PSU(%d)     : 0x%x\n", i + 1, bd->i2c_addr_psu[i]); 
        len += sprintf(buf + len, "    PMBus(%d)   : 0x%x\n", i + 1, bd->i2c_addr_psu_pmbus[i]);
        len += sprintf(buf + len, "    INA219(%d)  : 0x%x\n", i + 1, bd->i2c_addr_ina219[i]); 
    }
    for (i = 0; i < bd->fan_num; i++)
        len += sprintf(buf + len, "    FAN(%d)     : 0x%x\n", i + 1, bd->i2c_addr_fan[i]); 
    for (i = 0; i < bd->isl68127_num; i++)
        len += sprintf(buf + len, "    ISL68127(%d): 0x%x\n", i + 1, bd->i2c_addr_isl68127[i]); 
    for (i = 0; i < bd->adm1166_num; i++)
        len += sprintf(buf + len, "    ADM1166(%d) : 0x%x\n", i + 1, bd->i2c_addr_adm1166[i]);     
    
    return len;
}

ssize_t bsp_sysfs_i2c_debug_op_param_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int temp_dev_id = 0;
    //int temp_is_write = 0;
    int temp_i2c_addr = 0;
    int temp_inner_addr = 0;
    int temp_read_len = 0;
    int temp_write_value = 0;
    int temp_rw_mode = 0;

    if (sscanf (buf, "path 0x%x addr 0x%x read from 0x%x len 0x%x mode 0x%x", &temp_dev_id, &temp_i2c_addr, &temp_inner_addr, &temp_read_len, &temp_rw_mode) == 5)
    {
        i2c_debug_info_read.dev_path_id = temp_dev_id;
        i2c_debug_info_read.is_write    = 0;
        i2c_debug_info_read.i2c_addr    = temp_i2c_addr;
        i2c_debug_info_read.inner_addr  = temp_inner_addr;
        i2c_debug_info_read.read_len    = temp_read_len;
        i2c_debug_info_read.rw_mode     = temp_rw_mode;
        i2c_debug_info_read.is_valid = 1;
    }
    else if (sscanf (buf, "path 0x%x addr 0x%x write inner 0x%x value 0x%x mode 0x%x", &temp_dev_id, &temp_i2c_addr, &temp_inner_addr, &temp_write_value, &temp_rw_mode) == 5)
    {
        i2c_debug_info_write.dev_path_id = temp_dev_id;
        i2c_debug_info_write.is_write    = 1;
        i2c_debug_info_write.i2c_addr    = temp_i2c_addr;
        i2c_debug_info_write.inner_addr  = temp_inner_addr;
        i2c_debug_info_write.write_value = temp_write_value;
        i2c_debug_info_write.rw_mode     = temp_rw_mode;
        i2c_debug_info_write.is_valid = 1;
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "given: '%s'", buf);
        DBG_ECHO(DEBUG_ERR, "format: 'path 0x1 addr 0x50 read from 0x0 len 0x10 mode 0x1', all number must be hex");
        DBG_ECHO(DEBUG_ERR, "format: 'path 0x1 addr 0x50 write inner 0x0 value 0x1 mode 0x1', all number must be hex");
    }

    return count;
}


ssize_t bsp_sysfs_debug_level_get(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    len += sprintf(buf + len, "Debug information level current setting is 0x%x:\n", bsp_debug_level);
    len += sprintf(buf + len, "    INFO (1): %s\n", bsp_debug_level & 0x1 ? "on" : "off");
    len += sprintf(buf + len, "    ERR  (2): %s\n", bsp_debug_level & 0x2 ? "on" : "off");
    len += sprintf(buf + len, "    DEBUG(4): %s\n", bsp_debug_level & 0x4 ? "on" : "off");
    len += sprintf(buf + len, "example: echo 7 > debug_level turn on all.\n");
    
    return len;
}

ssize_t bsp_sysfs_i2c_debug_level_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int debug_level = 0;
    if ((sscanf(buf, "%d", &debug_level) == 1) || (sscanf(buf, "0x%x", &debug_level) == 1))
    {
        bsp_debug_level = debug_level & 0x7;
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "format error for %s", buf);
    }
    return count;
}

ssize_t bsp_sysfs_debug_i2c_diag(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    int i = 0;
    struct rtc_time tm = {0};
    
    u64 timezone_sec_diff = sys_tz.tz_minuteswest * 60;
        
    len += sprintf(buf + len, "i2c diag record count %d/%d, current index %d\n", i2c_diag_info.rec_count, MAX_I2C_DIAG_RECORD_COUNT, i2c_diag_info.curr_index);
    len += sprintf(buf + len, "use 'dmesg' to check detail\n");    

    printk(KERN_DEBUG"rv    : return value\n");
    printk(KERN_DEBUG"R/W   : Read or Write\n");
    printk(KERN_DEBUG"retry : retry times\n");
    printk(KERN_DEBUG"path  : I2C path id\n");
    printk(KERN_DEBUG"addr  : I2C device address\n");
    printk(KERN_DEBUG"pro   : protocol\n");
    printk(KERN_DEBUG"iaddr : I2C device inner address/command code\n");
    printk(KERN_DEBUG"time  : UTC time, need to diff timezone to convert to local time\n");
    printk(KERN_DEBUG"index  rv R/W retry path addr pro iaddr  time\n");
 
              //        [0000] -6  R  3     3    0x50 0x8 0x0000 2016/11/19 13:26:36
    for (i = 0; i < i2c_diag_info.rec_count; i++)
    {
        if (i2c_diag_info.is_valid)
        {
            tm = rtc_ktime_to_tm(i2c_diag_info.record[i].time_sec - timezone_sec_diff);
            
            printk(KERN_DEBUG"[%04d]%3d  %s  %d   %3d    0x%2x 0x%x 0x%04x %04d/%02d/%02d %02d:%02d:%02d\n", 
                i, 
                i2c_diag_info.record[i].error_code,
                i2c_diag_info.record[i].read_write == I2C_SMBUS_READ ? "R" : (i2c_diag_info.record[i].read_write == I2C_SMBUS_WRITE ? "W" : "?") ,
                i2c_diag_info.record[i].retry_times,
                i2c_diag_info.record[i].path_id,
                i2c_diag_info.record[i].i2c_addr,
                i2c_diag_info.record[i].protocol,
                i2c_diag_info.record[i].inner_addr,
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour, 
                tm.tm_min, 
                tm.tm_sec
                );
        }
    }
    
    return len;
}


ssize_t bsp_sysfs_debug_log_get(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    int i = 0;
    len += sprintf(buf + len, "recent log config size %d\n", BSP_LOG_FILETER_RECENT_LOG_COUNT);
    len += sprintf(buf + len, "current log index %d\n", bsp_recent_log.curr_record_index);

    len += sprintf(buf + len, "* 'echo 0 > recent_log' to clear the records\n");

    mutex_lock(&bsp_recent_log_lock);
    
    for (i = 0; i < BSP_LOG_FILETER_RECENT_LOG_COUNT; i++)
    {
        len += sprintf(buf + len, "[%02d] used=%d hit=%06d line=%05d file=%s\n", 
            i, 
            bsp_recent_log.used[i],
            bsp_recent_log.hit_count[i],
            bsp_recent_log.line_no[i],
            (bsp_recent_log.filename[i][0] == 0) ? "none" : bsp_recent_log.filename[i]
        );
    }


    mutex_unlock(&bsp_recent_log_lock);
    
    return len;
}

ssize_t bsp_sysfs_i2c_debug_log_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    mutex_lock(&bsp_recent_log_lock);
    
    memset(&bsp_recent_log, 0, sizeof(bsp_recent_log));

    mutex_unlock(&bsp_recent_log_lock);
    return count;
}


ssize_t bsp_sysfs_debug_private_log_get(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    
    len += sprintf(buf + len, "private log switch is %s\n", log_to_private_file == TRUE ? "ON" : "OFF");
    len += sprintf(buf + len, "current logfile is %s\n", curr_h3c_log_file);
    len += sprintf(buf + len, "echo 0 > private_log is turn off switch\n");
    len += sprintf(buf + len, "echo 1 > private_log is turn on  switch\n");

    return len;

}

ssize_t bsp_sysfs_i2c_debug_private_log_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    
    int on_off = 0;
    if (sscanf(buf, "%d", &on_off) == 1)
    {
        if (on_off == 1)
        {
            bsp_h3c_open_init_log();
        }
        else
        {
            bsp_h3c_close_init_log();
        }
          
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "format error for %s", buf);
    }
   
    return count;
}


ssize_t bsp_sysfs_debug_dmesg_filter_get(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    
    len += sprintf(buf + len, "dmesg filter is %s\n", log_filter_to_dmesg == TRUE ? "ON" : "OFF");
    
    len += sprintf(buf + len, "echo 0 > dmesg_filter is turn off filter\n");
    len += sprintf(buf + len, "echo 1 > dmesg_filter is turn on  filter\n");

    return len;

}

ssize_t bsp_sysfs_debug_dmesg_filter_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int on_off = 0;
    if (sscanf(buf, "%d", &on_off) == 1)
    {
        if (on_off == 1)
        {
            log_filter_to_dmesg = TRUE;
        }
        else
        {
            log_filter_to_dmesg = FALSE;
        }
          
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "format error for %s", buf);
    }
   
    return count;
}


ssize_t bsp_sysfs_dmesg_loglevel_get(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    
    len += sprintf(buf + len, "dmesg loglevel is 0x%x\n", bsp_dmesg_log_level);
    len += sprintf(buf + len, "DEBUG(0x%x) INFO(0x%x) ERROR(0x%x)\n", DEBUG_DBG, DEBUG_INFO, DEBUG_ERR);
    
    len += sprintf(buf + len, "echo 0x%x > dmesg_loglevel is set level to ERROR\n", DEBUG_ERR);
    len += sprintf(buf + len, "echo 0x%x > dmesg_loglevel is set level to ERROR and INFO\n", DEBUG_ERR | DEBUG_INFO);

    return len;

}

ssize_t bsp_sysfs_dmesg_loglevel_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int debug_level = 0;
    if (sscanf(buf, "0x%x", &debug_level) == 1)
    {
        bsp_dmesg_log_level = debug_level;
          
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "format error for %s", buf);
    }
   
    return count;
}


ssize_t bsp_sysfs_reset_smbus_slave_get(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    
    len += sprintf(buf + len, "reset smbus i2c slave\n");
    len += sprintf(buf + len, "usage:\n");  
    len += sprintf(buf + len, "    echo 'path_id' > reset_slave\n");  
    len += sprintf(buf + len, "    path_id is the i2c path id for the i2c slave where can be seen from i2c_read.py \n");
    len += sprintf(buf + len, "example: \n");
    len += sprintf(buf + len, "    echo %d > reset_slave (reset)\n", I2C_DEV_PSU);
    return len;

}


void bsp_send_i2c_reset_signal(void)
{
    int i = 0;
    u8 temp_value = 0;
    board_static_data *bd = bsp_get_board_data();
    for (i = 0; i < 9; i++)
    {
        //write high
        bsp_cpu_cpld_read_byte(&temp_value, bd->cpld_addr_gpio_i2c_0);
        temp_value = (1 << bd->cpld_offs_gpio_i2c_0) | temp_value;
        bsp_cpu_cpld_write_byte(temp_value, bd->cpld_addr_gpio_i2c_0);
        udelay(5);
        //write low
        bsp_cpu_cpld_read_byte(&temp_value, bd->cpld_addr_gpio_i2c_0);
        temp_value = (~(1 << bd->cpld_offs_gpio_i2c_0)) & temp_value;
        bsp_cpu_cpld_write_byte(temp_value, bd->cpld_addr_gpio_i2c_0);
        udelay(5);
    }
    udelay(1000);
}


//用cpld模拟9个时钟脉冲
int bsp_reset_smbus_slave(int i2c_device_id)
{
    
    int ret = ERROR_SUCCESS;
    //bsp_cpld_set_bit(bd->cpld_addr_cpld_buf_enable, bd->cpld_offs_cpld_buf_enable, 1);
    //udelay(1000);
    //if (lock_i2c_path(i2c_device_id) == ERROR_SUCCESS)
    //如果中间9545/9548访问出现异常，调路径不成功也需要发送复位信号
   
    lock_i2c_path(i2c_device_id);
    {
        bsp_send_i2c_reset_signal();
        DBG_ECHO(DEBUG_INFO, "i2c device %d reset done", i2c_device_id);
    }
    unlock_i2c_path();

    return ret;
}

ssize_t bsp_sysfs_reset_smbus_slave_set(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int i2c_device_id = 0;
    if (sscanf(buf, "%d", &i2c_device_id) == 1)
    {
        if (i2c_device_id < I2C_DEV_OPTIC_IDX_START || i2c_device_id >= I2C_DEV_BUTT)
        {
            DBG_ECHO(DEBUG_INFO, "param error device is [%d, %d)", I2C_DEV_OPTIC_IDX_START, I2C_DEV_BUTT);
            count = -EINVAL;
        }
        else
        {
            if (bsp_reset_smbus_slave(i2c_device_id) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "reset smbus slave %d failed", i2c_device_id);
            }
        }
    }
    else
    {
        DBG_ECHO(DEBUG_INFO, "format error for %s", buf);
        count = -EINVAL;
    }
   
    return count;
}


void bsp_h3c_open_init_log(void)
{
    log_to_private_file = TRUE;      
}

void bsp_h3c_close_init_log(void)
{
    log_to_private_file = FALSE; 
}

void bsp_h3c_dmesg_filter_on(void)
{
    log_filter_to_dmesg = TRUE; 
}

void bsp_h3c_dmesg_filter_off(void)
{
    log_filter_to_dmesg = FALSE; 
}


SYSFS_RO_ATTR_DEF(i2c_select_table, bsp_sysfs_print_i2c_select_table);
SYSFS_RO_ATTR_DEF(mem_dump, bsp_sysfs_debug_dump_i2c_mem);
SYSFS_RO_ATTR_DEF(do_write, bsp_sysfs_debug_i2c_do_write);
SYSFS_RO_ATTR_DEF(diag_info, bsp_sysfs_debug_i2c_diag);
SYSFS_RW_ATTR_DEF(param, bsp_sysfs_i2c_debug_op_param_get, bsp_sysfs_i2c_debug_op_param_set);
SYSFS_RW_ATTR_DEF(debug_level, bsp_sysfs_debug_level_get, bsp_sysfs_i2c_debug_level_set);
SYSFS_RW_ATTR_DEF(recent_log, bsp_sysfs_debug_log_get, bsp_sysfs_i2c_debug_log_set);
SYSFS_RW_ATTR_DEF(private_log, bsp_sysfs_debug_private_log_get, bsp_sysfs_i2c_debug_private_log_set);
SYSFS_RW_ATTR_DEF(dmesg_filter, bsp_sysfs_debug_dmesg_filter_get, bsp_sysfs_debug_dmesg_filter_set);
SYSFS_RW_ATTR_DEF(dmesg_loglevel, bsp_sysfs_dmesg_loglevel_get, bsp_sysfs_dmesg_loglevel_set);
SYSFS_RW_ATTR_DEF(reset_slave, bsp_sysfs_reset_smbus_slave_get, bsp_sysfs_reset_smbus_slave_set);


static int bsp_init_subcard(board_static_data * mainboard)
{
    int slot_index = 0;
    int ret = ERROR_SUCCESS;
    int slot_num = mainboard->slot_num;
    u8 absent = TRUE;
    int card_pdt_type = PDT_TYPE_BUTT;    
    //int slot_type = PDT_TYPE_BUTT;

    memset(&sub_board_data, 0, sizeof(sub_board_data));
    
    for (slot_index = 0; slot_index < slot_num; slot_index ++)
    {
        //子卡数据通过initialized=TRUE判断有效性，不通过NULL判断
        mainboard->sub_slot_info[slot_index] = &sub_board_data[slot_index];

        if (bsp_cpld_get_slot_absent(&absent, slot_index) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "get slot absent from cpld failed for slot index %d !", slot_index);      
            continue;
        }
        msleep(10);

        if (absent == TRUE)
        {//子卡不在位，直接过
            DBG_ECHO(DEBUG_INFO, "slot index %d is absent.", slot_index); 
            continue;
        }
        else
        {
            DBG_ECHO(DEBUG_INFO, "detect slot %d inserted!", slot_index); 
        }
        
        if (bsp_cpld_slot_power_enable(slot_index, TRUE) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "set slot index %d power on failed!", slot_index);
            continue;
        }
        msleep(10);

        //解复位子卡
        if ((ret = bsp_cpld_set_slot_reset(slot_index, 1)) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "slot %d reset failed", slot_index);  
            continue;
        }
        msleep(10);
        
        if (bsp_cpld_slot_buffer_enable(slot_index, TRUE) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "set slot index %d buffer open failed!", slot_index);
            continue;
        }
        msleep(300);
        if (bsp_get_card_product_type(&card_pdt_type, slot_index) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "get card product type failed for slot index %d!", slot_index);
            continue;
        }
        msleep(10);
        //主板寄存器
        if (bsp_cpld_miim_enable(TRUE) != ERROR_SUCCESS)
        {
            DBG_ECHO(DEBUG_ERR, "set miim enable failed!");
        }
        msleep(10);

        //slot is present, initialize the data
  
        switch(card_pdt_type)
        {
        case PDT_TYPE_TCS83_120F_32H_SUBCARD:
            {
                if ((ret = board_static_data_init_TCS83_120F_32H_subcard(slot_index, mainboard)) != ERROR_SUCCESS)
                {
                    DBG_ECHO(DEBUG_ERR, "initialize slot %d card type %d failed! ret=%d", slot_index, card_pdt_type, ret);
                    continue;
                }
                else
                {
                    DBG_ECHO(DEBUG_INFO, "slot %d card type %d static data initializd!", slot_index, card_pdt_type);
                }
                break;
            }
            default:
            {
                DBG_ECHO(DEBUG_ERR, "Not support slot %d card type %d !", slot_index, card_pdt_type);
                break;
            }
        }

    }
    return ERROR_SUCCESS;
}


int bsp_h3c_get_file_size(char * filename, int * file_size)
{
    struct file *pfile = NULL;
    pfile = filp_open(curr_h3c_log_file, O_RDONLY, 0644);
    if (IS_ERR (pfile))
    {
        //printk (KERN_ERR "BSP: bsp_h3c_get_file_size open file %s failed\n", filename);
        return -EIO;
    }
    *file_size = pfile->f_inode->i_size;
    fput(pfile);
    
    return ERROR_SUCCESS;
}

int bsp_h3c_localmsg_init (void)
{

    int ret0 = ERROR_SUCCESS;
    int ret1 = ERROR_SUCCESS;
    
    int file_size_0 = 0;
    int file_size_1 = 0;
    mutex_init(&bsp_logfile_lock);
    mutex_init(&bsp_recent_log_lock);
    
    memset(&bsp_recent_log, 0, sizeof(bsp_recent_log));
        

    ret0 = bsp_h3c_get_file_size(LOG_FILE_PATH_0, &file_size_0);
    ret1 = bsp_h3c_get_file_size(LOG_FILE_PATH_1, &file_size_1);

    if (file_size_0 < LOG_FILE_SIZE && ret0 == ERROR_SUCCESS)
    {
        curr_h3c_log_file = LOG_FILE_PATH_0;
    }
    else if (file_size_1 < LOG_FILE_SIZE && ret1 == ERROR_SUCCESS)
    {
        curr_h3c_log_file = LOG_FILE_PATH_1;
    }
    else
    {
        curr_h3c_log_file = LOG_FILE_PATH_0;
    }
    
    //printk (KERN_INFO "BSP: %s size=%d %s size=%d set logfile to %s\n", LOG_FILE_PATH_0, file_size_0, LOG_FILE_PATH_1, file_size_1, curr_h3c_log_file);

    return ERROR_SUCCESS;
    
}



int bsp_h3c_check_log_file(void)
{
    int file_size = 0;
    struct file *pfile = NULL;
    int ret = ERROR_SUCCESS;

    int temp_ret = bsp_h3c_get_file_size(curr_h3c_log_file, &file_size);
    
    if (file_size >= LOG_FILE_SIZE && temp_ret == ERROR_SUCCESS)
    {
        curr_h3c_log_file = (strcmp(curr_h3c_log_file, LOG_FILE_PATH_0) == 0) ? LOG_FILE_PATH_1 : LOG_FILE_PATH_0;
        pfile = filp_open(curr_h3c_log_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (IS_ERR (pfile))
        {
            printk (KERN_ERR "BSP: open logfile %s failed!\n", curr_h3c_log_file);
            ret = -EIO;
        }
        else
        {
            fput(pfile);
            ret = ERROR_SUCCESS;
        }
    }

    return ret;
}


int bsp_h3c_log_filter_to_dmesg(int log_level, const char * log_str, const char * src_file_name, unsigned int line_no)
{
    int i = 0;
    int found_hit = 0;
    int curr_index = 0;


    if ((log_level & bsp_dmesg_log_level) == 0)
        return ERROR_SUCCESS;


    if (log_str == NULL || src_file_name == NULL)
    {
        printk (KERN_ERR "BSP: param error, log_str=%p src_file=%p", log_str, src_file_name);
        return ERROR_FAILED;
    }

    mutex_lock(&bsp_recent_log_lock);
    
    for (i = 0; i < BSP_LOG_FILETER_RECENT_LOG_COUNT; i++)
    {
        if (bsp_recent_log.used[i] == 1 && bsp_recent_log.line_no[i] == line_no)
        {
            //There is the log recently, skip
            if (bsp_recent_log.filename[i] != NULL && strcmp(bsp_recent_log.filename[i], src_file_name) == 0)
            {
                bsp_recent_log.hit_count[i]++;
                found_hit = 1;
                break;
            }
        }
    }

    if (found_hit != 1)
    {   
        curr_index = bsp_recent_log.curr_record_index;
        bsp_recent_log.used[curr_index] = 1;
        bsp_recent_log.line_no[curr_index] = line_no;
        bsp_recent_log.hit_count[curr_index] = 1;
        strcpy(bsp_recent_log.filename[curr_index], src_file_name);
        bsp_recent_log.curr_record_index = (curr_index >= BSP_LOG_FILETER_RECENT_LOG_COUNT - 1) ? 0 : (curr_index + 1);
        printk(KERN_ERR"%s", log_str);
    }
    else if (log_filter_to_dmesg == FALSE)
    {
        printk(KERN_ERR"%s", log_str);
    }

    mutex_unlock(&bsp_recent_log_lock);

    return ERROR_SUCCESS;
}

//src_file and line_no is the key for log filter
int bsp_h3c_localmsg_to_file (char *buf, long len, int log_level, const char * src_file, unsigned int line_no)
{
	int ret = ERROR_SUCCESS;
	struct file *pfile = NULL;
    long len_s = 0;

	if (NULL == buf)
	{ 
		printk (KERN_ERR "function params buf = %p, path = %p is invalid.\n", buf, curr_h3c_log_file);
		ret = ERROR_FAILED;
        goto exit_no_lock;
	}
    
	mutex_lock(&bsp_logfile_lock);

    bsp_h3c_log_filter_to_dmesg(log_level, buf, src_file, line_no);

    if (log_to_private_file == TRUE)
    {
        if (bsp_h3c_check_log_file() < 0)
        {
            printk (KERN_ERR "BSP: bsp_h3c_check_log_file failed!");
        }
        pfile = filp_open (curr_h3c_log_file, O_RDWR | O_CREAT | O_APPEND , 0644);
        if (IS_ERR (pfile))
	    {
		    printk (KERN_ERR "file %s filp_open error.", curr_h3c_log_file);
		    ret = -EIO;
            goto exit;
	    }
        else
	    {
            char log_str[LOG_STRING_LEN + 64] = {0};
            struct rtc_time tm = {0};
            loff_t pos = 0;
            u64 timezone_sec_diff = sys_tz.tz_minuteswest * 60;
            tm = rtc_ktime_to_tm(get_seconds() - timezone_sec_diff);
            len_s = sprintf(log_str, "%04d-%02d-%02d %02d:%02d:%02d %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, buf);

            ret = kernel_write (pfile, log_str, len_s, &pos);
		    if (0 > ret)
		    {
		    	printk (KERN_ERR "Error writing local msg file: %s.\n", curr_h3c_log_file);
		    }
        
		    fput(pfile);
	    }
    }
    
exit:
    
    mutex_unlock(&bsp_logfile_lock);
exit_no_lock:
    
	return (ret < 0) ? -EIO : ERROR_SUCCESS;
}

int optoe_port_index_convert(int optoe_port_index)
{
	int port_num = 0;
	int switch_type;

	bsp_get_product_type(&switch_type);
	
	switch (switch_type)
	{
		case PDT_TYPE_TCS81_120F_1U:
			if(optoe_port_index > 104)/* 48x2 + 8 */
			{
				return -1;
			}
			if( 96 >= optoe_port_index )
			{	
				port_num = ( 0 == optoe_port_index%2 ) ? (optoe_port_index) : (optoe_port_index+1);
				return ( port_num/2 );
			}
			else
			{
				port_num = optoe_port_index - 96 + 48;
				return port_num;
			}
		    break;
		case PDT_TYPE_TCS83_120F_4U:
		case PDT_TYPE_TCS82_120F_1U:
		default:
			return optoe_port_index;
	}
}
/*
 * optic_index start from 0
 */
int optic_lock(int optic_index)
{
	int switch_type;

	bsp_get_product_type(&switch_type);	
	switch (switch_type)
	{
		/*devices with subcard and 32 ports per subcard*/
		case PDT_TYPE_TCS83_120F_4U:
			return lock_i2c_path(GET_I2C_DEV_OPTIC_IDX_START_SLOT(optic_index/32) + optic_index%32);
			break;
		/*devices without subcard*/
		case PDT_TYPE_TCS81_120F_1U:
		case PDT_TYPE_TCS82_120F_1U:
		default:
			return lock_i2c_path(GET_I2C_DEV_OPTIC_IDX_START_SLOT(-1) + optic_index);
	}
}
void optic_unlock(void)
{
	unlock_i2c_path();
}
static int __init bsp_init(void)
{
    int ret = ERROR_SUCCESS;
    int pdt_type = PDT_TYPE_BUTT;
    board_static_data *bd = bsp_get_board_data();

    bsp_h3c_localmsg_init();
    mutex_init(&bsp_fan_speed_lock);

    //必须先初始化核心数据
    CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_get_product_type(&pdt_type), "bsp get product type failed!");
    //pdt_type = PDT_TYPE_TCS81_120F_1U;
    
    switch(pdt_type)
    {
    case PDT_TYPE_TCS81_120F_1U:
        ret = board_static_data_init_TCS81_120F(bd);
        break;
    case PDT_TYPE_TCS83_120F_4U:
        ret = board_static_data_init_TCS83_120F(bd);
        break;
	case PDT_TYPE_TCS82_120F_1U:
		ret = board_static_data_init_TCS82_120F(bd);
        break;
    default:
        DBG_ECHO(DEBUG_ERR, "pdt_type=0x%x, not supported!\n", pdt_type);
        ret = ERROR_FAILED;
        goto exit;
    }
    CHECK_IF_ERROR_GOTO_EXIT(ret, "static data init failed!");    
    CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_init(), "cpld init failed!");  
    CHECK_IF_ERROR_GOTO_EXIT(ret=i2c_init(), "i2c init failed!");

    if (bsp_set_cpu_init_ok(1) != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "CPU init ok bit set failed!");
    }
    
    if (bsp_set_mac_rov() != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "MAC voltage set failed!");
    }

    kobj_switch = kobject_create_and_add("switch", kernel_kobj->parent);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_switch, "kobj_switch create falled!\n");

    kobj_debug = kobject_create_and_add("debug", kobj_switch);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_debug, "kobj_debug create falled!\n");

    kobj_debug_i2c = kobject_create_and_add("i2c", kobj_debug);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_debug_i2c, "create kobj_debug_i2c failed!");
        
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, i2c_select_table, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, mem_dump, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, param, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, do_write, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, debug_level, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, diag_info, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, recent_log, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, private_log, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, dmesg_filter, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, dmesg_loglevel, ret);
    CHECK_CREATE_SYSFS_FILE(kobj_debug_i2c, reset_slave, ret);

    if (bsp_get_board_data()->slot_num > 0)
    {
        //子卡初始化
        bsp_init_subcard(bd);
    }


exit:
    if (ret != ERROR_SUCCESS)
    {
        DBG_ECHO(DEBUG_ERR, "bsp init falled, ret=%d!\n", ret);
        ret = -ENOMEM;
    }
    else
    {
        INIT_PRINT("bsp init successed!\n");
    }
        
    return ret;
}


void release_all_kobjs(void)
{
    if (kobj_debug_i2c != NULL)
    {
        kobject_put(kobj_debug_i2c);
    }
    if (kobj_debug != NULL)
    {
        kobject_put(kobj_debug);
    }
    if (kobj_switch != NULL)
    {
        kobject_put(kobj_switch);
    }
    return;
}

static void __exit bsp_exit(void)
{
    i2c_deinit();
    bsp_cpld_deinit();

    release_all_kobjs();
    INIT_PRINT("module bsp uninstalled!\n");
    return;
}


EXPORT_SYMBOL(kobj_switch);
EXPORT_SYMBOL(kobj_debug);
EXPORT_SYMBOL(bsp_get_board_data);
EXPORT_SYMBOL(bsp_get_slot_data);

EXPORT_SYMBOL(bsp_cpld_read_byte);
EXPORT_SYMBOL(bsp_cpld_write_byte);
EXPORT_SYMBOL(bsp_cpld_read_part);
EXPORT_SYMBOL(bsp_cpld_write_part);
EXPORT_SYMBOL(bsp_cpld_set_bit);
EXPORT_SYMBOL(bsp_cpld_get_bit);
EXPORT_SYMBOL(bsp_cpu_cpld_read_byte);
EXPORT_SYMBOL(bsp_cpu_cpld_write_byte);
EXPORT_SYMBOL(bsp_slot_cpld_read_byte);
EXPORT_SYMBOL(bsp_slot_cpld_write_byte);
EXPORT_SYMBOL(bsp_slot_cpld_set_bit);
EXPORT_SYMBOL(bsp_slot_cpld_get_bit);


EXPORT_SYMBOL(bsp_cpld_get_size);
EXPORT_SYMBOL(bsp_get_cpu_cpld_size);
EXPORT_SYMBOL(bsp_cpld_get_slot_size);


EXPORT_SYMBOL(bsp_get_product_type);
EXPORT_SYMBOL(bsp_get_product_name_string);

EXPORT_SYMBOL(lock_i2c_path);
EXPORT_SYMBOL(unlock_i2c_path);
EXPORT_SYMBOL(bsp_i2c_24LC128_eeprom_read_bytes);
EXPORT_SYMBOL(bsp_i2c_24LC128_eeprom_write_byte);
EXPORT_SYMBOL(bsp_i2c_SFP_read_bytes);
EXPORT_SYMBOL(bsp_i2c_SFP_write_byte);
//EXPORT_SYMBOL(bsp_i2c_9545_write_byte);
//EXPORT_SYMBOL(bsp_i2c_9548_write_byte);
EXPORT_SYMBOL(bsp_i2c_LM75_get_temp);
EXPORT_SYMBOL(bsp_i2c_Max6696_get_temp);
EXPORT_SYMBOL(bsp_i2c_Max6696_reg_read);
EXPORT_SYMBOL(bsp_i2c_Max6696_reg_write);
EXPORT_SYMBOL(bsp_i2c_Max6696_limit_rw);
EXPORT_SYMBOL(bsp_cpld_reset_max6696);
EXPORT_SYMBOL(bsp_enable_slot_all_9548);
EXPORT_SYMBOL(bsp_enable_slot_all_9545);


EXPORT_SYMBOL(bsp_i2c_power_reg_read);
EXPORT_SYMBOL(bsp_i2c_common_eeprom_read_bytes);
EXPORT_SYMBOL(bsp_i2c_common_eeprom_write_byte);
EXPORT_SYMBOL(bsp_i2c_power650W_read_current);
EXPORT_SYMBOL(bsp_i2c_power650W_read_voltage);
EXPORT_SYMBOL(bsp_i2c_power650W_read_temperstatus);
EXPORT_SYMBOL(bsp_i2c_power650W_read_temper);
EXPORT_SYMBOL(bsp_i2c_power650W_read_fan_speed);
EXPORT_SYMBOL(bsp_i2c_power650W_read_SN);
EXPORT_SYMBOL(bsp_i2c_power650W_read_powerin);
EXPORT_SYMBOL(bsp_i2c_power650W_read_powerout);
EXPORT_SYMBOL(bsp_i2c_power650W_read_MFR_ID);
EXPORT_SYMBOL(bsp_i2c_power650W_read_pdtname);
EXPORT_SYMBOL(bsp_i2c_power650W_read_vendorname);
/*
EXPORT_SYMBOL(bsp_i2c_power650W_read_voltage_in);
EXPORT_SYMBOL(bsp_i2c_power650W_read_current_in);
*/

EXPORT_SYMBOL(bsp_i2c_power650W_read_hw_version);
EXPORT_SYMBOL(bsp_i2c_power650W_read_fw_version);


EXPORT_SYMBOL(bsp_i2c_power1600W_read_current);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_voltage);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_SN);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_pdtname);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_fan_speed);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_temperstatus);

EXPORT_SYMBOL(bsp_i2c_power1600W_read_powerin);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_powerout);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_temper);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_current_in);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_voltage_in);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_MFR_ID);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_hw_version);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_fw_version);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_voltage_in_type);
EXPORT_SYMBOL(bsp_i2c_power_read_status_word);
EXPORT_SYMBOL(bsp_i2c_power1600W_read_vendorname);


EXPORT_SYMBOL(bsp_i2c_ina219_read_reg);
EXPORT_SYMBOL(bsp_i2c_isl68127_read_reg);
EXPORT_SYMBOL(bsp_i2c_isl68127_write_reg);

EXPORT_SYMBOL(bsp_cpld_set_fan_pwm_reg);
EXPORT_SYMBOL(bsp_cpld_get_fan_pwm_reg);
EXPORT_SYMBOL(bsp_cpld_get_fan_speed);
EXPORT_SYMBOL(bsp_cpld_get_fan_enable);
EXPORT_SYMBOL(bsp_cpld_set_fan_enable);
EXPORT_SYMBOL(bsp_cpld_get_fan_absent);
EXPORT_SYMBOL(bsp_cpld_get_fan_status);
EXPORT_SYMBOL(bsp_cpld_set_fan_led_red);
EXPORT_SYMBOL(bsp_cpld_get_fan_led_red);
EXPORT_SYMBOL(bsp_cpld_set_fan_led_green);
EXPORT_SYMBOL(bsp_cpld_get_fan_led_green);


EXPORT_SYMBOL(bsp_cpld_get_psu_absent);
EXPORT_SYMBOL(bsp_cpld_get_psu_good);
EXPORT_SYMBOL(bsp_cpld_get_slot_absent);
EXPORT_SYMBOL(bsp_cpld_get_card_power_ok);

EXPORT_SYMBOL(bsp_print_memory);

EXPORT_SYMBOL(bsp_get_secondary_voltage_value);
EXPORT_SYMBOL(bsp_h3c_localmsg_to_file);
EXPORT_SYMBOL(bsp_h3c_open_init_log);
EXPORT_SYMBOL(bsp_h3c_close_init_log);
EXPORT_SYMBOL(bsp_reset_smbus_slave);
EXPORT_SYMBOL(optoe_port_index_convert);
EXPORT_SYMBOL(optic_lock);
EXPORT_SYMBOL(optic_unlock);
EXPORT_SYMBOL(bsp_mac_inner_temp_lock);
module_init(bsp_init);
module_exit(bsp_exit);


/*********************************************/
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system cpld driver");
MODULE_LICENSE("Dual BSD/GPL");
/*********************************************/



