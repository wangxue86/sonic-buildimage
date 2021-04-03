#include "bsp_base.h"
#include "static_ktype.h"

#define MODULE_NAME "slot"
void release_all_slot_kobj(void);

static int loglevel = DEBUG_INFO | DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt,##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)

#define DECODE_NAME_MAX             20
#define TLV_DECODE_VALUE_MAX_LEN    ((5 * TLV_VALUE_MAX_LEN) + 1)
#define TLV_VALUE_MAX_LEN           255
#define BOARD_OFFSET                0
#define BOARD_INFO                  44
#define BOARD_INFO_OFFSET           0
#define MANUFACTURELEN              256
#define PRODUCTNAMELEN              256
#define SERIALNUMBERLEN             256
#define PARTNUMBERLEN               256

enum senosr_custom_attributes {
    TEMP_ALIAS,
    TEMP_TYPE,
    TEMP_MAX,
    TEMP_MAX_HYST,
    TEMP_MIN,
    TEMP_INPUT
};
    
enum slot_sysfs_attributes {
    NUM_SLOT,
    PRODUCT_NAME,
    HW_VERSION,
    SN,
    PN,
    STATUS,
    LED_STATUS,
    NUM_TEMP_SENSORS,
    NUM_IN_SENSORS,
    NUM_CURR_SENSORS,
};

struct st_slot_info {
    int slot_index;
    int led_status;
    int lm75_num;
    struct kobject kobj_slot;
};

struct st_lm75_info {
    int slot_index;
    int lm75_index;
    char * temp_type;
    char * temp_alias;
    struct kobject kobj_lm75;
};

typedef struct    __attribute__ ((__packed__))  board_info_area_s {
    char      version;  /* 0x08        Structure version */
    u_int8_t    totallen; /* 0x09 - 0x0A Length of all data which follows */
    u_int8_t  languageCode;
    u_int8_t  Mfg_Date_Time[3];
    u_int8_t  manufacture[MANUFACTURELEN];
    u_int8_t  productname[PRODUCTNAMELEN];
    u_int8_t  serialnumber[SERIALNUMBERLEN];
    u_int8_t  partnumber[PARTNUMBERLEN];
} board_info;

typedef struct    __attribute__ ((__packed__))  tlvinfo_header_s {
    u_int8_t version1;   /* 0x00 - 0x07 EEPROM Tag "TlvInfo" */
    u_int8_t internal_offset;  /* 0x08        Structure version */
    u_int8_t chassis_offset;
    u_int8_t board_offset;
    u_int8_t product_offset;
    u_int8_t multirecord_offset;
    u_int8_t pad;
    u_int8_t checksum;
} tlvinfo_header_t;

typedef struct    __attribute__ ((__packed__))  board_info_header_s {
    char      version;  /* 0x08        Structure version */
    u_int8_t    totallen; 
    u_int8_t languageCode;
    u_int8_t Mfg_Date_Time[3];
} board_info_header_t;

typedef struct __attribute__ ((__packed__)) tlvinfo_tlv_s {
    unsigned  char length;
    unsigned  char value[0];
} tlvinfo_tlv_t;

static struct kobject *kobj_slot_root = NULL;
static struct st_slot_info slot_info[MAX_SLOT_NUM];
static struct st_lm75_info lm75_info[MAX_LM75_NUM];

u_int8_t RX_checksum (u_int8_t *buf, int len)
{
    int i;
    u_int8_t ret = 0;
    
    for(i=0; i < len; i++) {
        ret += *(buf++);
    }
    
    ret = ret + 1;
    return ret;
}

static inline bool is_valid_tlv (tlvinfo_tlv_t* tlv)
{
    return ((tlv->length != 0x00) && (tlv->length != 0xFF));
}

