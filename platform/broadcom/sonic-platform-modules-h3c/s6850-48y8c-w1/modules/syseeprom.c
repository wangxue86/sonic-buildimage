#include "bsp_base.h"

#define MODULE_NAME "syseeprom"
#define DECODE_NAME_MAX                 20
#define TLV_VALUE_MAX_LEN               ONIE_TLV_VALUE_MAX_LEN
#define TLV_DECODE_VALUE_MAX_LEN        ((5 * TLV_VALUE_MAX_LEN) + 1)
#define TLV_INFO_ID_STRING              "TlvInfo"
#define TLV_INFO_VERSION                0x01
#define TLV_TOTAL_LEN_MAX               (SYS_EEPROM_SIZE - sizeof(tlvinfo_header_t))
#define CONFIG_SYS_EEPROM_SIZE          2048
#define SYS_EEPROM_SIZE                 CONFIG_SYS_EEPROM_SIZE

#define TLV_CODE_PRODUCT_NAME   0x21
#define TLV_CODE_PART_NUMBER    0x22
#define TLV_CODE_SERIAL_NUMBER  0x23
#define TLV_CODE_MAC_BASE       0x24
#define TLV_CODE_MANUF_DATE     0x25
#define TLV_CODE_DEVICE_VERSION 0x26
#define TLV_CODE_LABEL_REVISION 0x27
#define TLV_CODE_PLATFORM_NAME  0x28
#define TLV_CODE_ONIE_VERSION   0x29
#define TLV_CODE_MAC_SIZE       0x2A
#define TLV_CODE_MANUF_NAME     0x2B
#define TLV_CODE_MANUF_COUNTRY  0x2C
#define TLV_CODE_VENDOR_NAME    0x2D
#define TLV_CODE_DIAG_VERSION   0x2E
#define TLV_CODE_SERVICE_TAG    0x2F
#define TLV_CODE_VENDOR_EXT     0xFD
#define TLV_CODE_CRC_32         0xFE

struct __attribute__ ((__packed__))  tlvinfo_header_s {
    char    signature[8];   /* 0x00 - 0x07 EEPROM Tag "TlvInfo" */
    char      version;  /* 0x08        Structure version */
    unsigned short int    totallen; /* 0x09 - 0x0A Length of all data which follows */
};

typedef struct tlvinfo_header_s tlvinfo_header_t;

struct __attribute__ ((__packed__)) tlvinfo_tlv_s {
    unsigned  char type;
    u_int8_t length;
    unsigned  char  value[0];
};

typedef struct tlvinfo_tlv_s tlvinfo_tlv_t;
uint32_t *global_crc32_table = NULL;
uint32_t global_crc_table_buffer[256] = {0};

int  onie_decode_tlv_type(const unsigned char * , const int ,unsigned int , char * );
int  decode_tlv_value(tlvinfo_tlv_t * , char* );
int  onie_validate_crc(const unsigned char * , unsigned short int );
unsigned long crc32 (unsigned long , const unsigned char *, unsigned );
uint32_t  crc32_block_endian0(uint32_t , const void *, unsigned , uint32_t *);
uint32_t*  crc32_filltable(uint32_t *, int );
int bsp_syseeprom_read_eeprom_binary(unsigned int);

static int loglevel = DEBUG_INFO | DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt,##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)

static struct kobject *kobj_syseeprom = NULL;
u8 eeprom_raw[CONFIG_SYS_EEPROM_SIZE] = {0};

enum EEPROM_ATTR {
    PRODUCT_NAME,
    PART_NUMBER,
    SERIAL_NUMBER,
    MANUFACTURE_DATE,
    DEVICE_VERSION,
    BASE_MAC_ADDRESS,
    LABEL_REVISION,
    PLATFORM_NAME,
    ONIE_VERSION,
    NUM_MACS,
    MANUFACTURER,
    MANUFACTURE_COUNTRY,
    VENDOR_NAME,
    DIAG_VERSION,
    SERVICE_TAG,
    VENDOR_EXTENSION,
    CRC_32,
    RAW_DATA,
    BSP_VERSION
};

static inline bool is_valid_tlvinfo_header(tlvinfo_header_t *hdr)
{
    int max_size = TLV_TOTAL_LEN_MAX;
    return((strcmp(hdr->signature, TLV_INFO_ID_STRING) == 0) && (hdr->version == TLV_INFO_VERSION) && (be16_to_cpu(hdr->totallen) <= max_size) );
}

