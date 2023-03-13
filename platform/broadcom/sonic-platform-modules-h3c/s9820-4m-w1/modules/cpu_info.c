#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/module.h>
#include <linux/rtc.h>
//#include "../include/adapter.h"
//#include "../include/cpu.h"
#include "pub.h"
#include "bsp_base.h"

#define MODULE_NAME "cpu_info"
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT (DEBUG_ERR | DEBUG_INFO, level, fmt,##args)
//static long loglevel = 0x3;

struct task_struct *cpu_monitor_thread;
// add function for getting the voltage of cpu, preparing to tranform to BMC

iic_dev_write_cmd_data cpu_vol_cmd_table [ ] = {{0x0, 0x0, 0x1}, {0x0, 0x0, 0x1}, {0x0, 0x1, 0x1}};
iic_dev_write_cmd_data cpu_vr_temp_cmd_table [ ] = {{0x0, 0x0, 0x1}, {0x0, 0x0, 0x1}};
iic_dev_read_cmd_data cpu_vol_read_addr[ ] = {{0x8B, 0x1}, {0x8B, 1}, {0x8B, 0x1}};
iic_dev_read_cmd_data cpu_vr_read_addr[ ] = {{0x8D, 0x2}, {0x8D,0x2}};

int get_tps53679_common(I2C_DEVICE_E i2c_device_index, u16 i2c_addr[ ], u16 index, iic_dev_write_cmd_data write_cmd_table[ ], iic_dev_read_cmd_data dev_read_cmd_data[ ], u16 *value)
{
    int ret  =  ERROR_SUCCESS;
    int i = 0;
    
    #define RETRY_COUNT 3
    
    lock_i2c_path (i2c_device_index + index);
    /* add code for write eeprom access cmd */
    for( i = 0; i < RETRY_COUNT; i++)
    {
        if((ret = bsp_i2c_tps53679_write_reg(i2c_addr[index], write_cmd_table[index].write_addr, write_cmd_table[index].write_val, write_cmd_table[index].access_len) )== ERROR_SUCCESS)
        {
            //DBG_ECHO(DEBUG_ERR, "tps53679 write block success! block=%d", write_cmd_table[index].write_val);

			break;    
        }
        else
        {
             DBG_ECHO(DEBUG_INFO, "tps53679 write block fail! block=%d", write_cmd_table[index].write_val);;
        }
    }
      CHECK_IF_ERROR_GOTO_EXIT(ret, "tps53679 block select failed, abort to set write cmd!");
    /* add code for cpu vol read */
    if (ERROR_SUCCESS != bsp_i2c_tps53679_read_reg  (i2c_addr[index], dev_read_cmd_data[i].read_addr, value, dev_read_cmd_data[i].access_len))
    {
        DBG_ECHO (DEBUG_INFO, "tps53679 get address 0x%08x failed.\n",  dev_read_cmd_data[i].read_addr);
        unlock_i2c_path();
        return ERROR_FAILED;
    }
    
exit:
    unlock_i2c_path();

    #undef RETRY_COUNT
    return ERROR_SUCCESS;    
}

/* add for getting all of the temp & vol */
u16 cpu_vol_for_bmc[MAX_CPU_VOL_NUM] = {0, 0 ,0};
u16 cpu_vr_temp_for_bmc[MAX_CPU_VR_TEMP_NUM] = {0, 0};

void get_tps53679_vol_temp(void)
{
    int i;
    u16 value;
    board_static_data *bd = bsp_get_board_data ();
    
    for(i = 0; i < sizeof(cpu_vol_cmd_table) / sizeof(cpu_vol_cmd_table[0]); i++)
    {
        get_tps53679_common(I2C_DEV_CPU_VOL, bd->i2c_addr_cpu_vol, i, cpu_vol_cmd_table, cpu_vol_read_addr, &value);
        cpu_vol_for_bmc[i] = value;
    }

    for(i = 0; i < sizeof(cpu_vr_temp_cmd_table) / sizeof(cpu_vr_temp_cmd_table[0]); i++)
    {
        get_tps53679_common(I2C_DEV_CPU_VR, bd->i2c_addr_cpu_vr, i, cpu_vr_temp_cmd_table, cpu_vr_read_addr, &value);
        cpu_vr_temp_for_bmc[i] = value;
    }
}

void monitor_cpu_thread(void)
{
    while(!kthread_should_stop())
    {
        get_tps53679_vol_temp();
        msleep (10000);
    }
    return;
}

int monitor_cpu_thread_fun(void *data)
{
    monitor_cpu_thread();
    return 0;
}

ssize_t get_cpu_core_data(unsigned int index, char *buf)
{
    /*
     * get cpu data
     * forbidden modify the data format
     * if has other data, add it in the back.
     * */
    int cpu_vr_power_temp1 = 0;
    int cpu_vr_power_temp2 = 0;
    int cpu_mem_vol_in = 0;
    int cpu_core_vol_in = 0;
    int cpu_1v05_vol = 0;
  

    // add vendor codes here
    
    if (NULL == buf)
    {
        //CPU_ERR ("function invalid pointer");	
        DBG_ECHO (DEBUG_INFO,"function get_cpu_core_data invalid pointer.");
        return (ssize_t)strlen(buf);;
    }


    cpu_vr_power_temp1 = cpu_vr_temp_for_bmc[0];
    cpu_vr_power_temp2 = cpu_vr_temp_for_bmc[1];
    cpu_mem_vol_in = cpu_vol_for_bmc[0];
    cpu_core_vol_in = cpu_vol_for_bmc[1];
    cpu_1v05_vol = cpu_vol_for_bmc[2];
    
     sprintf(buf + strlen(buf), "    %-16s: %d %s\n", "VrTemperature1", cpu_vr_power_temp1, "Centigrade");
     sprintf(buf + strlen(buf), "    %-16s: %d %s\n", "VrTemperature2", cpu_vr_power_temp1, "Centigrade");
     sprintf(buf + strlen(buf), "    %-16s: %d %s\n", "MemVol", cpu_mem_vol_in, "V");
     sprintf(buf + strlen(buf), "    %-16s: %d %s\n", "CoreVol", cpu_core_vol_in, "V");
     sprintf(buf + strlen(buf), "    %-16s: %d %s\n", "1v05", cpu_1v05_vol, "V");

     return (ssize_t)strlen(buf);
}

EXPORT_SYMBOL(get_cpu_core_data);
int get_cpuinfo_init(void)
{
   // CPU_INFO("...\n");

    memset (cpu_vol_for_bmc, 0, sizeof(cpu_vol_for_bmc));
    memset (cpu_vr_temp_for_bmc, 0, sizeof(cpu_vr_temp_for_bmc));
    
 
    /*创建电源监控任务*/
    cpu_monitor_thread = kthread_create (monitor_cpu_thread_fun, NULL, "h3c_cpu_monitor");
    if (!IS_ERR (cpu_monitor_thread))
    {
        wake_up_process (cpu_monitor_thread);
    }
    else
    {
       // CPU_ERR ("create cpu monitor task faild.");
		
        DBG_ECHO (DEBUG_INFO,"create cpu monitor task faild.");
    }
        
    //CPU_INFO("ok.\n");
 
    return 0;
}

void get_cpuinfo_exit(void){

    /*回收线程*/
    if (NULL != cpu_monitor_thread)
    {
        kthread_stop (cpu_monitor_thread);
        cpu_monitor_thread = NULL;
    }

    return;
}

module_init(get_cpuinfo_init);
module_exit(get_cpuinfo_exit);

MODULE_LICENSE("GPL");
// Change "odm" to your company's name
MODULE_AUTHOR("h3c");
MODULE_DESCRIPTION(" h3c cpu info driver");


 


 