board_info *read_binary(unsigned char *eeprom, int totall, u8 *value)
{
    int tlv_end;
    int curr_tlv;
    int num =0;
    board_info_header_t* eeprom_info = NULL;
    board_info* board_infos = NULL;
    unsigned char* eeprom_begin;
    tlvinfo_header_t* eeprom_hdr = (tlvinfo_header_t*)(eeprom+be16_to_cpu(BOARD_OFFSET));
    tlvinfo_tlv_t*    eeprom_tlv;
    
    eeprom_begin = eeprom + (eeprom_hdr->board_offset);
    eeprom_info =(board_info_header_t*)(eeprom + (int)(eeprom_hdr->board_offset));
    
    if(RX_checksum(eeprom,totall) != 0) {
        strcpy(value,"000000failed");
        board_infos = (board_info*)(value + BOARD_INFO_OFFSET);
        DBG_ECHO(DEBUG_ERR, "checksum invalid\n");
        return board_infos;
    }
    
    memcpy(value, eeprom_begin, sizeof(board_info_header_t));
    board_infos = (board_info*)(value + BOARD_INFO_OFFSET);
    curr_tlv = sizeof(board_info_header_t) + (eeprom_hdr->board_offset);
    tlv_end = sizeof(board_info_header_t) + (eeprom_info->totallen) - 16;
    
    while(curr_tlv < tlv_end) {
        eeprom_tlv =(tlvinfo_tlv_t*)&eeprom[curr_tlv];
        
        if(!is_valid_tlv(eeprom_tlv)) {
            strcpy(value,"00000failed");
            board_infos=(board_info*)(value + BOARD_INFO_OFFSET);
            DBG_ECHO(DEBUG_ERR, "eeprom tlv invalid\n");
            return board_infos;
        } else {
            switch(num) {
                case 0:
                    memcpy(board_infos->manufacture, eeprom_tlv->value, eeprom_tlv->length);
                    break;
                case 1:
                    memcpy(board_infos->productname, eeprom_tlv->value, eeprom_tlv->length);
                    break;
                case 2:
                    memcpy(board_infos->serialnumber, eeprom_tlv->value, eeprom_tlv->length);
                    break;
                case 3:
                    memcpy(board_infos->partnumber, eeprom_tlv->value, eeprom_tlv->length);
                    break;
                default:
                    DBG_ECHO(DEBUG_ERR, "eeprom tlv invalid\n");
                    break;
            }
            
            num++;
        }
        
        curr_tlv += sizeof(tlvinfo_tlv_t) + eeprom_tlv->length;
    }
    
    return board_infos;
}

int bsp_slot_get_hw_version (int slot_index, OUT int *version)
{
    u8 pcb_version = 0;
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);
    CHECK_IF_NULL_GOTO_EXIT(ret, bd, "get slot index %d data failed", slot_index);

    ret = bsp_slot_cpld_read_byte(slot_index, &pcb_version, bd->cpld_addr_pcb_ver);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "get slot index %d hw version from cpld failed!", slot_index);

    pcb_version = (pcb_version & bd->cpld_mask_pcb_ver) >> bd->cpld_offs_pcb_ver;
    *version = pcb_version;
    
exit:
    return ret;
}

int bsp_slot_get_status (int slot_index, OUT int *status)
{
    u8 absent = 0;
    u8 power_ok = 0;
    u8 power_on = 0;
    int ret = ERROR_SUCCESS;
    board_static_data * bd = NULL;
    *status = SLOT_STATUS_UNKNOWN;
    
    CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_get_slot_absent(&absent, slot_index), "get slot absent from cpld failed for slot index %d", slot_index);
    if (absent == TRUE) {
        *status = SLOT_STATUS_ABSENT;
        goto exit;
    } else {
        bd = bsp_get_slot_data(slot_index);
        CHECK_IF_NULL_GOTO_EXIT(ret, bd, "get slot index % data failed!", slot_index);
        CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_cage_power_on, bd->cpld_offs_cage_power_on, &power_on), "get card power on status for slot index %d failed!", slot_index);

        if (power_on == 1) {
            CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_get_card_power_ok(&power_ok, slot_index), "get card power ok failed for slot index %d", slot_index);
            *status = (power_ok == 1) ? SLOT_STATUS_NORMAL : SLOT_STATUS_FAULT;        
        } else {
            *status = SLOT_STATUS_UNKNOWN;
        }

        goto exit;
    }
    
 exit:   
    return ret;
}