bool is_valid_tlv(tlvinfo_tlv_t *tlv)
{
    return((tlv->type != 0x00) && (tlv->type != 0xFF));
}

int bsp_syseeprom_write_protect_set(int enable_disable_flag)
{
    u8  set_value = 0; 
    board_static_data * bdata = bsp_get_board_data();
    
    switch(enable_disable_flag) {
    case ENABLE:
        set_value = 0x3; break;
    case DISABLE:
        set_value = 0x0; break;
    default:
        return ERROR_FAILED;
    }
    
    return bsp_cpld_write_part(set_value, bdata->cpld_addr_eeprom_write_protect, bdata->cpld_mask_eeprom_write_protect, bdata->cpld_offs_eeprom_write_protect);
}

int bsp_syseeprom_write_buf(u8 * buf, size_t count)
{
    int i = 0;
    int ret = 0;
    u8 temp = 0;
    board_static_data * bdata = bsp_get_board_data();
    
    if (count > bdata->eeprom_used_size) {
        DBG_ECHO(DEBUG_ERR, "%d bytes larger than eeprom used size %d, abort!", (int)count, (int)bdata->eeprom_used_size);
        return ERROR_FAILED;
    }
    
    bsp_syseeprom_write_protect_set(DISABLE);
    if (lock_i2c_path(I2C_DEV_EEPROM) == ERROR_SUCCESS) {
        for (i = 0; i < bdata->eeprom_used_size ; i++) {
            temp = (i < count) ? buf[i] : 0;
            ret += bsp_i2c_24LC128_eeprom_write_byte(bdata->i2c_addr_eeprom, i, temp);
        }
    }
    unlock_i2c_path();
 
    DBG_ECHO(DEBUG_INFO, "eeprom write called ret=%d, %d bytes writed.", ret, (int)count);
    return ret;
}

int  onie_decode_tlv_type(const unsigned char *eeprom_binary, 
                          const int binary_length, 
                          unsigned int tlv_type, 
                          char *value)
{
    tlvinfo_tlv_t *eeprom_tlv;
    int tlv_end;
    int curr_tlv;
    tlvinfo_header_t *eeprom_hdr = (tlvinfo_header_t *)eeprom_binary;
    int checksum_crc = onie_validate_crc(eeprom_binary, eeprom_hdr->totallen);
    
    if(checksum_crc == ERROR_FAILED) {
		printk("Checksum_crc is %s.\n","ERROR_FAILED");
		return -1;
	}
    
    curr_tlv = sizeof(tlvinfo_header_t);
    tlv_end = sizeof(tlvinfo_header_t) + be16_to_cpu(eeprom_hdr->totallen);
    
    while(curr_tlv < tlv_end) {
        eeprom_tlv = (tlvinfo_tlv_t *) &eeprom_binary[curr_tlv];
        if (!is_valid_tlv(eeprom_tlv)) {
            printk("Invalid TLV field starting at EEPROM offset %d\n", curr_tlv);
            return -1;
        }
        
        if(eeprom_tlv->type == tlv_type) {
            int i=decode_tlv_value(eeprom_tlv, value);
            return i;
        } else {
            curr_tlv += sizeof(tlvinfo_tlv_t) + eeprom_tlv->length;
        }
    } 
    
    return -1;
}

int decode_tlv_value(tlvinfo_tlv_t *tlv, char *value)
{
    int i;
    
    switch (tlv->type) {
    case TLV_CODE_PRODUCT_NAME:
    case TLV_CODE_PART_NUMBER:
    case TLV_CODE_SERIAL_NUMBER:
    case TLV_CODE_MANUF_DATE:
    case TLV_CODE_LABEL_REVISION:
    case TLV_CODE_PLATFORM_NAME:
    case TLV_CODE_ONIE_VERSION:
    case TLV_CODE_MANUF_NAME:
    case TLV_CODE_MANUF_COUNTRY:
    case TLV_CODE_VENDOR_NAME:
    case TLV_CODE_DIAG_VERSION:
    case TLV_CODE_SERVICE_TAG:
	case TLV_CODE_VENDOR_EXT:
        memcpy(value, tlv->value, tlv->length);
        value[tlv->length] = 0;
        break;
    case TLV_CODE_MAC_BASE:
        sprintf(value, "%02X:%02X:%02X:%02X:%02X:%02X", 
                tlv->value[0], 
                tlv->value[1], 
                tlv->value[2],
                tlv->value[3], 
                tlv->value[4], 
                tlv->value[5]);
        break;
    case TLV_CODE_DEVICE_VERSION:
        sprintf(value, "%u", tlv->value[0]);
        break;
    case TLV_CODE_MAC_SIZE:
        sprintf(value, "%u", (tlv->value[0] << 8) | tlv->value[1]);
        break;
    case TLV_CODE_CRC_32:
        sprintf(value, "0x%02X%02X%02X%02X",
                tlv->value[0], tlv->value[1], 
                tlv->value[2],
                tlv->value[3]);
		break;
    default:
        value[0] = 0;
        
        for (i = 0; (i < (TLV_DECODE_VALUE_MAX_LEN/5)) && (i < tlv->length);i++) {
            sprintf(value, "%s 0x%02X", value, tlv->value[i]);
        }
		break;
    }
    
	return tlv->length;
}

