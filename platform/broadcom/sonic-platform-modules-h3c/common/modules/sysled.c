#include "bsp_base.h"

static int loglevel = DEBUG_INFO | DEBUG_ERR;
static struct timer_list timer;
static struct work_struct work;
static struct workqueue_struct *led_workqueue = NULL;
static struct kobject *kobj_sysled = NULL;
static struct kobject *kobj_led_debug = NULL;
extern int bsp_fan_get_status(int fan_index);
extern int bsp_psu_get_status(int psu_index);
static int sysled_monitor_task_sleep = 1;

#define MODULE_NAME "sysled"
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt,##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)

enum SYSLED_ATTR {
    SYSLED_LOC_FRONT,
    SYSLED_LOC_REAR,
    BMCLED_LOC_FRONT,
    BMCLED_LOC_REAR,
    FANLED_LOC_FRONT,
    FANLED_LOC_REAR,
    PSULED_LOC_FRONT,
    PSULED_LOC_REAR,
    IDLED_LOC_FRONT,
    IDLED_LOC_REAR,
    DIS_LED_MON,
    DIS_LOGLEVEL,
    DIS_DEBUG,
    SYSLED_ATTR_BUTT
};

struct led_color_status {
  int sys_led_front_color;
  int sys_led_rear_color;
  int bmc_led_front_color;
  int bmc_led_rear_color;
  int fan_led_front_color;
  int fan_led_rear_color;
  int psu_led_front_color;
  int psu_led_rear_color;
  int id_led_front_color;
  int id_led_rear_color;
};

struct led_color_status led_color_status_info = {0};

static int bsp_sysled_set_led_color(int led_location, int color_code)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_board_data();
    int led_green = 0, led_red = 0;
    u8 cpld_addr_green = 0, cpld_offs_green = 0;   
    u8 cpld_addr_red = 0, cpld_offs_red = 0;
    u16 cpld_value = 0;
    u16 cpld_addr_sysled = 0;

    switch(color_code) {
    case LED_COLOR_GREEN:
        led_green = CODE_LED_ON;
        led_red = CODE_LED_OFF;
        break;
    case LED_COLOR_RED:
        led_green = CODE_LED_OFF;
        led_red = CODE_LED_ON;
        break;
    case LED_COLOR_YELLOW:
        led_green = CODE_LED_ON;
        led_red = CODE_LED_ON;
        break;
    case LED_COLOR_DARK:
        led_green = CODE_LED_OFF;
        led_red = CODE_LED_OFF;
        break;
    default:
        if (led_location == IDLED_LOC_FRONT || led_location == IDLED_LOC_REAR)
            break;
        
        CHECK_IF_ERROR_GOTO_EXIT(ret = ERROR_FAILED, "unknown color_code=%d", color_code);
        goto exit;
        break;
    }

    switch(led_location) {
    case SYSLED_LOC_FRONT:
    case SYSLED_LOC_REAR:
        cpld_addr_sysled = bd->cpld_addr_pannel_sys_led_ctrl;
        switch (color_code) {
        case LED_COLOR_GREEN:
            cpld_value = bd->cpld_value_sys_led_code_green;
        break;
        case LED_COLOR_RED:
            cpld_value = bd->cpld_value_sys_led_code_red; 
            break;
        case LED_COLOR_YELLOW:
            cpld_value = bd->cpld_value_sys_led_code_yellow; 
            break;
        case LED_COLOR_DARK:
            cpld_value = bd->cpld_value_sys_led_code_dark; 
            break;
        }
            
        ret = bsp_cpld_write_byte(cpld_value, cpld_addr_sysled);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
        break;
    case BMCLED_LOC_FRONT:
    case BMCLED_LOC_REAR:
        cpld_addr_green = bd->cpld_addr_pannel_bmc_led_green;
        cpld_offs_green = bd->cpld_offs_pannel_bmc_led_green;
        cpld_addr_red = bd->cpld_addr_pannel_bmc_led_red;
        cpld_offs_red = bd->cpld_offs_pannel_bmc_led_red;
        
        ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
        
        ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
        break;
    case FANLED_LOC_FRONT:
    case FANLED_LOC_REAR:
        cpld_addr_green = bd->cpld_addr_pannel_fan_led_green;
        cpld_offs_green = bd->cpld_offs_pannel_fan_led_green;
        cpld_addr_red = bd->cpld_addr_pannel_fan_led_red;
        cpld_offs_red = bd->cpld_offs_pannel_fan_led_red;
        
        ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
        
        ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
        break;
    case PSULED_LOC_FRONT:
    case PSULED_LOC_REAR:
        cpld_addr_green = bd->cpld_addr_pannel_psu_led_green;
        cpld_offs_green = bd->cpld_offs_pannel_psu_led_green;
        cpld_addr_red = bd->cpld_addr_pannel_psu_led_red;
        cpld_offs_red = bd->cpld_offs_pannel_psu_led_red;
        
        ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
        
        ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
        break;
    case IDLED_LOC_FRONT:
    case IDLED_LOC_REAR:
        led_red =   (color_code != LED_COLOR_DARK) ? CODE_LED_ON : CODE_LED_OFF;
        led_green = (color_code != LED_COLOR_DARK) ? CODE_LED_ON : CODE_LED_OFF;
        cpld_addr_green = bd->cpld_addr_pannel_id_led_blue;
        cpld_offs_green = bd->cpld_offs_pannel_id_led_blue;
        cpld_addr_red = bd->cpld_addr_pannel_id_led_blue;
        cpld_offs_red = bd->cpld_offs_pannel_id_led_blue;
        
        ret = bsp_cpld_set_bit(cpld_addr_red, cpld_offs_red, led_red);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led red bit failed");
        
        ret = bsp_cpld_set_bit(cpld_addr_green, cpld_offs_green, led_green);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "set led green bit failed");
        break;
    default:
        CHECK_IF_ERROR_GOTO_EXIT(ret = ERROR_FAILED, "unknown led_location=%d", led_location);
        goto exit;
        break;
    }
 