int bsp_set_led_color_for_slot(int color_code, int slot_index)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_board_data(); 
    CHECK_IF_NULL_GOTO_EXIT(ret, bd, "slot index %d data is NULL! set led color failed", slot_index);

    switch (color_code)  {
    case LED_COLOR_RED:
        ret = bsp_cpld_write_byte(bd->cpld_value_sys_led_code_red,    bd->cpld_addr_slot_sysled[slot_index]); 
        break;
    case LED_COLOR_GREEN:
        ret = bsp_cpld_write_byte(bd->cpld_value_sys_led_code_green,  bd->cpld_addr_slot_sysled[slot_index]); 
        break;
    case LED_COLOR_YELLOW:
        ret = bsp_cpld_write_byte(bd->cpld_value_sys_led_code_yellow, bd->cpld_addr_slot_sysled[slot_index]); 
        break;
    case LED_COLOR_DARK:
        ret = bsp_cpld_write_byte(bd->cpld_value_sys_led_code_dark,   bd->cpld_addr_slot_sysled[slot_index]); 
        break;
    default:
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_INFO, "Not support color code %d", color_code);
    }
    
exit:
    if (ERROR_SUCCESS == ret) {
        slot_info[slot_index].led_status = color_code;
        return ERROR_SUCCESS;
    } else {
        DBG_ECHO (DEBUG_ERR, "set led color code %d failed for slot %d", color_code, slot_index);
        return ERROR_FAILED;
    }
}

int bsp_slot_get_eeprom (int slot_index, u8 *eeprom_buff, int buff_len)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);
    int current_slot_eeprom_start_index = 0;
    
    CHECK_IF_NULL_GOTO_EXIT(ret, bd, "get slot index %d data is NULL", slot_index);
    current_slot_eeprom_start_index = GET_I2C_DEV_EEPROM_IDX_START_SLOT(slot_index);
    
    if (lock_i2c_path(current_slot_eeprom_start_index) == ERROR_SUCCESS) {
        ret = bsp_i2c_24LC128_eeprom_read_bytes(bd->i2c_addr_eeprom, 0, bd->eeprom_used_size > buff_len ? buff_len : bd->eeprom_used_size, eeprom_buff);
    }
    unlock_i2c_path();
    
exit:
    return ret;
}

int bsp_slot_get_lm75_temp(int slot_index, int lm75_index, s16 *temp)
{
    int ret = ERROR_SUCCESS;
    int current_slot_lm75_start_index = 0;
    board_static_data *bd = bsp_get_slot_data(slot_index);
   
    CHECK_IF_NULL_GOTO_EXIT(ret, bd, "get board data is NULL for slot %d", slot_index);
    current_slot_lm75_start_index = GET_I2C_DEV_LM75_IDX_START_SLOT(slot_index);
    
    if (lock_i2c_path(current_slot_lm75_start_index) == ERROR_SUCCESS) {
        ret = bsp_i2c_LM75_get_temp(bd->i2c_addr_lm75[lm75_index], temp);
    }
    unlock_i2c_path();
    
    CHECK_IF_ERROR_GOTO_EXIT(ret, "slot index %d get lm75 index %d temperature failed!", slot_index, lm75_index);
    
exit:
    return ret;
}