int onie_validate_crc (const unsigned char *eeprom_binary, unsigned short int binary_length) 
{        
    tlvinfo_header_t *eeprom_hdr = (tlvinfo_header_t *)eeprom_binary;
    tlvinfo_tlv_t    *eeprom_crc;
    unsigned int       calc_crc;
    unsigned int       stored_crc;
    
    if (!is_valid_tlvinfo_header(eeprom_hdr))
        return ERROR_FAILED;
    
    eeprom_crc = (tlvinfo_tlv_t *) &eeprom_binary[sizeof(tlvinfo_header_t) + be16_to_cpu(binary_length) - (sizeof(tlvinfo_tlv_t) + 4)];
    if ((eeprom_crc->type != TLV_CODE_CRC_32) || 
        (eeprom_crc->length != 4)) {
        return  ERROR_FAILED;
    }  
        
    calc_crc = crc32(0, (void *)eeprom_binary, sizeof(tlvinfo_header_t) + be16_to_cpu(binary_length) - 4);
    stored_crc = ((eeprom_crc->value[0] << 24) | 
                  (eeprom_crc->value[1] << 16) | 
                  (eeprom_crc->value[2] <<  8) | 
                  eeprom_crc->value[3]);
    
    if(calc_crc == stored_crc) {
        return ERROR_SUCCESS;
    } else {
        return ERROR_FAILED;
    }
}

unsigned long crc32 (unsigned long crc, const unsigned char *buf, unsigned len)
{
    if (!global_crc32_table) {
        global_crc32_table = crc32_filltable(NULL, 0);
    }
    
    return crc32_block_endian0( crc ^ 0xffffffffL, buf, len, global_crc32_table) ^ 0xffffffffL;
}

uint32_t  crc32_block_endian0(uint32_t val, const void *buf, unsigned len, uint32_t *crc_table)
{
    const void *end = (uint8_t*)buf + len;
    
    while (buf != end) {
        val = crc_table[(uint8_t)val ^ *(uint8_t*)buf] ^ (val >> 8);
        buf = (uint8_t*)buf + 1;
    }
    
    return val;
}

uint32_t *crc32_filltable(uint32_t *crc_table, int endian)
{
    uint32_t polynomial = endian ? 0x04c11db7 : 0xedb88320;
    uint32_t c;
    int i, j;
    
    if (!crc_table)
        crc_table = global_crc_table_buffer;
    
    for (i = 0; i < 256; i++) {
        c = endian ? (i << 24) : i;
        
        for (j = 8; j; j--) {
            if (endian)
                c = (c&0x80000000) ? ((c << 1) ^ polynomial) : (c << 1);
            else
                c = (c&1) ? ((c >> 1) ^ polynomial) : (c >> 1);
        }
        
        *crc_table++ = c;
    }
    
    return crc_table - 256;
}

char *bsp_syseeprom_get_onie_tlv (unsigned char tlv_type, char *tlv_info_string)
{
    int len  = 0;
    u8 *buf_data = eeprom_raw;
    
	if (buf_data[0] == 0) {
	    board_static_data *bdata = bsp_get_board_data();
        len = bdata->eeprom_used_size;
    
        if (lock_i2c_path(I2C_DEV_EEPROM) == ERROR_SUCCESS) {
            bsp_i2c_24LC128_eeprom_read_bytes(bdata->i2c_addr_eeprom, 0, len, buf_data);
        }
        unlock_i2c_path();
    }
	
    if(onie_decode_tlv_type(buf_data, len, tlv_type, tlv_info_string) == -1) {
        strcpy(tlv_info_string, "Not found tlv in eeprom");
        return tlv_info_string;
    }
    
	if(onie_decode_tlv_type(buf_data, len, tlv_type, tlv_info_string) == 0) {
		strcpy(tlv_info_string, "\0");
        return tlv_info_string;	
	}
    
    return tlv_info_string;
}