exit:
    return ret;
}

static ssize_t bsp_sysled_get_loglevel (char *buf)
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

static ssize_t bsp_sysled_debug_help (char *buf)
{
    ssize_t index = 0;

    index += sprintf (buf + index, "%s", " Sysled monitor control command:\n");
    index += sprintf (buf + index, "%s", "   0 ------ enable monitor\n");
    index += sprintf (buf + index, "%s", "   1 ------ disable monitor\n\n");
    index += sprintf (buf + index, "%s", " Disable/Enable monitor:\n");
    index += sprintf (buf + index, "%s", "   echo command > /sys/switch/debug/psu/disable_led_mon\n\n");
    index += sprintf (buf + index, "%s", "   eg:\n\n");
    index += sprintf (buf + index, "%s", "      echo 1 > /sys/switch/debug/sysled/disable_led_mon\n\n");
    index += sprintf (buf + index, "%s", " Set pannel led on/off:\n");
    index += sprintf (buf + index, "%s", "   Warning: To manually control the led, the first step is to disable the led monitoring tasks.\n\n");
    index += sprintf (buf + index, "%s", "   eg:\n\n");
    index += sprintf (buf + index, "%s", "      root@sonic: echo 0 > /sys/switch/sysled/fan_led_status_front\n\n");
    index += sprintf (buf + index, "%s", "      root@sonic: echo 1 > /sys/switch/sysled/fan_led_status_front\n");

    return index;
}