static ssize_t bsp_slot_sysfs_get_attr(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;
    int rv = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_slot_info * slot_ptr = container_of((struct kobject *)kobjs, struct st_slot_info, kobj_slot);
    int slot_index = slot_ptr->slot_index;
    board_static_data *bd = bsp_get_board_data();
    board_info* board_infos1 = NULL;
    board_static_data *subcard = NULL;
    
    int temp_32value = 0;
    u8  eeprom_buff[512] = {0};
    u8 *value_buffer = NULL;
    
    switch(attr->index) {
    case NUM_SLOT:
        len = sprintf(buf, "%d", (int)bd->slot_num);
        break;
    case SN:
    case PN:
    case PRODUCT_NAME:
        rv = bsp_slot_get_eeprom(slot_index, eeprom_buff, sizeof(eeprom_buff));
        if (!rv) {
            if (NULL != (value_buffer = (u8 *)kmalloc(TLV_DECODE_VALUE_MAX_LEN, GFP_KERNEL))) {
                memset(value_buffer, 0, TLV_DECODE_VALUE_MAX_LEN);
                board_infos1 = read_binary(eeprom_buff, sizeof(eeprom_buff), value_buffer);
            
                if (attr->index == PRODUCT_NAME) {
                    len = sprintf(buf, "%s", board_infos1->productname);
                } else if (attr->index == SN) {
                    len = sprintf(buf, "%s", board_infos1->serialnumber);
                } else if (attr->index == PN) {
                    len = sprintf(buf, "%s", board_infos1->partnumber);
                } else {
                    DBG_ECHO (DEBUG_ERR, "attr index %d is invalid.\n", attr->index);
                    len = sprintf(buf, "%s", "Unknown");
                }
                
                kfree(value_buffer);
            } else {
                DBG_ECHO(DEBUG_ERR, "kmalloc failed!");
            }
        } 
        break; 
    case HW_VERSION:
        CHECK_IF_ERROR_GOTO_EXIT(bsp_slot_get_hw_version(slot_index, &temp_32value), "get hw version failed for slot %d", slot_index);
        len = sprintf(buf, "%d", temp_32value);
        break;
    case STATUS:
        CHECK_IF_ERROR_GOTO_EXIT(bsp_slot_get_status(slot_index, &temp_32value), "get slot status failed for slot %d", slot_index);
        len = sprintf(buf, "%d", temp_32value);
        break;
    case LED_STATUS:
        len = sprintf(buf, "%d", slot_ptr->led_status);
        break;
    case NUM_CURR_SENSORS:
    case NUM_IN_SENSORS:
        len = sprintf(buf, "%d", 0);
        break;
    case NUM_TEMP_SENSORS:
        subcard = bsp_get_slot_data(slot_index);
        if ((subcard != NULL) && 
            (subcard->initialized)) {
            len = sprintf(buf, "%d", (int)subcard->lm75_num);
        } else {
            len = sprintf(buf, "invalid slot index %d or slot data not initialized", slot_index);
        }
        break;
    default:
        len = sprintf(buf, "Not supported attribute index  %d\n", attr->index);
        break;
    }
    
exit: 
    return len;
}

static ssize_t bsp_slot_sysfs_set_attr (struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    struct kobject * okobjs = (struct kobject*)kobjs;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_slot_info * slot_ptr = container_of((struct kobject *)kobjs, struct st_slot_info, kobj_slot);
    int slot_index = slot_ptr->slot_index;
    int temp = 0;
    
    switch(attr->index) {
    case LED_STATUS:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "set led status failed for format error '%s'", buf);
            return count;
        }

        if (ERROR_SUCCESS != bsp_set_led_color_for_slot(temp, slot_index)) {
            DBG_ECHO(DEBUG_ERR, "set led color failed for slot index %d color %d", slot_index, temp);
        }
        break;
    default:
        DBG_ECHO(DEBUG_INFO, "not support write attribute %s -> %s", okobjs->name, attr->dev_attr.attr.name);
        break;
    }
    
    DBG_ECHO(DEBUG_DBG, "store called kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);

    return count;
}