ssize_t bsp_syseeprom_sysfs_read(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;
    char tlv_info_string[TLV_DECODE_VALUE_MAX_LEN]={0};
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    board_static_data *bdata = bsp_get_board_data();
    
    switch(attr->index) {
    case PRODUCT_NAME:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_PRODUCT_NAME, tlv_info_string));
        break;
    case PART_NUMBER:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_PART_NUMBER, tlv_info_string));
        break;
    case MANUFACTURE_DATE:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_MANUF_DATE, tlv_info_string));
        break;
    case DEVICE_VERSION:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_DEVICE_VERSION, tlv_info_string));
        break;
    case BASE_MAC_ADDRESS:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_MAC_BASE, tlv_info_string));
        break;
    case LABEL_REVISION:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_LABEL_REVISION, tlv_info_string));
        break;
    case PLATFORM_NAME:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_PLATFORM_NAME, tlv_info_string));
        break;
    case ONIE_VERSION:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_ONIE_VERSION, tlv_info_string));
        break;
    case MANUFACTURER:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_MANUF_NAME, tlv_info_string));
        break;
    case MANUFACTURE_COUNTRY:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_MANUF_COUNTRY, tlv_info_string));
        break;
    case VENDOR_NAME:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_VENDOR_NAME, tlv_info_string));
        break;
    case DIAG_VERSION:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_DIAG_VERSION, tlv_info_string));
        break;
    case SERVICE_TAG:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_SERVICE_TAG, tlv_info_string));
        break;
    case VENDOR_EXTENSION:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_VENDOR_EXT, tlv_info_string));
        break;
    case NUM_MACS:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_MAC_SIZE, tlv_info_string));
        break;
    case SERIAL_NUMBER:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_SERIAL_NUMBER, tlv_info_string));
        break;
    case CRC_32:
        len = sprintf(buf, "%s\n", bsp_syseeprom_get_onie_tlv(TLV_CODE_CRC_32, tlv_info_string));
        break;
    case RAW_DATA:
        len = bdata->eeprom_used_size;
        if (lock_i2c_path(I2C_DEV_EEPROM) == ERROR_SUCCESS)
        {
            bsp_i2c_24LC128_eeprom_read_bytes(bdata->i2c_addr_eeprom, 0, len, buf);
        }
        unlock_i2c_path();
        break;
    case BSP_VERSION:
        len = sprintf (buf, "%s\n", WHITEBOX_BSP_VERSION);
        break;
    default:
        len = sprintf(buf, "%s\n", "Not supported.");
    }
    
    return len;
}

ssize_t bsp_syseeprom_sysfs_memory_read(struct kobject *kobjs, struct kobj_attribute *attr, char *buf)
{
    unsigned char temp1[1024] = {0};
    ssize_t len = 0;
    board_static_data * bdata = bsp_get_board_data();

    if (lock_i2c_path(I2C_DEV_EEPROM) == ERROR_SUCCESS) {
        bsp_i2c_24LC128_eeprom_read_bytes(bdata->i2c_addr_eeprom, 0, bdata->eeprom_used_size, temp1);
        len += bsp_print_memory(temp1, bdata->eeprom_used_size, buf + len, 4096 - len, 0x0, 4);
    }
    unlock_i2c_path();
   
    return len;
}

