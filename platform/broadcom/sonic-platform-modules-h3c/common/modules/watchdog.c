/*
 *H3C CPLD watchdog driver
 *Copyright (c) Qianchaoyang <qian.chaoyang@h3c.com>,2020
 *
 *based on
 *
 *SoftDog: A Software Watchdog Device
 *
 *
 */
#include "bsp_base.h"

static int loglevel = DEBUG_INFO | DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt,##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)

#define MODULE_NAME "watchdog"
#define MAX_TIME_OUT  408
#define OPTIONS (WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE)

static unsigned long g_wdt_ping_jiffies;
static unsigned int g_soft_margin = 60;
static const struct watchdog_info h3c_wdt_ident = {
     .options           = OPTIONS,
     .firmware_version  = 0,
     .identity          = "CPLD Watchdog",
};

static int watchdog_ping(void)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    ret = bsp_cpu_cpld_write_byte(1, bd->cpld_addr_wd_feed);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld write watchdog FEED_DOG failed!");
    g_wdt_ping_jiffies = jiffies;

exit:
    return ret;

}

static unsigned int h3c_wdt_get_timeleft(struct watchdog_device *wdd)
{
    unsigned long curr_jiffies;
    unsigned int run_time;
    unsigned int left_time;

    curr_jiffies = jiffies;
    run_time = jiffies_to_msecs(curr_jiffies - g_wdt_ping_jiffies) / 1000;
    left_time = g_soft_margin - run_time;

    return left_time;
}
static long watchdog_ioctl(struct watchdog_device *wdd,unsigned int cmd,unsigned long arg)
{
    int new_margin = -1;
    int ret = ERROR_SUCCESS;
    u8 set_value = 1;
    int new_state = -1;
    unsigned int left_time;
    board_static_data *bd = bsp_get_board_data();   
    int __user *int_arg = (int __user *)arg;
    void __user *argp = (void __user *)arg;
    
    switch(cmd) {
    case WDIOC_KEEPALIVE:
        watchdog_ping();
        break;
    case WDIOC_GETSUPPORT:
        ret = copy_to_user(argp, &h3c_wdt_ident, sizeof(h3c_wdt_ident));
        CHECK_IF_ERROR_GOTO_EXIT(ret, "WDIOC_GETSUPPORT failed!");
        break;
    case WDIOC_SETTIMEOUT:
        ret = get_user(new_margin,int_arg);
        if(ret)
            break;
        
        if (new_margin < 0) {
            DBG_ECHO(DEBUG_ERR, "Time_out Value %d is invalid, range 0 ~ %d, ignored", new_margin, MAX_TIME_OUT);
            ret = ERROR_FAILED;
        } else {
            if(new_margin > MAX_TIME_OUT) {
               DBG_ECHO(DEBUG_INFO, "Time_out Value %d is out of range, max =%d, set timeout to max.", new_margin, MAX_TIME_OUT);
               new_margin = MAX_TIME_OUT;
            }
                  
            g_soft_margin = new_margin;
            set_value =(g_soft_margin * 10) / 16;
            ret = bsp_cpu_cpld_write_byte(set_value, bd->cpld_addr_wd_timeout);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld write watchdog timeout failed!");
            watchdog_ping();
        }
        break;
    case WDIOC_GETTIMEOUT:
        ret = put_user(g_soft_margin,int_arg);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld write watchdog wd_enable failed!");
        break;     
    case WDIOC_SETOPTIONS:
        ret = get_user(new_state,int_arg);       
        if(new_state & WDIOS_DISABLECARD) {
            ret = bsp_cpu_cpld_write_byte(0, bd->cpld_addr_wd_enable);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld write watchdog wd_enable failed!");
        }
        
        if(new_state & WDIOS_ENABLECARD) {
            ret = bsp_cpu_cpld_write_byte(1, bd->cpld_addr_wd_enable);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld write watchdog wd_enable failed!");
        }            
        break;     
        case WDIOC_GETTIMELEFT:
            left_time = h3c_wdt_get_timeleft(wdd);
            ret = put_user(left_time, int_arg);
            CHECK_IF_ERROR_GOTO_EXIT(ret, "WDT get timeleft failed!");
            break;
    default:
        DBG_ECHO(DEBUG_ERR, "Not found command %d", cmd);
        break;            
    }
    
exit:
    return ret;
}

static int h3c_wdt_start(struct watchdog_device *wdd)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    u8 get_val = 0;

    ret = bsp_cpu_cpld_read_byte(&get_val, bd->cpld_addr_wd_enable);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld read watchdog enable failed!");
    if (((get_val & bd->cpld_mask_wd_enable) >> bd->cpld_offs_wd_enable) == 0)
    {
        g_wdt_ping_jiffies = jiffies;
        ret = bsp_cpu_cpld_write_byte(1, bd->cpld_addr_wd_enable);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld write watchdog wd_enable failed!");
    }

exit:
    return ret;
}
 
static int h3c_wdt_stop(struct watchdog_device *wdd)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    ret = bsp_cpu_cpld_write_byte(0, bd->cpld_addr_wd_enable);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "cpld write watchdog wd_enable failed!");
    
exit:
    return ret;
}

static int h3c_wdt_ping(struct watchdog_device *wdd)
{
     watchdog_ping();
     return ERROR_SUCCESS;
}

static struct watchdog_ops h3c_wdtops = {
    .owner           = THIS_MODULE,
    .ioctl           = watchdog_ioctl,
    .start           = h3c_wdt_start,
    .stop            = h3c_wdt_stop,
    .ping            = h3c_wdt_ping,
    .get_timeleft    = h3c_wdt_get_timeleft
};
    
static struct watchdog_device h3c_wdd = {
    .info = &h3c_wdt_ident,
    .ops  = &h3c_wdtops,
};

static int __init wdt_init_module(void)
{
    int ret = ERROR_SUCCESS;

    watchdog_set_nowayout(&h3c_wdd, WATCHDOG_NOWAYOUT);
    watchdog_set_restart_priority(&h3c_wdd, 128);
    ret = watchdog_register_device(&h3c_wdd);
    if (ret == ERROR_SUCCESS)
    {
        INIT_PRINT("CPLD watchdog module init finished and success!");
    }
    else
    {
        DBG_ECHO(DEBUG_ERR, "CPLD watchdog module int failed!");
    }


    return ret;
}

static void __exit wdt_cleanup_module(void)
{
    watchdog_unregister_device(&h3c_wdd);
    INIT_PRINT("CPLD watchdog module uninstalled!");
}

module_init(wdt_init_module);
module_exit(wdt_cleanup_module);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Qianchaoyang <qian.chaoyang@h3c.com>");
MODULE_DESCRIPTION("h3c watchdog driver");
MODULE_LICENSE("Dual BSD/GPL");