static ssize_t bsp_sysfs_temp_sensor_lm75_get_attr(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;
    s16 temperature = 0;
    int ret = ERROR_SUCCESS;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_lm75_info * lm75_ptr = container_of((struct kobject *)kobjs, struct st_lm75_info, kobj_lm75);
    int slot_index = lm75_ptr->slot_index;
    int lm75_index = lm75_ptr->lm75_index;
    board_static_data *bd = bsp_get_slot_data(slot_index);
    CHECK_IF_NULL_GOTO_EXIT(ret, bd, "get slot index %d static data is NULL", slot_index);
    
    switch(attr->index) {
    case TEMP_INPUT:
        if (bsp_slot_get_lm75_temp(slot_index, lm75_index, &temperature) == ERROR_SUCCESS) {
            len = sprintf(buf, "%d.%d\n", ((int)temperature) / 2, (temperature & 0x1) ? 5 : 0);
        } else {
            len = sprintf(buf, "get temp failed!\n");
        }
        break;
    case TEMP_TYPE:
        len = sprintf(buf, "%s\n", lm75_ptr->temp_type);
        break;
    case TEMP_ALIAS:
        len = sprintf(buf, "%s\n", lm75_ptr->temp_alias);
        break;
    case TEMP_MIN:
        len = sprintf(buf, "%s\n", "NA");
        break;
    case TEMP_MAX:
        len = sprintf(buf, "%s\n", "NA");
        break;
    case TEMP_MAX_HYST:
        len = sprintf(buf, "%d.0\n", LM75_DEFAULT_HYST);
        break;
    }
    
exit:
    return len;
}

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static ssize_t bsp_default_loglevel_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
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

static ssize_t bsp_default_loglevel_store (struct kobject *kobj, struct kobj_attribute *attr, const char* _buf, size_t count)
{
    if (_buf[1] == 'x') {
        sscanf(_buf, "%x", &loglevel);
    } else {
        sscanf(_buf, "%d", &loglevel);
    }

    DBG_ECHO(DEBUG_INFO, "set loglevel %x ...\n", loglevel);
    return count;
}

static ssize_t bsp_default_debug_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    //debug_help();
    return snprintf (buf, PAGE_SIZE, " %s\n", "N/A");
}

static ssize_t bsp_default_debug_store (struct kobject *kobj, struct kobj_attribute *attr, const char* buf, size_t count)
{
    //debug();
	return count;
}
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static SENSOR_DEVICE_ATTR(num_slot              , S_IRUGO, bsp_slot_sysfs_get_attr, NULL,  NUM_SLOT);
static SENSOR_DEVICE_ATTR(product_name          , S_IRUGO, bsp_slot_sysfs_get_attr, NULL,  PRODUCT_NAME);
static SENSOR_DEVICE_ATTR(hw_version            , S_IRUGO, bsp_slot_sysfs_get_attr, NULL, HW_VERSION);
static SENSOR_DEVICE_ATTR(sn                    , S_IRUGO, bsp_slot_sysfs_get_attr, NULL, SN);
static SENSOR_DEVICE_ATTR(pn                    , S_IRUGO, bsp_slot_sysfs_get_attr, NULL, PN);
static SENSOR_DEVICE_ATTR(status                , S_IRUGO, bsp_slot_sysfs_get_attr, NULL, STATUS);
static SENSOR_DEVICE_ATTR(led_status            , S_IRUGO|S_IWUSR, bsp_slot_sysfs_get_attr, bsp_slot_sysfs_set_attr, LED_STATUS);
static SENSOR_DEVICE_ATTR(num_temp_sensors      , S_IRUGO, bsp_slot_sysfs_get_attr, NULL, NUM_TEMP_SENSORS);
static SENSOR_DEVICE_ATTR(num_in_sensors        , S_IRUGO, bsp_slot_sysfs_get_attr, NULL, NUM_IN_SENSORS);
static SENSOR_DEVICE_ATTR(num_curr_sensors      , S_IRUGO, bsp_slot_sysfs_get_attr, NULL, NUM_CURR_SENSORS);
static SENSOR_DEVICE_ATTR(temp_alias, S_IRUGO, bsp_sysfs_temp_sensor_lm75_get_attr, NULL, TEMP_ALIAS);
static SENSOR_DEVICE_ATTR(temp_type, S_IRUGO, bsp_sysfs_temp_sensor_lm75_get_attr, NULL, TEMP_TYPE);
static SENSOR_DEVICE_ATTR(temp_min, S_IRUGO, bsp_sysfs_temp_sensor_lm75_get_attr, NULL, TEMP_MIN);
static SENSOR_DEVICE_ATTR(temp_max, S_IRUGO, bsp_sysfs_temp_sensor_lm75_get_attr, NULL, TEMP_MAX);
static SENSOR_DEVICE_ATTR(temp_input, S_IRUGO, bsp_sysfs_temp_sensor_lm75_get_attr, NULL, TEMP_INPUT);
static SENSOR_DEVICE_ATTR(temp_max_hyst, S_IRUGO, bsp_sysfs_temp_sensor_lm75_get_attr, NULL, TEMP_MAX_HYST);

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct kobj_attribute loglevel_att =
    __ATTR(loglevel, S_IRUGO | S_IWUSR, bsp_default_loglevel_show, bsp_default_loglevel_store);