ssize_t  bsp_syseeprom_sysfs_memory_write(struct kobject *kobjs, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    
	ret = bsp_syseeprom_write_buf((u8 *)buf, count);
    if (ERROR_SUCCESS != ret)
    {
        DBG_ECHO(DEBUG_ERR, "eeprom sysfs write failed, ret=%d", ret);
    }
    
    return count;
}

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static ssize_t bsp_default_debug_help (char *buf)
{
    ssize_t index = 0;

    index += sprintf (buf + index, "%s", " Read system eeprom:\n");
    index += sprintf (buf + index, "%s", "   you can run 'i2c_read.py' get help.\n\n");
    index += sprintf (buf + index, "%s", "   eg:\n");
    index += sprintf (buf + index, "%s", "      root@sonic:/home/admin# i2c_read.py 0x182 0x50 0x02 0x0 200\n");
    index += sprintf (buf + index, "%s", "      Read dev id 0x182 address 0x50 from 0x0 length 0xc8\n");
    index += sprintf (buf + index, "%s", "      0000: 54 6c 76 49 6e 66 6f 00  01 00 c4 24 06 00 01 02  * TlvInfo....$.... *\n");
    index += sprintf (buf + index, "%s", "      0010: 03 04 05 29 15 6d 61 73  74 65 72 2d 31 32 31 31  * ...).master-1211 *\n");
    index += sprintf (buf + index, "%s", "      0020: 30 33 35 32 2d 64 69 72  74 79 28 1c 78 38 36 5f  * 0352-dirty(.x86_ *\n");
    index += sprintf (buf + index, "%s", "      0030: 36 34 2d 68 33 63 5f 73  36 38 35 30 2d 34 38 79  * 64-h3c_s6850-48y *\n");
    index += sprintf (buf + index, "%s", "      0040: 38 63 2d 77 31 2d 72 30  23 14 32 31 30 32 33 35  * 8c-w1-r0#.210235 *\n");
    index += sprintf (buf + index, "%s", "      0050: 41 33 58 36 32 30 32 31  30 32 30 34 30 31 21 0e  * A3X62021020401!. *\n");
    index += sprintf (buf + index, "%s", "      0060: 53 36 38 35 30 2d 34 38  59 38 43 2d 57 31 22 0c  * S6850-48Y8C-W1\". *\n");
    index += sprintf (buf + index, "%s", "      0070: 31 32 30 46 30 32 33 35  41 33 58 36 2b 03 48 33  * 120F0235A3X6+.H3 *\n");
    index += sprintf (buf + index, "%s", "      0080: 43 25 13 30 33 2f 30 32  2f 32 30 32 31 20 31 37  * C%.03/02/2021 17 *\n");
    index += sprintf (buf + index, "%s", "      0090: 3a 33 32 3a 32 37 2c 03  43 48 4e 26 01 64 2d 03  * :32:27,.CHN&.d-. *\n");
    index += sprintf (buf + index, "%s", "      00a0: 48 33 43 2e 08 30 2e 31  2e 30 2e 31 36 2f 0b 77  * H3C..0.1.0.16/.w *\n");
    index += sprintf (buf + index, "%s", "      00b0: 77 77 2e 68 33 63 2e 63  6f 6d 27 03 52 30 31 2a  * ww.h3c.com'.R01* *\n");
    index += sprintf (buf + index, "%s", "      00c0: 02 00 05 fd 04 44 45 4d                           * .....DEM         *\n");

    return index;
}

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
    return bsp_default_debug_help(buf);
}

static ssize_t bsp_default_debug_store (struct kobject *kobj, struct kobj_attribute *attr, const char* buf, size_t count)
{
	return count;
}
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

SENSOR_DEVICE_ATTR(product_name, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, PRODUCT_NAME);
SENSOR_DEVICE_ATTR(part_number, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, PART_NUMBER);
SENSOR_DEVICE_ATTR(serial_number, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, SERIAL_NUMBER);
SENSOR_DEVICE_ATTR(base_MAC_address, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, BASE_MAC_ADDRESS);
SENSOR_DEVICE_ATTR(manufacture_date, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, MANUFACTURE_DATE);
SENSOR_DEVICE_ATTR(device_version, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, DEVICE_VERSION);
SENSOR_DEVICE_ATTR(label_revision, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, LABEL_REVISION);
SENSOR_DEVICE_ATTR(platform_name, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, PLATFORM_NAME);
SENSOR_DEVICE_ATTR(ONIE_version, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, ONIE_VERSION);
SENSOR_DEVICE_ATTR(num_MACs, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, NUM_MACS);
SENSOR_DEVICE_ATTR(manufacturer, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, MANUFACTURER);
SENSOR_DEVICE_ATTR(manufacture_country, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, MANUFACTURE_COUNTRY);
SENSOR_DEVICE_ATTR(vendor_name, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, VENDOR_NAME);
SENSOR_DEVICE_ATTR(diag_version, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, DIAG_VERSION);
SENSOR_DEVICE_ATTR(service_tag, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, SERVICE_TAG);
SENSOR_DEVICE_ATTR(vendor_extension, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, VENDOR_EXTENSION);
SENSOR_DEVICE_ATTR(crc_32, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, CRC_32);
SENSOR_DEVICE_ATTR(eeprom, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, RAW_DATA);
SENSOR_DEVICE_ATTR(bsp_version, S_IRUGO, bsp_syseeprom_sysfs_read, NULL, BSP_VERSION);
SYSFS_RW_ATTR_DEF(eeprom, bsp_syseeprom_sysfs_memory_read, bsp_syseeprom_sysfs_memory_write);

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct kobj_attribute loglevel_att =
    __ATTR(loglevel, S_IRUGO | S_IWUSR, bsp_default_loglevel_show, bsp_default_loglevel_store);