static ssize_t bsp_sysled_sysfs_get_led_status (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!strcmp(attr->attr.name, "sys_led_status_front"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.sys_led_front_color);
    else if (!strcmp(attr->attr.name, "sys_led_status_rear"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.sys_led_rear_color);
    else if (!strcmp(attr->attr.name, "bmc_led_status_front"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.bmc_led_front_color);
    else if (!strcmp(attr->attr.name, "bmc_led_status_rear"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.bmc_led_rear_color);
    else if (!strcmp(attr->attr.name, "fan_led_status_front"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.fan_led_front_color);
    else if (!strcmp(attr->attr.name, "fan_led_status_rear"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.fan_led_rear_color);
    else if (!strcmp(attr->attr.name, "psu_led_status_front"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.psu_led_front_color);
    else if (!strcmp(attr->attr.name, "psu_led_status_rear"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.psu_led_rear_color);
    else if (!strcmp(attr->attr.name, "id_led_status_front"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.id_led_front_color);
    else if (!strcmp(attr->attr.name, "id_led_status_rear"))
        return snprintf(buf, PAGE_SIZE, "%d\n", led_color_status_info.id_led_rear_color);
    else if (!strcmp(attr->attr.name, "debug"))
        return bsp_sysled_debug_help (buf);
    else if (!strcmp(attr->attr.name, "loglevel"))
        return bsp_sysled_get_loglevel (buf);
    else
        return -EINVAL;
}

static ssize_t bsp_sysled_sysfs_set_led_status (struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int temp = 0;
    
    if ((!strcmp(attr->attr.name, "sys_led_status_front")) || 
        (!strcmp(attr->attr.name, "sys_led_status_rear"))) {
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
        } else {
            bsp_sysled_set_led_color(SYSLED_LOC_FRONT, temp);
            led_color_status_info.sys_led_front_color = temp;
            led_color_status_info.sys_led_rear_color = temp;
        }
    } else if ((!strcmp(attr->attr.name, "bmc_led_status_front")) ||
               (!strcmp(attr->attr.name, "bmc_led_status_rear"))) {
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
        } else {
            bsp_sysled_set_led_color(BMCLED_LOC_FRONT, temp);
            led_color_status_info.bmc_led_front_color = temp;
            led_color_status_info.bmc_led_rear_color = temp;
        }           
    } else if ((!strcmp(attr->attr.name, "fan_led_status_front")) ||
               (!strcmp(attr->attr.name, "fan_led_status_rear"))) {
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
        } else {
            bsp_sysled_set_led_color(FANLED_LOC_FRONT, temp);
            led_color_status_info.fan_led_front_color = temp;
            led_color_status_info.fan_led_rear_color = temp;
        }           
    } else if ((!strcmp(attr->attr.name, "psu_led_status_front")) ||
               (!strcmp(attr->attr.name, "psu_led_status_rear"))) {
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
        } else {
            bsp_sysled_set_led_color(PSULED_LOC_FRONT, temp);
            led_color_status_info.psu_led_front_color = temp;
            led_color_status_info.psu_led_rear_color = temp;
        }           
    } else if ((!strcmp(attr->attr.name, "id_led_status_front")) ||
               (!strcmp(attr->attr.name, "id_led_status_rear"))) {
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);
        } else {
            bsp_sysled_set_led_color(IDLED_LOC_FRONT, temp);
            led_color_status_info.id_led_front_color = temp;
            led_color_status_info.id_led_rear_color = temp;
        }           
    } else if (!strcmp(attr->attr.name, "debug")) {
         DBG_ECHO(DEBUG_DBG, "Notice: set debug value is faild.\n");
    } else if (!strcmp(attr->attr.name, "loglevel")) {
        if (buf[1] == 'x') {
            sscanf(buf, "%x", &loglevel);
        } else {
            sscanf(buf, "%d", &loglevel);
        }
        
        DBG_ECHO(DEBUG_INFO, "set loglevel %x ...\n", loglevel);
    } else {
        DBG_ECHO(DEBUG_INFO, "Not support attribute name %s ", attr->attr.name);
    }
       
    return count;
}

static ssize_t bsp_sysled_sysfs_get_led_debug (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!strcmp(attr->attr.name, "disable_led_mon"))
        return sprintf(buf, "%d\n", sysled_monitor_task_sleep);
    else
        return -EINVAL;
}

static ssize_t bsp_sysled_sysfs_set_led_debug (struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int temp = 0;

    if (!strcmp(attr->attr.name, "disable_led_mon")) {
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "integer format expected, given '%s'", buf);    
        } else {
            sysled_monitor_task_sleep = (temp != 0);
        }
    } else {
        DBG_ECHO(DEBUG_INFO, "Not support attribute name %s ", attr->attr.name);
    }

    return count;
}