static struct kobj_attribute debug_att =
    __ATTR(debug, S_IRUGO | S_IWUSR, bsp_default_debug_show, bsp_default_debug_store);
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *custom_temp_sensor_attributes[] = {
    &sensor_dev_attr_temp_alias.dev_attr.attr,
    &sensor_dev_attr_temp_type.dev_attr.attr,
    &sensor_dev_attr_temp_min.dev_attr.attr,
    &sensor_dev_attr_temp_max.dev_attr.attr,
    &sensor_dev_attr_temp_max_hyst.dev_attr.attr,
    &sensor_dev_attr_temp_input.dev_attr.attr,
    NULL
};
    
static struct attribute *slot_attributes[] = {
    &sensor_dev_attr_product_name.dev_attr.attr,
    &sensor_dev_attr_hw_version.dev_attr.attr,
    &sensor_dev_attr_sn.dev_attr.attr,
    &sensor_dev_attr_pn.dev_attr.attr,
    &sensor_dev_attr_status.dev_attr.attr,
    &sensor_dev_attr_led_status.dev_attr.attr,
    &sensor_dev_attr_num_temp_sensors.dev_attr.attr,
    &sensor_dev_attr_num_in_sensors.dev_attr.attr,
    &sensor_dev_attr_num_curr_sensors.dev_attr.attr,
    NULL
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute *def_attrs[] = {
    &loglevel_att.attr,
    &debug_att.attr,
    NULL,
};

static struct attribute_group def_attr_group = {
    .attrs = def_attrs,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *slot_root_attributes[] = {
    &sensor_dev_attr_num_slot.dev_attr.attr,
};

static const struct attribute_group temp_sensor_attr_group = {
    .attrs = custom_temp_sensor_attributes,
};

static const struct attribute_group slot_attr_group = {
    .attrs = slot_attributes,
};

static const struct attribute_group slot_root_attr_group = {
    .attrs = slot_root_attributes,
};

static int __init slot_init(void)
{
    int ret = ERROR_SUCCESS;
    int slot_index = 0;
    int lm75_index = 0;
    int lm75_abs_index = 0;
    board_static_data *bd = NULL;
    char temp_str[128] = {0};
    board_static_data *mbd = bsp_get_board_data();

    INIT_PRINT("slot module init started\n");
    kobj_slot_root = kobject_create_and_add("slot", kobj_switch);
    if (!kobj_slot_root) {
        DBG_ECHO(DEBUG_ERR, "kobj_slot_root create falled!\n");          
        ret = -ENOMEM;       
        goto exit;     
    }

    if (sysfs_create_group(kobj_slot_root, &def_attr_group) != 0) {
        DBG_ECHO(DEBUG_INFO, "create fan default attr faild.\n");
        ret = -ENOSYS;
        goto exit;
    }

    CHECK_IF_ERROR_GOTO_EXIT(ret=sysfs_create_group(kobj_slot_root, &slot_root_attr_group), "create slot_num attribute failed!");

    memset(slot_info, 0, sizeof(slot_info));
    memset(lm75_info, 0, sizeof(lm75_info));

    for (slot_index = 0; slot_index < mbd->slot_num; slot_index++) {
        sprintf(temp_str, "slot%d", slot_index + 1);
        slot_info[slot_index].slot_index = slot_index;
         
        CHECK_IF_ERROR_GOTO_EXIT(kobject_init_and_add(&(slot_info[slot_index].kobj_slot),   &static_kobj_ktype, kobj_slot_root, temp_str), "slot kobj init failed for slot index %d", slot_index);
        CHECK_IF_ERROR_GOTO_EXIT(sysfs_create_group(&(slot_info[slot_index].kobj_slot), &slot_attr_group), "create slot_attr_group failed for slot index %d", slot_index);

        bd = bsp_get_slot_data(slot_index);
        if (bd->initialized != TRUE)
            continue;
        
        for (lm75_index = 0; lm75_index < bd->lm75_num; lm75_index++) {
            lm75_abs_index = slot_index * MAX_LM75_NUM_PER_SLOT + lm75_index;
            sprintf(temp_str, "temp%d", lm75_index + 1);
            slot_info[slot_index].lm75_num++;

            CHECK_IF_ERROR_GOTO_EXIT(kobject_init_and_add(&(lm75_info[lm75_abs_index].kobj_lm75),   &static_kobj_ktype, &slot_info[slot_index].kobj_slot, temp_str), "slot lm75 %d kobj init failed for slot index %d", lm75_index, slot_index);
            
            lm75_info[lm75_abs_index].lm75_index = lm75_index;
            lm75_info[lm75_abs_index].slot_index = slot_index;
            lm75_info[lm75_abs_index].temp_alias = bd->lm75_describe[lm75_index];
            lm75_info[lm75_abs_index].temp_type = "LM75";
            
            CHECK_IF_ERROR_GOTO_EXIT(sysfs_create_group(&(lm75_info[lm75_abs_index].kobj_lm75), &temp_sensor_attr_group), "create temp_sensor_attr_group failed for slot index %d lm75_index %d", slot_index, lm75_index);               
        }

    }
             
exit:
    if (ret != 0) {
        DBG_ECHO(DEBUG_ERR, "slot module init failed!\n");
        release_all_slot_kobj();
    } else {
        INIT_PRINT("slot module finished and success!");
    }
    
    return ret;
}

void release_slot_kobj(int slot_index)
{
    int i = 0;
    int lm75_abs_index = 0;
    
    for (i = 0; i < MAX_LM75_NUM_PER_SLOT; i++) {
        lm75_abs_index = slot_index * MAX_LM75_NUM_PER_SLOT + i;
        if (lm75_info[lm75_abs_index].kobj_lm75.state_initialized)
            kobject_put(&(lm75_info[lm75_abs_index].kobj_lm75));
    }
    
    DBG_ECHO(DEBUG_INFO, "slot index %d lm75 kobject released!\n", slot_index);
    if (slot_info[slot_index].kobj_slot.state_initialized)
        kobject_put(&(slot_info[slot_index].kobj_slot));
    
    DBG_ECHO(DEBUG_INFO, "slot index %d slot released!\n", slot_index);
    return;
}

void release_all_slot_kobj()
{
    int i;
    board_static_data *bd = bsp_get_board_data();
    
    for (i = 0; i < bd->slot_num; i++) {
        release_slot_kobj(i);
    }
    
    if ((kobj_slot_root != NULL) && 
        (kobj_slot_root->state_initialized)) {
        sysfs_remove_group (kobj_slot_root, &def_attr_group);
        kobject_put(kobj_slot_root);
    }

    return;
}

static void __exit slot_exit(void)
{
    release_all_slot_kobj();
    INIT_PRINT("module slot uninstalled !\n");
}

module_init(slot_init);
module_exit(slot_exit);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system slot driver");
MODULE_LICENSE("Dual BSD/GPL");