static struct kobj_attribute debug_att =
    __ATTR(debug, S_IRUGO | S_IWUSR, bsp_default_debug_show, bsp_default_debug_store);
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *syseeprom_attributes[] = {
    &sensor_dev_attr_product_name.dev_attr.attr,
    &sensor_dev_attr_part_number.dev_attr.attr,
    &sensor_dev_attr_serial_number.dev_attr.attr,
    &sensor_dev_attr_manufacture_date.dev_attr.attr,
    &sensor_dev_attr_base_MAC_address.dev_attr.attr,
    &sensor_dev_attr_device_version.dev_attr.attr,
    &sensor_dev_attr_label_revision.dev_attr.attr,
    &sensor_dev_attr_platform_name.dev_attr.attr,
    &sensor_dev_attr_ONIE_version.dev_attr.attr,
    &sensor_dev_attr_num_MACs.dev_attr.attr,
    &sensor_dev_attr_manufacturer.dev_attr.attr,
    &sensor_dev_attr_manufacture_country.dev_attr.attr,
    &sensor_dev_attr_vendor_name.dev_attr.attr,
    &sensor_dev_attr_diag_version.dev_attr.attr,
    &sensor_dev_attr_service_tag.dev_attr.attr,
    &sensor_dev_attr_vendor_extension.dev_attr.attr,
    &sensor_dev_attr_crc_32.dev_attr.attr,
    &sensor_dev_attr_eeprom.dev_attr.attr,
    &sensor_dev_attr_bsp_version.dev_attr.attr,
    NULL
};

static const struct attribute_group syseeprom_group = {
    .attrs = syseeprom_attributes,
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

void init_platform_name(void)
{
    board_static_data *bd = bsp_get_board_data();
    
    (void)bsp_syseeprom_get_onie_tlv(TLV_CODE_PLATFORM_NAME, bd->onie_platform_name);
    DBG_ECHO(DEBUG_INFO, "Current platform from syseeprom is '%s'", bd->onie_platform_name);   
}

static int __init syseeprom_init(void)
{
    int ret = ERROR_SUCCESS;

    kobj_syseeprom= kobject_create_and_add("syseeprom", kobj_switch);
    if (!kobj_syseeprom) {
        DBG_ECHO(DEBUG_ERR, "kobj_switch create falled!\n");          
        ret = -ENOMEM;       
        goto exit;     
    }

    if (sysfs_create_group(kobj_syseeprom, &def_attr_group) != 0) {
        DBG_ECHO(DEBUG_INFO, "create fan default attr faild.\n");
        ret = -ENOSYS;
        goto exit;
    }
    
    CHECK_CREATE_SYSFS_FILE(kobj_debug, eeprom, ret);
    ret = sysfs_create_group(kobj_syseeprom, &syseeprom_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create group failed");

    init_platform_name();
    
exit:
    if (ret != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "module init failed! result=%d\n", ret);
        if (kobj_syseeprom != NULL)
            kobject_put(kobj_syseeprom);
    } else {
        INIT_PRINT("module finished and success!");
    }
 
    return ret;
}

static void __exit syseeprom_exit(void)
{
    if (kobj_syseeprom != NULL) {
        sysfs_remove_group (kobj_syseeprom, &def_attr_group);
        kobject_put(kobj_syseeprom);
        kobj_syseeprom = NULL;
    }
    
    sysfs_remove_file(kobj_debug, &(eeprom.attr));
    INIT_PRINT("module syseeprom uninstalled !\n");
}

module_init(syseeprom_init);
module_exit(syseeprom_exit);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system eeprom driver");
MODULE_LICENSE("Dual BSD/GPL");