static struct kobj_attribute sysled_loglevel_att =
    __ATTR(loglevel, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_debug_att =
    __ATTR(debug, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_sys_status_front_att =
    __ATTR(sys_led_status_front, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_sys_status_rear_att =
    __ATTR(sys_led_status_rear, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_bmc_status_front_att =
    __ATTR(bmc_led_status_front, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_bmc_status_rear_att =
    __ATTR(bmc_led_status_rear, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_fan_status_front_att =
    __ATTR(fan_led_status_front, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_fan_status_rear_att =
    __ATTR(fan_led_status_rear, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_psu_status_front_att =
    __ATTR(psu_led_status_front, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_psu_status_rear_att =
    __ATTR(psu_led_status_rear, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_id_status_front_att =
    __ATTR(id_led_status_front, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_id_status_rear_att =
    __ATTR(id_led_status_rear, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_status, bsp_sysled_sysfs_set_led_status);

static struct kobj_attribute sysled_disable_led_mon_att =
    __ATTR(disable_led_mon, S_IRUGO | S_IWUSR, bsp_sysled_sysfs_get_led_debug, bsp_sysled_sysfs_set_led_debug);

static struct attribute *sysled_attributes[] = {
    &sysled_loglevel_att.attr,
    &sysled_debug_att.attr,
    &sysled_sys_status_front_att.attr,
    &sysled_sys_status_rear_att.attr,
    &sysled_bmc_status_front_att.attr,
    &sysled_bmc_status_rear_att.attr,
    &sysled_fan_status_front_att.attr,
    &sysled_fan_status_rear_att.attr,
    &sysled_psu_status_front_att.attr,
    &sysled_psu_status_rear_att.attr,
    &sysled_id_status_front_att.attr,
    &sysled_id_status_rear_att.attr,
    NULL,
};

static const struct attribute_group sysled_group = {
    .attrs = sysled_attributes,
};

static struct attribute *sysled_debug_attributes[] = {
    &sysled_disable_led_mon_att.attr,
    NULL,
};

static const struct attribute_group sysled_debug_group = {
    .attrs = sysled_debug_attributes,
};

static void bsp_sysled_monitor_proc (void) 
{
    int psu_number, psu_index;
    int fan_number, fan_index;
    int fan_led_status;
    int psu_led_status;
    int sys_led_status;
    int fan_status;
    int psu_status;
    board_static_data *bd = bsp_get_board_data();
    
    fan_number = bd->fan_num;
    psu_number = bd->psu_num;
    fan_led_status = LED_COLOR_GREEN;
    psu_led_status = LED_COLOR_GREEN;
    sys_led_status = LED_COLOR_GREEN;

    for (fan_index=0; fan_index < fan_number; fan_index++) {
        fan_status = bsp_fan_get_status(fan_index);
        if (fan_status == FAN_STATUS_FAULT) {
            fan_led_status = LED_COLOR_RED;
            break;
        }
        
        if (fan_status == FAN_STATUS_ABSENT) {
            fan_led_status = LED_COLOR_YELLOW;
        }
    }
    
    for (psu_index=0; psu_index < psu_number; psu_index++) {
        psu_status = bsp_psu_get_status(psu_index);
        if (psu_status == PSU_STATUS_FAULT) {
            psu_led_status = LED_COLOR_RED;
            break;
        }
        
        if (psu_status == PSU_STATUS_ABSENT) {
            psu_led_status = LED_COLOR_YELLOW;
        }
    }

    if (led_color_status_info.fan_led_front_color != fan_led_status) {
        DBG_ECHO(DEBUG_DBG, "set fan status %d", fan_led_status);
        led_color_status_info.fan_led_front_color = fan_led_status;
        led_color_status_info.fan_led_rear_color = fan_led_status;
        
        bsp_sysled_set_led_color(FANLED_LOC_FRONT, fan_led_status);
        bsp_sysled_set_led_color(FANLED_LOC_REAR,  fan_led_status);
    }
    
    if (led_color_status_info.psu_led_front_color != psu_led_status) {
        DBG_ECHO(DEBUG_DBG, "set psu status %d", psu_led_status);
        led_color_status_info.psu_led_front_color = psu_led_status;
        led_color_status_info.psu_led_rear_color = psu_led_status;

        bsp_sysled_set_led_color(PSULED_LOC_FRONT, psu_led_status);
        bsp_sysled_set_led_color(PSULED_LOC_REAR,  psu_led_status);
    }
    
    if (led_color_status_info.sys_led_front_color != sys_led_status) {
        DBG_ECHO(DEBUG_DBG, "set sys status %d", sys_led_status);
        led_color_status_info.sys_led_front_color = sys_led_status;
        led_color_status_info.sys_led_rear_color = sys_led_status;
        
        bsp_sysled_set_led_color(SYSLED_LOC_FRONT, sys_led_status);
        bsp_sysled_set_led_color(SYSLED_LOC_REAR,  sys_led_status);
    }

    return;
}

static void bsp_sysled_work_handler (struct work_struct *work)
{
    if (!sysled_monitor_task_sleep) {
        bsp_sysled_monitor_proc ();
    } else {
        DBG_ECHO (DEBUG_DBG, "sys led work queue debug information.\n");
    }
    
    mod_timer (&timer, jiffies + 600 * HZ / 1000);
    return;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void bsp_sysled_timer_proc (unsigned long data)
#else
static void bsp_sysled_timer_proc (struct timer_list *timer)
#endif
{
    if (led_workqueue) {
        if (!queue_work (led_workqueue, &work)) {
            DBG_ECHO (DEBUG_ERR, "h3c led queue_work error.\n");
         }
    }

    return;
}

static void bsp_sysled_timer_deinit (void)
{
    del_timer (&timer);
    return;
}

static void bsp_sysled_workqueue_deinit (void)
{
    if (led_workqueue) {
        flush_workqueue (led_workqueue);
        destroy_workqueue (led_workqueue);
        led_workqueue = NULL;        
    }
    
    return;
}

static int bsp_sysled_workqueue_init (void)
{
    led_workqueue = create_workqueue ("sysled_monitor");
    if (!led_workqueue) {
        DBG_ECHO(DEBUG_ERR, "create h3c sys led work queue faild.\n");
        return -ENOMEM;
    }

    INIT_WORK (&work, bsp_sysled_work_handler);
    return 0;
}

static void bsp_sysled_timer_init (void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
    init_timer (&timer);
    timer.function = bsp_sysled_timer_proc;
#else
    timer_setup(&timer, bsp_sysled_timer_proc, 0);
#endif

    timer.expires = jiffies + 600 * HZ / 1000;
    add_timer(&timer);
    return;
}

static int __init sysled_init(void)
{
    int ret = ERROR_SUCCESS;

    memset(&led_color_status_info, 0 ,sizeof(led_color_status_info));
    led_color_status_info.sys_led_front_color = LED_COLOR_DARK;
    led_color_status_info.sys_led_rear_color  = LED_COLOR_DARK;
    led_color_status_info.psu_led_front_color = LED_COLOR_DARK;
    led_color_status_info.fan_led_front_color = LED_COLOR_DARK;
    led_color_status_info.bmc_led_front_color = LED_COLOR_DARK;
    
    kobj_sysled = kobject_create_and_add("sysled", kobj_switch);
    if (!kobj_sysled) {
        DBG_ECHO(DEBUG_ERR, "kobj_switch create falled!\n");          
        ret = -ENOMEM;       
        goto exit;     
    }
    
    ret = sysfs_create_group(kobj_sysled, &sysled_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create group failed");

    kobj_led_debug = kobject_create_and_add("led", kobj_debug);
    CHECK_IF_NULL_GOTO_EXIT(ret, kobj_led_debug, "fan debug kobject created failed!");
    
    ret = sysfs_create_group(kobj_led_debug, &sysled_debug_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "fan debug attribute group created failed!");

    bsp_sysled_timer_init ();
    ret = bsp_sysled_workqueue_init();
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create sys led monitor task return %d is error.\n", ret);
    
exit:
    if (ret != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "module init failed! result=%d\n", ret);

        bsp_sysled_workqueue_deinit ();
        bsp_sysled_timer_deinit ();
        
        if (kobj_led_debug) {
            sysfs_remove_group (kobj_led_debug, &sysled_debug_group);
            kobject_put(kobj_led_debug);
            kobj_led_debug = NULL; 
        }
        
        if (kobj_sysled) {
            sysfs_remove_group (kobj_sysled, &sysled_group);
            kobject_put(kobj_sysled);
            kobj_sysled = NULL;
        }
    } else {
        INIT_PRINT("module finished and success!");
    }

    return ret;
}

static void __exit sysled_exit(void)
{
    bsp_sysled_workqueue_deinit ();
    bsp_sysled_timer_deinit ();
    
    if (kobj_led_debug) {
        sysfs_remove_group (kobj_led_debug, &sysled_debug_group);
        kobject_put(kobj_led_debug);
        kobj_led_debug = NULL;
    }

    if (kobj_sysled) {
        sysfs_remove_group (kobj_sysled, &sysled_group);
        kobject_put(kobj_sysled);
        kobj_sysled = NULL;
    }

    INIT_PRINT("module sysled uninstalled !\n");
}

module_init(sysled_init);
module_exit(sysled_exit);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Wan Huan <wan.huan@h3c.com>");
MODULE_DESCRIPTION("h3c system led driver");
MODULE_LICENSE("Dual BSD/GPL");
