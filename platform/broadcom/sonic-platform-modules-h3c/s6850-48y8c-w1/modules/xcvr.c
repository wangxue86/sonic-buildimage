#include "bsp_base.h"
#include "static_ktype.h"

void release_all_sfp_kobj(void);
void release_slot_sfp_kobj(int slot_index);

#define MODULE_NAME "xcvr"

static int loglevel = DEBUG_INFO | DEBUG_ERR;
#define DBG_ECHO(level, fmt, args...) DEBUG_PRINT(loglevel, level, fmt,##args)
#define INIT_PRINT(fmt, args...) DEBUG_PRINT(loglevel, DEBUG_INFO, fmt, ##args)

#define SFP_EEPROM_READ_BYTES    256    
#define SUB_PORT_CFG_PATH        "/usr/share/sonic/device/%s/sub_port_split.cfg"
#define PORT_CONFIG_SEG          "[PORT_CONFIG]"

#define bit0                     1 << 0
#define bit1                     1 << 1
#define bit2                     1 << 2
#define bit3                     1 << 3
#define bit4                     1 << 4
#define bit5                     1 << 5
#define bit6                     1 << 6
#define bit7                     1 << 7

#define GET_SLOT_XCVR_INFO_START_INDEX(slot_index,start_index)                     ((start_index)=(MAX_OPTIC_PER_SLOT * ((slot_index) + 1)))
#define GET_SLOT_PORT_FROM_XCVR_GLOBAL_INDEX(xcvr_index,slot_index,port_in_slot)   \
    (slot_index)=(xcvr_index / MAX_OPTIC_PER_SLOT - 1 );\
    (port_in_slot)=(xcvr_index % MAX_OPTIC_PER_SLOT);

struct st_xcvr_info {
    unsigned int sfp_index;       
    int slot_index;
    int xcvr_global_index;
    struct kobject kobj_xcvr;
    struct kobject kobj_eeprom;
    struct kobject kobj_dom;
};

typedef struct {
    s16 temperature;
    u16 voltage;
    u16 rxpower;
    u16 rxpower1;
    u16 rxpower2;
    u16 rxpower3;
    u16 rxpower4;
    int tx_dis1;
    int tx_dis2;
    int tx_dis3;
    int tx_dis4;
    u16 txbias;
    u16 txpower;
    u16 txbias1;
    u16 txbias2;
    u16 txbias3;
    u16 txbias4;
    u16 txpower1;
    u16 txpower2;
    u16 txpower3;
    u16 txpower4;
} sfp_info_st; 

typedef struct tag_port_struct {
    int is_valid;
    int cage_index;
    int subport_num;
    char main_port_name[PORT_NAME_LEN];
    char subport_name[MAX_SUB_PORT_NUM][PORT_NAME_LEN];
} port_struct;

enum sfp_sysfs_attributes {
    POWER_ON,
    MODULE_PRESENT,
    TX_FAULT,
    TX_DIS,
    PRE_N,
    RX_LOS,
    RESET,
    LPMODE,
    INTERRUPT,
    TYPE,
    HW_VERSION,
    SERIAL_NUM,
    MANUFACTURE_NAME,
    MODEL_NAME,
    CONNECTOR,
    ENCODING,
    EXT_IDENTIFIER,
    EXT_RATESELECT_COMPLIANCE,
    CABLE_LENGTH,
    NOMINAL_BIT_RATE,
    SPECIFICATION_COMPLIANCE,
    VENDOR_DATE,
    VENDOR_OUI,
    VOLTAGE,
    TEMPERATURE,
    TXBIAS,
    RXPOWER,
    TXPOWER,
    TXBIAS1,
    RXPOWER1,
    TXPOWER1,
    TXBIAS2,
    RXPOWER2,
    TXPOWER2,
    TXBIAS3,
    RXPOWER3,
    TXPOWER3,
    TXBIAS4,
    RXPOWER4,
    TXPOWER4,
    RAW, 
    DOM_RAW,  
    INFO_BULK,
    TX_DISABLE1,      
    TX_DISABLE2,
    TX_DISABLE3, 
    TX_DISABLE4, 
};
    
typedef struct {
    int signal;
    char name[100];
}sfp_info_analy_st;

sfp_info_analy_st sfp_info_connector_8636[] = {
    {0x00,           "Unknown or unspecified"},
    {0x01,                               "SC"},
    {0x02,      "FC Style 1 copper connector"},
    {0x03,      "FC Style 2 copper connector"},
    {0x04,                          "BNC/TNC"},
    {0x05,                  "FC coax headers"},
    {0x06,                        "Fiberjack"},
    {0x07,                               "LC"},
    {0x08,                            "MT-RJ"},
    {0x09,                               "MU"},
    {0x0a,                              "SG"},
    {0x0b,                 "Optical Pigtail"},
    {0x0c,                          "MPOx12"},
    {0x0d,                          "MPOx16"},
    {0x20,                        "HSSDC II"},
    {0x21,                  "Copper pigtail"},
    {0x22,                            "RJ45"},
    {0x23,          "No separable connector"},
};

sfp_info_analy_st sfp_info_encoding_codes_8636[]= {
    {0,                      "Unspecified"},
    {1,                            "8B10B"},
    {2,                             "4B5B"},
    {3,                              "NRZ"},
    {4,                  "SONET Scrambled"},
    {5,                           "64B66B"},
    {6,                       "Manchester"},
    {7,                         "256B257B"},
};

sfp_info_analy_st sfp_info_ext_identifier_8636[] = {
    {0x00,                                       "Power Class 1(1.5W max)"},
    {0x04,                    "Power Class 1(1.5W max), CDR present in Tx"},
    {0x08,                    "Power Class 1(1.5W max), CDR present in Rx"},
    {0x0c,                 "Power Class 1(1.5W max), CDR present in Rx Tx"},
    {0x10,                         "Power Class 1(1.5W max), CLEI present"},
    {0x14,      "Power Class 1(1.5W max), CLEI present, CDR present in Tx"},
    {0x18,      "Power Class 1(1.5W max), CLEI present, CDR present in Rx"},
    {0x1c,   "Power Class 1(1.5W max), CLEI present, CDR present in Rx Tx"},
    {0x40,                                       "Power Class 2(2.0W max)"},
    {0x44,                    "Power Class 2(2.0W max), CDR present in Rx"},
    {0x48,                    "Power Class 2(2.0W max), CDR present in Tx"},
    {0x4c,                 "Power Class 2(2.0W max), CDR present in Rx Tx"},
    {0x50,                         "Power Class 2(2.0W max), CLEI present"},
    {0x54,      "Power Class 2(2.0W max), CLEI present, CDR present in Rx"},
    {0x58,      "Power Class 2(2.0W max), CLEI present, CDR present in Tx"},
    {0x5c,   "Power Class 2(2.0W max), CLEI present, CDR present in Rx Tx"},
    {0x80,                                       "Power Class 3(2.5W max)"},
    {0x84,                    "Power Class 3(2.5W max), CDR present in Rx"},
    {0x88,                    "Power Class 3(2.5W max), CDR present in Tx"},
    {0x8c,                 "Power Class 3(2.5W max), CDR present in Rx Tx"},
    {0x90,                         "Power Class 3(2.5W max), CLEI present"},
    {0x94,      "Power Class 3(2.5W max), CLEI present, CDR present in Rx"},
    {0x98,      "Power Class 3(2.5W max), CLEI present, CDR present in Tx"},
    {0x9c,   "Power Class 3(2.5W max), CLEI present, CDR present in Rx Tx"},
    {0x0c,                                       "Power Class 4(3.5W max)"},
    {0xc4,                    "Power Class 4(3.5W max), CDR present in Rx"},
    {0xc8,                    "Power Class 4(3.5W max), CDR present in Tx"},
    {0xcc,                 "Power Class 4(3.5W max), CDR present in Rx Tx"},
    {0xd0,                         "Power Class 4(3.5W max), CLEI present"},
    {0xd4,      "Power Class 4(3.5W max), CLEI present, CDR present in Rx"},
    {0xd8,      "Power Class 4(3.5W max), CLEI present, CDR present in Tx"},
    {0xdc,   "Power Class 4(3.5W max), CLEI present, CDR present in Rx Tx"},
};

sfp_info_analy_st sfp_info_type[] = {
    {0x01,                                                    "GBIC"},
    {0x02,                "Module/connector soldered to motherboard"},
    {0x03,                                          "SFP/SFP+/SFP28"},
    {0x04,                                             "300 pin XBI"},
    {0x05,                                                  "XENPAK"},
    {0x06,                                                     "XFP"},
    {0x07,                                                     "XFF"},
    {0x08,                                                   "XFP-E"},
    {0x09,                                                    "XPAK"},
    {0x0a,                                                      "X2"},
    {0x0b,                                           "DWDM-SFP/SFP+"},
    {0x0c,                                                    "QSFP"},
    {0x0d,                                          "QSFP+ or later"},
    {0x0e,                                            "CXP or later"},
    {0x0f,                           "Shielded Mini Multilane HD 4X"},
    {0x10,                           "Shielded Mini Multilane HD 8X"},
    {0x11,                                         "QSFP28 or later"},
    {0x12,                               "CXP2 (aka CXP28) or later"},  
    {0x13,                                   "CDFP (Style 1/Style2)"},
    {0x14,              "Shielded Mini Multilane HD 4X Fanout Cable"},
    {0x15,              "Shielded Mini Multilane HD 8X Fanout Cable"},
    {0x16,                                          "CDFP (Style 3)"},   
    {0x17,                                               "microQSFP"},
    {0x18,         "QSFP-DD Double Density 8X Pluggable Transceiver"},
    {0x19,                           "OSFP 8X Pluggable Transceiver"},
    {0x1a,          "SFP-DD Double Density 2X Pluggable Transceiver"},
};

sfp_info_analy_st sfp_info_specification_compliance_3_8636[] = {
    {bit6,                 "10GBase-LRM"},
    {bit5,                 "10GBase-LR"},
    {bit4,                 "10GBase-SR"},
    {bit3,                 "40GBASE-CR4"},
    {bit2,                 "40GBASE-SR4"},
    {bit1,                 "40GBASE-LR4"},
    {bit0,                 "40G Active Cable (XLPPI)"},
};

sfp_info_analy_st sfp_info_specification_compliance_4_8636[] = {
    {bit3,                 "40G OTN (OTU3B/OTU3C)"},
    {bit2,                 "OC 48, long reach"},
    {bit1,                 "OC 48, intermediate reach"},
    {bit0,                 "OC 48 short reach"},
};

sfp_info_analy_st sfp_info_specification_compliance_5_8636[] = {
    {bit5,                 "SAS 6.0G"},
    {bit4,                 "SAS 3.0G"},
};

sfp_info_analy_st sfp_info_specification_compliance_6_8636[] = {
    {bit3,                 "1000BASE-T"},
    {bit2,                 "1000BASE-CX"},
    {bit1,                 "1000BASE-LX"},
    {bit0,                 "1000BASE-SX"},
};

sfp_info_analy_st sfp_info_specification_compliance_7_8636[] = {
    {bit7,                 "Very long distance (V)"},
    {bit6,                 "Short distance (S)"},
    {bit5,                 "Intermediate distance (I)"},
    {bit4,                 "Long distance (L)"},
    {bit3,                 "Medium (M)"},
    {bit1,                 "Longwave laser (LC)"},
    {bit0,                 "Electrical inter-enclosure (EL)"},
    
};

sfp_info_analy_st sfp_info_specification_compliance_8_8636[] = {
    {bit7,                 "Electrical intra-enclosure'"},
    {bit6,                 "Shortwave laser w/o OFC (SN)"},
    {bit5,                 "Shortwave laser w OFC (SL)"},
    {bit4,                 "Longwave Laser (LL)"},
};

sfp_info_analy_st sfp_info_specification_compliance_9_8636[] = {
    {bit7,                 "Twin Axial Pair (TW)"},
    {bit6,                 "Shielded Twisted Pair (TP)"},
    {bit5,                 "Miniature Coax (MI)"},
    {bit4,                 "Video Coax (TV)"},
    {bit3,                 "Multi-mode 62.5m (M6)"},
    {bit2,                 "Multi-mode 50m (M5)"},
    {bit1,                 "Multi-mode 50um (OM3)"},
    {bit0,                 "Single Mode (SM)"},
};

sfp_info_analy_st sfp_info_specification_compliance_10_8636[] = {
    {bit7,                 "1200 Mbytes/Sec"},
    {bit6,                 "800 Mbytes/Sec"},
    {bit5,                 "1600 Mbytes/Sec"},
    {bit4,                 "400 Mbytes/Sec"},
    {bit2,                 "200 Mbytes/Sec"},
    {bit0,                 "100 Mbytes/Sec"},
};

sfp_info_analy_st sfp_info_connector_8472[] = {
    {0,           "Unknown or unspecified"},
    {1,                               "SC"},
    {2,      "FC Style 1 copper connector"},
    {3,      "FC Style 2 copper connector"},
    {4,                          "BNC/TNC"},
    {5,                  "FC coax headers"},
    {6,                        "Fiberjack"},
    {7,                               "LC"},
    {8,                            "MT-RJ"},
    {9,                               "MU"},
    {10,                              "SG"},
    {11,                 "Optical Pigtail"},
    {12,              "MPO Parallel Optic"},
    {20,                         "HSSDCII"},
    {21,                   "CopperPigtail"},
    {22,                            "RJ45"},

};

sfp_info_analy_st sfp_info_encoding_codes_8472[] = {
    {1,                            "8B/10B"},
    {2,                             "4B/5B"},
    {3,                               "NRZ"},
    {4,                        "Manchester"},
    {5,                   "SONET Scrambled"},
    {6,                           "64B/66B"},
};

sfp_info_analy_st sfp_info_ext_identifier_8472[] = {
    {1,                 "GBIC is compliant with MOD_DEF 1"},
    {2,                 "GBIC is compliant with MOD_DEF 2"},
    {3,                 "GBIC is compliant with MOD_DEF 3"},
    {4,         "GBIC/SFP defined by twowire interface ID"},
    {5,                 "GBIC is compliant with MOD_DEF 5"},
    {6,                 "GBIC is compliant with MOD_DEF 6"},
    {7,                 "GBIC is compliant with MOD_DEF 7"},
};

sfp_info_analy_st sfp_info_ext_rateselect_compliance_8472[] = {
    {0,                                                                               "Unspecified"},
    {1,                                       "Defined for SFF-8079 (4/2/1G Rate_Select & AS0/AS1)"},
    {2,                                          "Defined for SFF-8431 (8/4/2G Rx Rate_Select only"},
    {3,                                                                               "Unspecified"},
    {4,                                          "Defined for SFF-8431 (8/4/2G Tx Rate_Select only"},
    {5,                                                                               "Unspecified"},
    {6,                             "Defined for SFF-8431 (8/4/2G Independent Rx & Tx Rate_select)"},
    {7,                                                                               "Unspecified"},
    {8,                "Defined for FC-PI-5 (16/8/4G Rx Rate_select only) High=16G only, Low=8G/4G"},
    {9,                                                                               "Unspecified"},
    {10,    "Defined for FC-PI-5 (16/8/4G Independent Rx, Tx Rate_select) High=16G only, Low=8G/4G"},
};

sfp_info_analy_st sfp_info_specification_compliance_3_10GEthernetComplianceCode_8472[] = {
    {bit7,                 "10G Base-ER"},
    {bit6,                 "10G Base-LRM"},
    {bit5,                 "10G Base-LR"},
    {bit4,                 "10G Base-SR"},
};

sfp_info_analy_st sfp_info_specification_compliance_3_InfinibandComplianceCode_8472[] = {
    {bit3,                 "1X SX"},
    {bit2,                 "1X LX"},
    {bit1,                 "1X Copper Active"},
    {bit0,                 "1X Copper Passive"},
};

sfp_info_analy_st sfp_info_specification_compliance_4_ESCONComplianceCodes_8472[]= {
    {bit7,                 "ESCON MMF, 1310nm LED"},
    {bit6,                 "ESCON SMF, 1310nm Laser"},
};

sfp_info_analy_st sfp_info_specification_compliance_4_SONETComplianceCodes_8472[] = {
    {bit5,                 "OC-192, short reach"},
    {bit4,                 "SONET reach specifier bit 1"},
    {bit3,                 "SONET reach specifier bit 2"},
    {bit2,                 "OC-48, long reach"},
    {bit1,                 "OC-48, intermediate reach"},
    {bit0,                 "OC-48, short reach"},
};

sfp_info_analy_st sfp_info_specification_compliance_5_ESCONComplianceCodes_8472[] = {
    {bit6,                 "OC-12, single mode, long reach"},
    {bit5,                 "OC-12, single mode, inter reach"},
    {bit4,                 "OC-12, short reach"},
    {bit2,                 "OC-3, single mode, long reach"},
    {bit1,                 "OC-3, single mode, inter reach"},
    {bit0,                 "OC-3, short reach"},
};

sfp_info_analy_st sfp_info_specification_compliance_6_EthernetComplianceCodes_8472[] = {
    {bit7,                 "BASE-PX"},
    {bit6,                 "BASE-BX10"},
    {bit5,                 "100BASE-FX"},
    {bit4,                 "100BASE-LX/LX10"},
    {bit3,                 "1000BASE-T"},
    {bit2,                 "1000BASE-CX"},
    {bit1,                 "1000BASE-LX"},
    {bit0,                 "1000BASE-SX"},
};

sfp_info_analy_st sfp_info_specification_compliance_7_FibreChannelLinkLength_8472[] = {
    {bit7,                 "very long distance (V)"},
    {bit6,                 "short distance (S)"},
    {bit5,                 "Intermediate distance (I)"},
    {bit4,                 "Long distance (L)"},
    {bit3,                 "medium distance (M)"},
};

sfp_info_analy_st sfp_info_specification_compliance_7_FibreChannelTechnology_8472[] = {
    {bit2,                 "Shortwave laser, linear Rx (SA)"},
    {bit1,                 "Longwave Laser (LC)"},
    {bit0,                 "Electrical inter-enclosure (EL)"},
};

sfp_info_analy_st sfp_info_specification_compliance_8_FibreChannelTechnology_8472[] = {
    {bit7,                 "Electrical intra-enclosure (EL)"},
    {bit6,                 "Shortwave laser w/o OFC (SN)"},
    {bit5,                 "Shortwave laser with OFC (SL)"},
    {bit4,                 "Longwave laser (LL)"},
};
    
sfp_info_analy_st sfp_info_specification_compliance_8_SFPCableTechnology_8472[] = {
    {bit3,                 "Active Cable"},
    {bit2,                 "Passive Cable"},
};

sfp_info_analy_st sfp_info_specification_compliance_9_FibreChannelTransmissionMedia_8472[] = {
    {bit7,                 "Twin Axial Pair (TW)"},
    {bit6,                 "Twisted Pair (TP)"},
    {bit5,                 "Miniature Coax (MI)"},
    {bit4,                 "Video Coax (TV)"},
    {bit3,                 "Multimode, 62.5um (M6)"},
    {bit2,                 "Multimode, 50um (M5, M5E)"},
    {bit0,                 "Single Mode (SM)"},
};

sfp_info_analy_st sfp_info_specification_compliance_10_FibreChannelSpeed_8472[]= {
    {bit7,                 "1200 MBytes/sec"},
    {bit6,                 "800 MBytes/sec"},
    {bit5,                 "1600 MBytes/sec"},
    {bit4,                 "400 MBytes/sec"},
    {bit2,                 "200 MBytes/sec"},
    {bit0,                 "100 MBytes/sec"},
};

port_struct splitable_port_info[MAX_OPTIC_COUNT] = {{0}};
static sfp_info_st sfp_info[MAX_OPTIC_COUNT];
static struct kobject *kobj_xcvr_root = NULL;
static struct st_xcvr_info xcvr_info[MAX_OPTIC_COUNT];
int all_port_power_on = 0;

static short __inline__ uchar2short (unsigned char *byte)
{
    return (byte[0] << 8) | byte [1];
}

int bsp_sfp_get_value_8636(int command, int slot_index, int sfp_index, u8* value)
{
    int ret = ERROR_SUCCESS;
	board_static_data *bd = bsp_get_slot_data(slot_index);
    
	ret = lock_i2c_path(GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index) + sfp_index);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "lock i2c path slot index %d sfp index %d failed", slot_index, sfp_index);

    switch(command) {
    case TX_DIS:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TX_DISABLE_8636,1, value);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case TEMPERATURE:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TEMPERATURE_8636,2, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case VOLTAGE:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_VOLTAGE_8636,2, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case RXPOWER:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_RX_POWER_8636,8, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case TXBIAS:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TX_BIAS_8636,8, value) ;
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case TXPOWER:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TX_POWER_8636,8, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case MANUFACTURE_NAME:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_MANUFACTURE_NAME_8636,16, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);    
        break;
    case MODEL_NAME:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_MODEL_NAME_8636,16, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);    
        break;   
    case SERIAL_NUM:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_SERIAL_NUM_8636,16, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case CABLE_LENGTH:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_CABLE_LENGTH_8636,6, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case HW_VERSION:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_HW_VERSION_8636,2, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break;
    case INFO_BULK:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_INFO_BULK_8636,20, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break; 
    case VENDOR_DATE:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_VENDOR_DATE_8636,8, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break; 
    case VENDOR_OUI:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_VENDOR_OUI_8636,3, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break; 
    case CONNECTOR:
        ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_CONNECTOR_8636,1, value); 
        CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
        break; 
    default:
        DBG_ECHO(DEBUG_ERR, "unknown command %d", command);
        ret = ERROR_FAILED;
        break;
    }

exit: 
     unlock_i2c_path();
     return ret;
}

static int bsp_sfp_set_tx_disable_8636(int slot_index, int sfp_index, u8 temp_value)
{
    int ret = ERROR_SUCCESS;
	board_static_data *bd = bsp_get_slot_data(slot_index);
    
	if (bd->initialized != 1) {
	    DBG_ECHO(DEBUG_ERR, "slot index %d data not initialized!", slot_index);
	    ret = ERROR_FAILED;
		goto exit;
	}
    
	if (bd->cage_type[sfp_index] != CAGE_TYPE_QSFP) {
	    DBG_ECHO(DEBUG_ERR, "slot index %d sfp index %d cage not QSFP!", slot_index, sfp_index);
	    ret = ERROR_FAILED;
		goto exit;
	}
    
	if (lock_i2c_path(GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index) + sfp_index) == ERROR_SUCCESS) {
    	ret = bsp_i2c_SFP_write_byte(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TX_DISABLE_8636, temp_value);
        DBG_ECHO(DEBUG_DBG, "slot index %d sfp index %d addr=0x%x temp_value=0x%x!", slot_index, sfp_index, bd->i2c_addr_optic_eeprom_dom[sfp_index], temp_value);

        if (ret != ERROR_SUCCESS) {
    	    DBG_ECHO(DEBUG_ERR, "slot index %d sfp index %d i2c tx disable failed!", slot_index, sfp_index);
    	}
	}
    unlock_i2c_path();
    
exit: 
     return ret;
}


int bsp_sfp_get_value_8472(int command, int slot_index, int sfp_index, char *value)
{
    int ret = ERROR_SUCCESS;
	board_static_data *bd = bsp_get_slot_data(slot_index);
    
	ret = lock_i2c_path(GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index) + sfp_index);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "lock i2c path slot index %d sfp index %d failed!", slot_index, sfp_index);

    switch(command) {
    case TEMPERATURE:
	   ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TEMPERATURE_8472,2, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
    case VOLTAGE:
	   ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_VOLTAGE_8472,2, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
     case RXPOWER:
	   ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_RX_POWER_8472,2, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
    case TXBIAS:
	   ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TX_BIAS_8472,2, value) ;
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
    case TXPOWER:
	   ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[sfp_index], REG_ADDR_TX_POWER_8472,2, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
    case MANUFACTURE_NAME:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_MANUFACTURE_NAME_8472,16, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);    
       break;
    case MODEL_NAME:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_MODEL_NAME_8472,16, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);    
       break;
    case SERIAL_NUM:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_SERIAL_NUM_8472,16, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
    case CONNECTOR:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_CONNECTOR_8472,1, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
    case HW_VERSION:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_HW_VERSION_8472,4, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break;
    case INFO_BULK:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_INFO_BULK_8472,20, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break; 
    case VENDOR_DATE:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_VENDOR_DATE_8472,8, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break; 
    case VENDOR_OUI:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_VENDOR_OUI_8472,3, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_sfp_get_value! command=%d", command);
       break; 
    case CABLE_LENGTH:
       ret = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[sfp_index], REG_ADDR_CABLE_LENGTH_8472,6, value); 
       CHECK_IF_ERROR_GOTO_EXIT(ret, "Failed for bsp_qsfp_get_value! command=%d", command);
       break;
    default:
       DBG_ECHO(DEBUG_ERR, "unknown command %d", command);
       ret = ERROR_FAILED;
       break;
    }

exit: 
     unlock_i2c_path();
     return ret;
}

static int bsp_get_cage_power_on(int slot_index, int port_index, OUT u8 *power_on)
{
    int ret = ERROR_SUCCESS;  
    board_static_data *bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_cage_power_on, "cage power_on not avaliable on slot index %d", slot_index);
  
    if (slot_index == MAIN_BOARD_SLOT_INDEX) {
        CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_get_bit(bd->cpld_addr_cage_power_on,  bd->cpld_offs_cage_power_on, power_on), "get cage power_on state for port %d failed",  port_index + 1);
    } else {
       CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_cage_power_on,  bd->cpld_offs_cage_power_on, power_on), "get slot index %d cage power_on state for port %d failed", slot_index, port_index + 1);
    }
    
exit:    
    return ret;
}

static int bsp_set_cage_power_on(int slot_index, int port_index, u8 power_on)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_slot_data(slot_index);    
    u8 power_on_bit = (power_on != 0);
    
    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_cage_power_on, "cage power_on not avaliable on slot index %d", slot_index);

    if (slot_index == MAIN_BOARD_SLOT_INDEX) {
        CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_set_bit(bd->cpld_addr_cage_power_on,bd->cpld_offs_cage_power_on, power_on_bit),"set cage power_on state %d for port %d failed", power_on_bit, port_index + 1);   
    } else {
        CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_cage_power_on,bd->cpld_offs_cage_power_on, power_on_bit),"set slot index %d cage power_on state %d for port %d failed",slot_index, power_on_bit, port_index + 1);
    }
    
exit:
    return ret;
}

static int bsp_set_all_port_power_on (u8 power_on)
{
    int port_index = 0;
    int slot_index = 0;
    int rv = 0;
    board_static_data *bd = bsp_get_board_data();

    for (port_index = 0; port_index < bd->optic_modlue_num; port_index++) {
        rv = bsp_set_cage_power_on (MAIN_BOARD_SLOT_INDEX, port_index, power_on);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "set mainboard port index %d power on %d failed!", port_index, power_on);
        } else {
            break;
        }
    }
    
    for (slot_index = 0; slot_index < bd->slot_num; slot_index++) {
        for (port_index = 0; port_index < bd->sub_slot_info[slot_index]->optic_modlue_num; port_index++) {
            if ((bd->sub_slot_info[slot_index]->initialized) && 
                (bsp_set_cage_power_on(slot_index, port_index, power_on) != ERROR_SUCCESS)) {
                DBG_ECHO(DEBUG_ERR, "set mainboard port index %d power on %d failed!", port_index, power_on);
            } else {
                break;
            }
        }
    }
    
    all_port_power_on = (int)power_on;
    return ERROR_SUCCESS;
}

static int bsp_get_optical_present(int slot_index, int port_index, OUT u8 *present)
{
    int ret = ERROR_SUCCESS;
    u8 absent = 0;
    u8 slot_absent = 0;
    u8 power_on = 0;
    board_static_data *bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_get_cage_power_on(slot_index, port_index, &power_on), "slot index %d cage %d power on status get failed, abort to get present", slot_index, port_index + 1);
    CHECK_IF_ERROR_GOTO_EXIT(ret=bsp_cpld_get_slot_absent(&slot_absent, slot_index), "get slot index %d absent failed!", slot_index);

    if ((power_on == 0) || 
        (slot_absent == 1)) {
        DBG_ECHO(DEBUG_DBG, "module %d present unknown for power off or slot %d is absent", port_index + 1, slot_index + 1);
        *present = 0;
        ret = ERROR_SUCCESS;
        goto exit;
    }

    if (bd->cage_type[port_index] == CAGE_TYPE_SFP) {
        CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_sfp_present[port_index], "slot index %d sfp index %d present reg is not defined!", slot_index, port_index);

    ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
           bsp_cpld_get_bit(bd->cpld_addr_sfp_present[port_index], bd->cpld_offs_sfp_present[port_index], &absent) : 
           bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_sfp_present[port_index], bd->cpld_offs_sfp_present[port_index], &absent);
    } else if (bd->cage_type[port_index] == CAGE_TYPE_QSFP) {
        CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_qsfp_present[port_index], "slot index %d sfp index %d present reg is not defined!", slot_index, port_index);

        ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
               bsp_cpld_get_bit(bd->cpld_addr_qsfp_present[port_index], bd->cpld_offs_qsfp_present[port_index], &absent) :
               bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_qsfp_present[port_index], bd->cpld_offs_qsfp_present[port_index], &absent);
    } else {
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_ERR, "error cage type %d for port index %d", bd->cage_type[port_index], port_index + 1);
    }
    
    *present = !absent;
    
 exit:
    return ret;
}

static int bsp_get_optical_tx_fault (int slot_index, int port_index, OUT u8 *tx_fault)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_sfp_tx_fault[port_index], "slot index %d sfp index %d tx_fault reg is not defined!", slot_index, port_index);

    if ((port_index >= 0) && 
        (port_index < bd->optic_modlue_num) && 
        (bd->cage_type[port_index] == CAGE_TYPE_SFP)) {
       ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
              bsp_cpld_get_bit(bd->cpld_addr_sfp_tx_fault[port_index], bd->cpld_offs_sfp_tx_fault[port_index] , tx_fault) :
              bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_sfp_tx_fault[port_index], bd->cpld_offs_sfp_tx_fault[port_index] , tx_fault);
    } else {
        DBG_ECHO(DEBUG_ERR, "error cage type %d for port index %d", bd->cage_type[port_index], port_index + 1);
        ret = ERROR_FAILED;
        
    }
    
exit:
    return ret;
}

static int bsp_get_optical_rx_los (int slot_index, int port_index, OUT u8 *rx_los)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_sfp_rx_los[port_index], "slot index %d sfp index %d rx_los reg is not defined!", slot_index, port_index);

    if ((port_index >= 0) && 
        (port_index < bd->optic_modlue_num) && 
        (bd->cage_type[port_index] == CAGE_TYPE_SFP)) {
       ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
              bsp_cpld_get_bit(bd->cpld_addr_sfp_rx_los[port_index], bd->cpld_offs_sfp_rx_los[port_index] , rx_los) :
              bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_sfp_rx_los[port_index], bd->cpld_offs_sfp_rx_los[port_index] , rx_los);
    } else {
        DBG_ECHO(DEBUG_ERR, "error cage type %d for port index %d", bd->cage_type[port_index], port_index + 1);
        ret = ERROR_FAILED;
        
    }
    
exit:
    return ret;
}

static int bsp_get_optical_txdis (int slot_index, int port_index, OUT u8 *txdis)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_sfp_tx_dis[port_index], "slot index %d sfp index %d tx_dis reg is not defined!", slot_index, port_index);

    if ((port_index >= 0) && 
        (port_index < bd->optic_modlue_num) && 
        (bd->cage_type[port_index] == CAGE_TYPE_SFP)) {
       ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
              bsp_cpld_get_bit(bd->cpld_addr_sfp_tx_dis[port_index], bd->cpld_offs_sfp_tx_dis[port_index] , txdis) :
              bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_sfp_tx_dis[port_index], bd->cpld_offs_sfp_tx_dis[port_index] , txdis);
    } else {
        DBG_ECHO(DEBUG_ERR, "error cage type %d for port index %d", bd->cage_type[port_index], port_index + 1);
        ret = ERROR_FAILED;
        
    }
    
exit:  
    return ret;
}

static int bsp_set_optical_txdis (int slot_index, int port_index, u8 txdis)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_slot_data(slot_index);
    
    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_sfp_tx_dis[port_index], "slot index %d sfp index %d tx_dis reg is not defined!", slot_index, port_index);

    if ((port_index >= 0) && 
        (port_index < bd->optic_modlue_num) && 
        (bd->cage_type[port_index] == CAGE_TYPE_SFP)) {
       ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
              bsp_cpld_set_bit(bd->cpld_addr_sfp_tx_dis[port_index], bd->cpld_offs_sfp_tx_dis[port_index] , txdis):
              bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_sfp_tx_dis[port_index], bd->cpld_offs_sfp_tx_dis[port_index] , txdis);
    } else {
        DBG_ECHO(DEBUG_ERR, "error cage type %d for port index %d", bd->cage_type[port_index], port_index + 1);
        ret = ERROR_FAILED;
        
    }
    
exit:
    return ret;
}
 
static int bsp_get_optical_reset(int slot_index, int port_index, OUT u8 *reset)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);
    
    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_qsfp_reset[port_index], "slot index %d sfp index %d reset reg is not defined!", slot_index, port_index);

    if (bd->cage_type[port_index] == CAGE_TYPE_QSFP) {
        ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
               bsp_cpld_get_bit(bd->cpld_addr_qsfp_reset[port_index], bd->cpld_offs_qsfp_reset[port_index], reset) :
               bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_qsfp_reset[port_index], bd->cpld_offs_qsfp_reset[port_index], reset);
    } else {
        DBG_ECHO(DEBUG_ERR, "port %d not QSFP, not support reset!", port_index + 1);
        ret = ERROR_FAILED;
    }
    
exit:
    return ret;
}

static int bsp_set_optical_reset(int slot_index, int port_index, u8 reset)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_qsfp_reset[port_index], "slot index %d sfp index %d reset reg is not defined!", slot_index, port_index);

    if (bd->cage_type[port_index] == CAGE_TYPE_QSFP) {
        ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ?
               bsp_cpld_set_bit(bd->cpld_addr_qsfp_reset[port_index], bd->cpld_offs_qsfp_reset[port_index], reset):
               bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_qsfp_reset[port_index], bd->cpld_offs_qsfp_reset[port_index], reset);
    } else {
        DBG_ECHO(DEBUG_ERR, "port %d not QSFP, not support reset!", port_index + 1);
        ret = ERROR_FAILED;
    }
    
exit:
    return ret;
}

static int bsp_set_optical_lpmode (int slot_index, int port_index, u8 lpmode)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_qsfp_lpmode[port_index], "slot index %d sfp index %d lpmode reg is not defined!", slot_index, port_index);

    if (bd->cage_type[port_index] == CAGE_TYPE_QSFP) {
        lpmode = (lpmode != 0) ? 1 : 0;
        
        ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
               bsp_cpld_set_bit(bd->cpld_addr_qsfp_lpmode[port_index], bd->cpld_offs_qsfp_lpmode[port_index], lpmode):
               bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_qsfp_lpmode[port_index], bd->cpld_offs_qsfp_lpmode[port_index], lpmode);
    } else {
        DBG_ECHO(DEBUG_ERR, "port %d not QSFP, not support lpmode!", port_index + 1);
        ret = ERROR_FAILED;
    }
    
exit:
    return ret;
}

static int bsp_get_optical_lpmode(int slot_index, int port_index, OUT u8 *lpmode)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);

    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_qsfp_lpmode[port_index], "slot index %d sfp index %d lpmode reg is not defined!", slot_index, port_index);

    if (bd->cage_type[port_index] == CAGE_TYPE_QSFP) {
        ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ?  
               bsp_cpld_get_bit(bd->cpld_addr_qsfp_lpmode[port_index], bd->cpld_offs_qsfp_lpmode[port_index], lpmode):
               bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_qsfp_lpmode[port_index], bd->cpld_offs_qsfp_lpmode[port_index], lpmode);
    } else {
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_ERR, "cage type %d not support lpmode for port %d", bd->cage_type[port_index], port_index + 1);
    }
    
exit:    
    return ret;
}

static int bsp_get_optical_interrupt(int slot_index, int port_index, OUT u8 *interrupt)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);
    
    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_qsfp_interrupt[port_index], "slot index %d sfp index %d interrupt reg is not defined!", slot_index, port_index);
    
    if (bd->cage_type[port_index] == CAGE_TYPE_QSFP) {
        ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ?  
              bsp_cpld_get_bit(bd->cpld_addr_qsfp_interrupt[port_index], bd->cpld_offs_qsfp_interrupt[port_index], interrupt):
              bsp_slot_cpld_get_bit(slot_index, bd->cpld_addr_qsfp_interrupt[port_index], bd->cpld_offs_qsfp_interrupt[port_index], interrupt);
    } else {
        ret = ERROR_FAILED;
        DBG_ECHO(DEBUG_ERR, "cage type %d not support interrupt for port %d", bd->cage_type[port_index], port_index + 1);
    }
        
 exit:   
    return ret;
}

static int bsp_set_cage_tx_disable (int slot_index, int port_index, u8 tx_dis)
{
    int ret = ERROR_SUCCESS;
    board_static_data *bd = bsp_get_slot_data(slot_index);
    CHECK_IF_ZERO_GOTO_EXIT(ret, bd->cpld_addr_sfp_tx_dis[port_index], "slot index %d sfp index %d tx_dis reg is not defined!", slot_index, port_index);

    if (bd->cage_type[port_index] != CAGE_TYPE_SFP) {
        DBG_ECHO(DEBUG_ERR, "set port %d tx_dis state failed for cage not sfp", port_index + 1);
        return ERROR_FAILED;
    }
    
    ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
           bsp_cpld_set_bit(bd->cpld_addr_sfp_tx_dis[port_index],  bd->cpld_offs_sfp_tx_dis[port_index], (tx_dis != 0)):
           bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_sfp_tx_dis[port_index],  bd->cpld_offs_sfp_tx_dis[port_index], (tx_dis != 0));
exit:
    return ret;
}

static int bsp_get_optic_eeprom_raw(int slot_index, int port_index, u8 * buf, int count)
{
    int rv = 0;
    board_static_data *bd = bsp_get_slot_data(slot_index);

    rv = lock_i2c_path (GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index) + port_index);
    if (rv) {
        unlock_i2c_path();
        return rv;
    }
    
    rv = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[port_index], 0, count, buf);
    if(rv) {
         DBG_ECHO(DEBUG_ERR, "slot index %d read optic eeprom for port %d failed!", slot_index, port_index + 1);
    }
    
    unlock_i2c_path();
    return rv;
}

static int bsp_get_optic_dom_raw(int slot_index, int port_index, u8 * buf, int count)
{
    int rv = 0;
    board_static_data * bd = bsp_get_slot_data(slot_index);
    u8 diag_support_flag = 0;

    rv = lock_i2c_path (GET_I2C_DEV_OPTIC_IDX_START_SLOT(slot_index) + port_index);
    if (rv) {
        unlock_i2c_path();
        return rv;
    }

    if (bd->cage_type[port_index] == CAGE_TYPE_SFP) {
    
        rv = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom[port_index], REG_ADDR_DIAG_SUPPORT_8472, 1, &diag_support_flag);
        CHECK_IF_ERROR_GOTO_EXIT(rv, "get diag supports flag(0x5c) failed for port index %d failed.", port_index);

        if ((diag_support_flag & bit6) == 0) {
            goto exit;
        }
    }
    
    rv = bsp_i2c_SFP_read_bytes(bd->i2c_addr_optic_eeprom_dom[port_index], 0, count, buf);
    if(rv) {
        DBG_ECHO(DEBUG_ERR, "slot index %d read optic dom for port %d failed!",slot_index, port_index + 1);
    }
    
exit:
    unlock_i2c_path();
    return rv;
}

static int bsp_set_ext_phy_reset_reg(int slot_index, int phy_index, int reset_bit)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = bsp_get_slot_data(slot_index);
    
    if (phy_index >= bd->ext_phy_num)
    {
        DBG_ECHO(DEBUG_ERR, "slot index %d phy index %d is invalid! %d phys in this slot!",slot_index, phy_index, bd->ext_phy_num);
        ret = ERROR_FAILED;
        goto exit;
    }
    ret = (slot_index == MAIN_BOARD_SLOT_INDEX) ? 
           bsp_cpld_set_bit(bd->cpld_addr_phy_reset[phy_index], bd->cpld_offs_phy_reset[phy_index], reset_bit):
           bsp_slot_cpld_set_bit(slot_index, bd->cpld_addr_phy_reset[phy_index], bd->cpld_offs_phy_reset[phy_index], reset_bit);
    
    CHECK_IF_ERROR_GOTO_EXIT(ret, "slot index %d phy %d reset failed!", slot_index, phy_index);
    
exit:
   return ret;
}

static int bsp_set_all_ext_phy_reset(int slot_index, int reset_bit)
{
    int ret = ERROR_SUCCESS;
    int i = 0;
    board_static_data * bd = bsp_get_slot_data(slot_index);

    if (bd->ext_phy_num == 0) {
        DBG_ECHO(DEBUG_INFO, "no external phys on slot index %d!", slot_index);
        return ERROR_SUCCESS;
    }
    
    for (i = 0; i < bd->ext_phy_num; i++) {
        ret += bsp_set_ext_phy_reset_reg(slot_index, i, reset_bit);
    }
    
    return ret;
}

static ssize_t bsp_xcvr_sysfs_xcvr_recall(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_xcvr_info * xcvr_ptr = container_of((struct kobject *)kobjs, struct st_xcvr_info, kobj_xcvr);
    int port_index = xcvr_ptr->sfp_index;
    int slot_index = xcvr_ptr->slot_index;
    u8 temp_value = 0;
    int type = PDT_TYPE_BUTT;
    board_static_data *bd = NULL;
    int rv = 0;
    int i = 0;
    u8 present = 0;
    
    switch(attr->index) {
    case POWER_ON:
        if ((struct kobject*)kobjs == kobj_xcvr_root)
        {
            bd = bsp_get_board_data();
            if (!bd) 
                return snprintf (buf, PAGE_SIZE, " %s\n", "N/A");

            rv = bsp_get_product_type (&type);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "get product type return %d is error.\n", rv);
                return snprintf (buf, PAGE_SIZE, " %s\n", "N/A");
            }

            if (PDT_TYPE_S9820_4M_W1 != type) {
                for (i = 0; i < bd->optic_modlue_num; i++) { 
                    rv = bsp_get_optical_present (MAIN_BOARD_SLOT_INDEX, i, &present);
                    if (rv) {
                        len += sprintf (buf + len, "%d", 0);
                        DBG_ECHO(DEBUG_INFO, "get slot %d port index %d optical present return %d is error.\n", 
                                 MAIN_BOARD_SLOT_INDEX, i, rv);
                        continue;
                    }

                    if (!present) {
                        len += sprintf (buf + len, "%d", 0);
                    } else {
                        rv = bsp_get_cage_power_on (MAIN_BOARD_SLOT_INDEX, i, &temp_value);
                        if (rv) {
                            len += sprintf (buf + len, "%d", 0);
                            DBG_ECHO(DEBUG_INFO, "get slot %d port index %d optical power return %d is error.\n", 
                                     MAIN_BOARD_SLOT_INDEX, i, rv);
                            continue;
                        }

                        len += sprintf (buf + len, "%d", temp_value);
                    }
                }
            } 

            len += sprintf (buf + len, "%s", "\n");
        } else if (bsp_get_cage_power_on(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len  = sprintf(buf, "%d\n", temp_value);
        } else {
            len  = sprintf(buf, "error occur for slot %d cage power on!", slot_index + 1);
        }
        break;
    case TX_DIS:
        if (bsp_get_optical_txdis(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len = sprintf(buf, "%d\n", temp_value);
        } else {
            len = sprintf(buf, "error occur for tx dis!");
        }
        break;
    case RX_LOS:
        if (bsp_get_optical_rx_los(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len = sprintf(buf, "%d\n", temp_value);
        } else {
            len = sprintf(buf, "error occur for rx_los!");
        }
        break;
    case TX_FAULT:
        if (bsp_get_optical_tx_fault(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len = sprintf(buf, "%d\n", temp_value);
        } else {
            len = sprintf(buf, "error occur for rx_los!");
        }
        break;
    case PRE_N:
    case MODULE_PRESENT:
        if (bsp_get_optical_present(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len  = sprintf(buf, "%d\n", temp_value);
        } else {
            len  = sprintf(buf, "error occur for module_present port %d!\n", port_index + 1);
        }
        break;
    case RESET:
        if (bsp_get_optical_reset(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len = sprintf(buf, "%d\n", temp_value);
        } else {
            len = sprintf(buf, "error occur for reset port %d!\n", port_index + 1);
        }
        break;
    case LPMODE:
        if (bsp_get_optical_lpmode(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len  = sprintf(buf, "%d\n", temp_value);
        } else {
            len  = sprintf(buf, "error occur for lpmode port %d!\n", port_index + 1);
        }
        break; 
    case INTERRUPT:
        if (bsp_get_optical_interrupt(slot_index, port_index, &temp_value) == ERROR_SUCCESS) {
            len  = sprintf(buf, "%d\n", temp_value);
        } else {
            len  = sprintf(buf, "error occur for interrupt port %d!\n", port_index + 1);
        }
        break; 
    default:
        len  = sprintf(buf, "Not supported. port %d\n", port_index + 1);
        break;
    }

    return len;
}

static ssize_t bsp_xcvr_sysfs_eeprom_recall(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = -EIO;
    int rv = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_xcvr_info * xcvr_ptr = container_of((struct kobject *)kobjs, struct st_xcvr_info, kobj_eeprom);
    int sfp_index = xcvr_ptr->sfp_index;
    int slot_index = xcvr_ptr->slot_index;
    board_static_data * bd = bsp_get_slot_data(slot_index);
    int temp = 0;
    int i = 0;
    u8 tempu8 = 0;
    u8 present = 0;
    u8 data[50] = {0};
    unsigned char connect_types = 0;

    rv = bsp_get_optical_present (slot_index, sfp_index, &present);
    if (rv) {
        DBG_ECHO(DEBUG_ERR, "get slot %d sfp index %d return %d error.\n", slot_index, sfp_index + 1, rv);
        return sprintf(buf, "%s\n", "N/A");
    }

    if (!present) 
        return sprintf(buf, "%s\n", "N/A");
    
    switch(attr->index) {
    case TYPE:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP) {
            rv = bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_86472 %d get CONNECTOR failed!\n", sfp_index + 1);
            } else {         
                temp = sizeof(sfp_info_type) / sizeof(sfp_info_analy_st);
                for( i = 0; i < temp ; i++) {
                    if(data[2] == sfp_info_type[i].signal) {
                        len = sprintf(buf, "%s\n", sfp_info_type[i].name);
                        break;
                    }
                }                  
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TYPE failed!\n", sfp_index + 1);
            } else {
                temp = sizeof(sfp_info_type) / sizeof(sfp_info_analy_st);
                for( i = 0; i < temp ; i++) {
                   if(data[0] == sfp_info_type[i].signal) {
                       len = sprintf(buf, "%s\n", sfp_info_type[i].name);
                       break;
                    }
                }                  
            }
        }
        break;
    case HW_VERSION:
         if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            rv = bsp_sfp_get_value_8472(attr->index, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get HW_VERSION failed!\n", sfp_index + 1);
            } else {
                for (i = 0; i < 4; i++)  {
                    if (data[i] == 0x20) {
                        data[i] = 0;
                    }
                }
                
                len = sprintf(buf, "%s\n", data);     
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(attr->index, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get HW_VERSION failed!\n", sfp_index + 1);
            } else {
                for (i = 0; i < 2; i++) 
                {
                    if (data[i] == 0x20) 
                    {
                        data[i] = 0;
                    }
                }
                
                len = sprintf(buf, "%s\n", data);
            }
        }
        break;
    case RAW:
        rv = bsp_get_cage_power_on (slot_index, sfp_index, &tempu8);
        if (!rv) {
            if (tempu8 == 0) {
                 len = sprintf(buf, "port %d is power off, can't read eeprom\n",sfp_index + 1);
                 break;
            }
                 
            len = SFP_EEPROM_READ_BYTES;
            rv = bsp_get_optic_eeprom_raw(slot_index, sfp_index, buf, len);
            if(rv) {
                len = sprintf(buf, "read optic eeprom for slot %d port %d failed!\n", slot_index + 1, sfp_index + 1);
            }               
        } else {
             len = sprintf(buf, "get port %d power_on state failed!\n",sfp_index + 1); 
        }
        break;
    case CABLE_LENGTH:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {   
            rv = bsp_sfp_get_value_8472(CABLE_LENGTH, slot_index, sfp_index, data) ;
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get CABLE_LENGTH failed!\n", sfp_index + 1);
            } else {
               len = sprintf(buf, "%dm\n", data[0]);
            }            
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(CONNECTOR, slot_index, sfp_index, &connect_types);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get CONNECTOR failed!\n", sfp_index + 1);
            } else {
                rv = bsp_sfp_get_value_8636(CABLE_LENGTH, slot_index, sfp_index, data);
                if (rv) {
                    DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get CABLE_LENGTH failed!\n", sfp_index + 1);
                } else {
                    if (connect_types == 0x23) {
                        len = sprintf(buf + strlen(buf), "%dm(OM4) \n", data[4]);
                    } else {
                        if (data[0] != 0x0) {
                            len = sprintf(buf + strlen(buf), "%dkm(SMF) ", data[0]);
                        }

                        if (data[1] != 0x0) {
                            len = sprintf(buf + strlen(buf) , "%dm(OM3) ", data[1] * 2);
                        }

                        if (data[2] != 0x0) {
                            len = sprintf(buf + strlen(buf) , "%dm(OM2) ", data[2]);
                        }

                        if (data[3] != 0x0) {
                            len = sprintf(buf + strlen(buf) , "%dm(OM1) ", data[3]);
                        }
                        
                        if (data[4] != 0x0 && data[4] != 255) {
                            len = sprintf(buf + strlen(buf) , "%dm(OM4)", data[4] * 2);
                        } else if (data[4] == 255) {
                            len = sprintf(buf + strlen(buf), ">508m");
                        }
                        
                        len = strlen(buf);
                    }
               }
           }     
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case SERIAL_NUM:
    case MODEL_NAME:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            rv = bsp_sfp_get_value_8472(attr->index, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get MANUFACTURE_NAME failed!\n", sfp_index + 1);
            } else {
                for (i = 0; i < 16; i++)  {
                    if (data[i] == 0x20)  {
                        data[i] = 0;
                    }
                }
                
                len = sprintf(buf, "%s\n", data);    
            }
        }
        else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(attr->index, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get VOLTAGE failed!\n", sfp_index + 1);
            } else {
                for (i = 0; i < 16; i++) {
                    if (data[i] == 0x20) {
                        data[i] = 0;
                    }
                }
                
                len = sprintf(buf, "%s\n", data);
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case MANUFACTURE_NAME:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            rv = bsp_sfp_get_value_8472(attr->index, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get MANUFACTURE_NAME failed!\n", sfp_index + 1);
            } else {
                len = sprintf(buf, "%s\n", data);    
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(attr->index, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get VOLTAGE failed!\n", sfp_index + 1);
            } else {
                len = sprintf(buf, "%s\n", data);
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case CONNECTOR:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            rv = bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_86472 %d get CONNECTOR failed!\n", sfp_index + 1);
            } else {         
                  temp = sizeof(sfp_info_connector_8472) / sizeof(sfp_info_analy_st);
                  for( i = 0; i < temp ; i++) {
                      if(data[2] == sfp_info_connector_8472[i].signal) {
                          len = sprintf(buf, "%s\n", sfp_info_connector_8472[i].name);
                          break;
                      }
                  }                  
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636 (INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get CONNECTOR failed!\n", sfp_index + 1);
            } else {
                temp = sizeof(sfp_info_connector_8636) / sizeof(sfp_info_analy_st);
                for(i = 0; i < temp ; i++) {
                    if(data[2] == sfp_info_connector_8636[i].signal) {
                        len = sprintf(buf, "%s\n", sfp_info_connector_8636[i].name);
                        break;
                    }
                }                  
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case ENCODING:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            rv = bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get ENCODING failed!\n", sfp_index + 1);
            } else {
                temp = sizeof(sfp_info_encoding_codes_8472) / sizeof(sfp_info_analy_st);
                for( i = 0; i < temp ; i++) {
                   if(data[11] == sfp_info_encoding_codes_8472[i].signal) {
                       len = sprintf(buf, "%s\n", sfp_info_encoding_codes_8472[i].name);
                       break;
                   }
                }                  
            }
        } else if (bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get ENCODING failed!\n", sfp_index + 1);
            } else { 
                temp = sizeof(sfp_info_encoding_codes_8636) / sizeof(sfp_info_analy_st);
                for( i = 0; i < temp ; i++) {
                    if(data[11] == sfp_info_encoding_codes_8636[i].signal) {
                       len = sprintf(buf, "%s\n", sfp_info_encoding_codes_8636[i].name);
                       break;
                     }
                }                  
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case EXT_IDENTIFIER:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            rv = bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get EXT_IDENTIFIER failed!\n", sfp_index + 1);
            } else {
                temp = sizeof(sfp_info_ext_identifier_8472) / sizeof(sfp_info_analy_st);
                for( i = 0; i < temp ; i++) {
                    if(data[1] == sfp_info_ext_identifier_8472[i].signal) {
                        len = sprintf(buf, "%s\n", sfp_info_ext_identifier_8472[i].name);
                        break;
                    }
                }                  
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get EXT_IDENTIFIER failed!\n", sfp_index + 1);
            }
            else{  
                temp = sizeof(sfp_info_ext_identifier_8636) / sizeof(sfp_info_analy_st);
                for( i = 0; i < temp ; i++){
                   if(data[1] == (sfp_info_ext_identifier_8636)[i].signal) {
                       len = sprintf(buf, "%s\n", (sfp_info_ext_identifier_8636)[i].name);
                       break;
                   }
               }                  
           }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case EXT_RATESELECT_COMPLIANCE:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP) {
            if (bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get EXT_RATESELECT_COMPLIANCE failed!\n", sfp_index + 1);
            }
            else
            {
                  temp = sizeof(sfp_info_ext_rateselect_compliance_8472) / sizeof(sfp_info_analy_st);
                  if(data[13] == sfp_info_ext_rateselect_compliance_8472[i].signal)
                  {
                      len = sprintf(buf, "%s\n", sfp_info_ext_rateselect_compliance_8472[i].name);
                      break;
                  }       
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            if (bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get EXT_IDENTIFIER failed!\n", sfp_index + 1);
            }
            else
            {               
                  if(data[13] == 0)
                  {
                      len = sprintf(buf, "%s\n", "QSFP+ Rate Select Version 1");
                      break;
                  }                
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case NOMINAL_BIT_RATE:
        if (bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            if (bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get EXT_IDENTIFIER failed!\n", sfp_index + 1);
            }
            else
            {
                len = sprintf(buf, "%d\n", data[12]);        
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            if (bsp_sfp_get_value_8636(INFO_BULK, slot_index, sfp_index, data) != ERROR_SUCCESS)
            {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get EXT_IDENTIFIER failed!\n", sfp_index + 1);
            }
            else
            {
                 len = sprintf(buf, "%d\n", data[12]);      
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case SPECIFICATION_COMPLIANCE:
        if (bd->cage_type[sfp_index] == CAGE_TYPE_SFP) {
            rv = bsp_sfp_get_value_8472(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get INFO_BULK failed!\n", sfp_index + 1);
            } else {
                 len = sprintf(buf, "Specification compliance: \n");
                 if (data[3] & 0xf0) {
                     len += sprintf(buf + len, "\t10GEthernetComplianceCode: ");
                     temp = sizeof(sfp_info_specification_compliance_3_10GEthernetComplianceCode_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[3] & sfp_info_specification_compliance_3_10GEthernetComplianceCode_8472[i].signal ) {
                             len += sprintf(buf + strlen(buf), "%s\n", sfp_info_specification_compliance_3_10GEthernetComplianceCode_8472[i].name);
                             break;
                         }
                     }    
                 }
                 
                 if (data[3] & 0x0f) {
                     len += sprintf(buf + len, "\tInfinibandComplianceCode: ");
                     temp = sizeof(sfp_info_specification_compliance_3_InfinibandComplianceCode_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[3] & sfp_info_specification_compliance_3_InfinibandComplianceCode_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n", sfp_info_specification_compliance_3_InfinibandComplianceCode_8472[i].name);
                             break;
                         }
                     }    
                 }
                 
                 if (data[4] & 0xc0) {
                     len += sprintf(buf + len, "\tESCONComplianceCodes: ");
                     temp = sizeof(sfp_info_specification_compliance_4_ESCONComplianceCodes_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[4] & sfp_info_specification_compliance_4_ESCONComplianceCodes_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_4_ESCONComplianceCodes_8472[i].name);
                             break;
                         }
                     }  
                 }
                 
                 if (data[4] & 0x3f) {
                     len += sprintf(buf + len, "\tSONETComplianceCodes: ");
                     temp = sizeof(sfp_info_specification_compliance_4_SONETComplianceCodes_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[4] & sfp_info_specification_compliance_4_SONETComplianceCodes_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_4_SONETComplianceCodes_8472[i].name);
                             break;
                         }
                     }
                 } else if (data[5] & 0x77) {
                     len += sprintf(buf + len, "\tSONETComplianceCodes: ");
                     temp = sizeof(sfp_info_specification_compliance_5_ESCONComplianceCodes_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[5] & sfp_info_specification_compliance_5_ESCONComplianceCodes_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_5_ESCONComplianceCodes_8472[i].name);
                             break;
                         }
                     }
                 }
                    
                 if (data[6] & 0xff) {
                     len += sprintf(buf + len, "\tEthernetComplianceCodes: ");
                     temp = sizeof(sfp_info_specification_compliance_6_EthernetComplianceCodes_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[6] & sfp_info_specification_compliance_6_EthernetComplianceCodes_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_6_EthernetComplianceCodes_8472[i].name);
                             break;
                         }
                     }
                 }

                 if (data[7] & 0xf8) {
                     len += sprintf(buf + len, "\tFibreChannelLinkLength: ");
                     temp = sizeof(sfp_info_specification_compliance_7_FibreChannelLinkLength_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[7] & sfp_info_specification_compliance_7_FibreChannelLinkLength_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_7_FibreChannelLinkLength_8472[i].name);
                             break;
                         }
                     } 
                 }

                 if (data[7] & 0x07) {
                     len += sprintf(buf + len, "\tFibreChannelTechnology: ");
                     temp = sizeof(sfp_info_specification_compliance_7_FibreChannelTechnology_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[7] & sfp_info_specification_compliance_7_FibreChannelTechnology_8472[i].signal ) {
                             len += sprintf(buf + strlen(buf), "%s\n ", sfp_info_specification_compliance_7_FibreChannelTechnology_8472[i].name);
                             break;
                         }
                     } 
                 } else if (data[8] & 0xf0) {
                     len += sprintf(buf + len, "\tFibreChannelTechnology: ");
                     temp = sizeof(sfp_info_specification_compliance_8_FibreChannelTechnology_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[8] & sfp_info_specification_compliance_8_FibreChannelTechnology_8472[i].signal ) {
                             len += sprintf(buf + strlen(buf), "%s\n ", sfp_info_specification_compliance_8_FibreChannelTechnology_8472[i].name);
                             break;
                         }
                     }
                 }

                 if (data[8] & 0x0c) {
                     len += sprintf(buf + len, "\tSFP+CableTechnology: ");
                     temp = sizeof(sfp_info_specification_compliance_8_SFPCableTechnology_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {   
                         if(data[8] & sfp_info_specification_compliance_8_SFPCableTechnology_8472[i].signal ) { 
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_8_SFPCableTechnology_8472[i].name);
                             break;
                         }
                     }
                 }
                
                 if (data[9] & 0xfd) {
                     len += sprintf(buf + len, "\tFibreChannelTransmissionMedia: ");
                     temp = sizeof(sfp_info_specification_compliance_9_FibreChannelTransmissionMedia_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[9] & sfp_info_specification_compliance_9_FibreChannelTransmissionMedia_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_9_FibreChannelTransmissionMedia_8472[i].name);
                             break;
                         }
                     }     
                 }
                 
                 if (data[10] & 0xf5) {
                     len += sprintf(buf + len, "\tFibreChannelSpeed: ");
                     temp = sizeof(sfp_info_specification_compliance_10_FibreChannelSpeed_8472) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[10] & sfp_info_specification_compliance_10_FibreChannelSpeed_8472[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_10_FibreChannelSpeed_8472[i].name);
                             break;
                         }
                     }
                }
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(INFO_BULK, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get SPECIFICATION_COMPLIANCE failed!\n", sfp_index + 1);
            } else {
                 len = sprintf(buf, "Specification compliance: \n");
                 
                 if (data[3] & 0x7f) {
                     len += sprintf(buf + len, "\t10/40G Ethernet Compliance Code: ");
                     temp = sizeof(sfp_info_specification_compliance_3_8636) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[3] & sfp_info_specification_compliance_3_8636[i].signal) {
                             len += sprintf(buf + len, "%s\n", sfp_info_specification_compliance_3_8636[i].name);
                             break;
                         }
                     }  
                 }
                 
                 if (data[4] & 0x0f) {
                     len += sprintf(buf + len, "\tSONET Compliance codes: ");
                     temp = sizeof(sfp_info_specification_compliance_4_8636) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[4] & sfp_info_specification_compliance_4_8636[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_4_8636[i].name);
                             break;
                         }
                     }
                 }
                 
                 if (data[5] & 0x30) {
                     len += sprintf(buf + len, "\tSAS/SATA compliance codes: ");
                     temp = sizeof(sfp_info_specification_compliance_5_8636) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[5] & sfp_info_specification_compliance_5_8636[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_5_8636[i].name);
                             break;
                         }
                     } 
                 }
                 
                 if (data[6] & 0x0f) {
                     len += sprintf(buf + len, "\tGigabit Ethernet Compliant codes: ");
                     temp = sizeof(sfp_info_specification_compliance_6_8636) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[6] & sfp_info_specification_compliance_6_8636[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_6_8636[i].name);
                             break;
                         }
                     } 
                 }
                 
                 if ((data[7] & 0xfb) | 
                    (data[8] & 0xf0)) {
                     len += sprintf(buf + len, "\tFibre Channel link length/Transmitter Technology: ");
                       
                     if(data[7]  & 0xfb) {
                        temp = sizeof(sfp_info_specification_compliance_7_8636) / sizeof(sfp_info_analy_st); 

                        for(i= 0 ; i < temp; i++) {
                            if(data[7] & sfp_info_specification_compliance_7_8636[i].signal ) {
                                len += sprintf(buf + len, "%s ", sfp_info_specification_compliance_7_8636[i].name);
                                break;
                            }
                        }
                     } else if(data[8] & 0xf0) {
                        temp = sizeof(sfp_info_specification_compliance_8_8636) / sizeof(sfp_info_analy_st); 

                        for(i= 0 ; i < temp; i++) {
                            if(data[8] & sfp_info_specification_compliance_8_8636[i].signal ) {
                                len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_8_8636[i].name);
                                break;
                            }
                        }
                     }
                     
                     len += sprintf(buf + len,"\n");  
                 }
                    
                 if (data[9] & 0xff) {
                     len += sprintf(buf + len, "\tFibre Channel transmission media: ");
                     temp = sizeof(sfp_info_specification_compliance_9_8636) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[9] & sfp_info_specification_compliance_9_8636[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_9_8636[i].name);
                             break;
                         }
                     }   
                 }
                 
                 if (data[10] & 0xf5) {
                     
                     len += sprintf(buf + len, "\tFibre Channel Speed: ");
                     temp = sizeof(sfp_info_specification_compliance_10_8636) / sizeof(sfp_info_analy_st); 

                     for(i= 0 ; i < temp; i++) {
                         if(data[10] & sfp_info_specification_compliance_10_8636[i].signal ) {
                             len += sprintf(buf + len, "%s\n ", sfp_info_specification_compliance_10_8636[i].name);
                             break;
                         }
                     }  
                 }
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case VENDOR_DATE:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP) {
            rv = bsp_sfp_get_value_8472(VENDOR_DATE, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get EXT_IDENTIFIER failed!\n", sfp_index + 1);
            } else {
                for(i=0;i<6;i++) {
                    data[i] -= 48;
                }
                
                len = sprintf(buf, "Vendor Date Code(YYYY-MM-DD Lot): 20%d%d-%d%d-%d%d\n", data[0],data[1],data[2],data[3],data[4],data[5]);                   
            }
        }
        else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(VENDOR_DATE, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get EXT_IDENTIFIER failed!\n", sfp_index + 1);
            } else {
                for(i=0;i<6;i++) {
                    data[i] -= 48;
                }
                
                len = sprintf(buf, "Vendor Date Code(YYYY-MM-DD Lot): 20%d%d-%d%d-%d%d\n", data[0],data[1],data[2],data[3],data[4],data[5]);             
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    case VENDOR_OUI:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP ) {
            rv = bsp_sfp_get_value_8472(VENDOR_OUI, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get VENDOR_OUI failed!\n", sfp_index + 1);
            } else {
                len = sprintf(buf, "%x-%x-%x\n", data[0],data[1],data[2]);       
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(VENDOR_OUI, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get VENDOR_OUI failed!\n", sfp_index + 1);
            } else {
                 len = sprintf(buf, "%x-%x-%x\n", data[0],data[1],data[2]);      
            }
        } else {
            len = sprintf(buf + strlen(buf), "%s", "No support\n");
        }
        break;
    default:
        len  = sprintf(buf, "Not supported. port %d\n", sfp_index + 1);
        break;
    }
    
    return len;
}


static ssize_t bsp_xcvr_sysfs_dom_recall(struct device *kobjs, struct device_attribute *da, char *buf)
{
    ssize_t len = -EIO;
    int rv = 0;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_xcvr_info * xcvr_ptr = container_of((struct kobject *)kobjs, struct st_xcvr_info, kobj_dom);
    int sfp_index = xcvr_ptr->sfp_index;
    int slot_index = xcvr_ptr->slot_index;
    u8 data[20] = {0};
    u8 tempu8 = 0;  
    u8 present = 0xff;
    board_static_data * bd = bsp_get_slot_data(slot_index);

    rv = bsp_get_optical_present (slot_index, sfp_index, &present);
    if (rv) {
        DBG_ECHO(DEBUG_ERR, "get slot %d sfp index %d return %d error.\n", slot_index, sfp_index + 1, rv);
        return sprintf(buf, "%s\n", "N/A");
    }

    if (!present) 
        return sprintf(buf, "%s\n", "N/A");

    switch(attr->index) {
    case TX_DISABLE1: 
        if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP)
        {
            rv = bsp_sfp_get_value_8636(TX_DIS, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TX_DISABLE1 failed!\n", sfp_index + 1);             
            } else {
                sfp_info[sfp_index].tx_dis1 = data[0] & 0x01 ;
                len = sprintf(buf, "%d\n", sfp_info[sfp_index].tx_dis1);   
            }
        }
        break;
  	case TX_DISABLE2:
		 if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(TX_DIS, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TX_DISABLE2 failed!\n", sfp_index + 1);
            } else {
                sfp_info[sfp_index].tx_dis2 = (data[0]>>1)  & 0x01 ;
                len = sprintf(buf, "%d\n", sfp_info[sfp_index].tx_dis2); 
            }
         }
         break;
    case TX_DISABLE3:
		 if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(TX_DIS, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TX_DISABLE3 failed!\n", sfp_index + 1);
            } else {
                sfp_info[sfp_index].tx_dis3 = (data[0]>>2)  & 0x01 ;
                len = sprintf(buf, "%d\n", sfp_info[sfp_index].tx_dis3);   
            }
         }
         break;
	case TX_DISABLE4:
	   	 if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(TX_DIS, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TX_DISABLE4 failed!\n", sfp_index + 1);
            } else {
                sfp_info[sfp_index].tx_dis4 = (data[0]>>3)  & 0x01 ;
                len = sprintf(buf, "%d\n", sfp_info[sfp_index].tx_dis4);   
            }
         }
         break;    
    case VOLTAGE:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP) {
            rv = bsp_sfp_get_value_8472(VOLTAGE, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get VOLTAGE failed!\n", sfp_index + 1);
            } else {
                sfp_info[sfp_index].voltage = (unsigned short)uchar2short(data) * 100 / 10000;
                 len = sprintf(buf, "%d.%02d\n", 
                               sfp_info[sfp_index].voltage / 100, 
                               sfp_info[sfp_index].voltage % 100);    
            }
        } else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(VOLTAGE, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get VOLTAGE failed!\n", sfp_index + 1);
            } else {
                sfp_info[sfp_index].voltage = (unsigned short)uchar2short(data) * 100 / 10000;
                len = sprintf(buf, "%d.%02d\n", 
                              sfp_info[sfp_index].voltage / 100, 
                              sfp_info[sfp_index].voltage % 100);
            }
        }
        break;
    case TEMPERATURE:
        if(bd->cage_type[sfp_index] == CAGE_TYPE_SFP)
        {
            rv = bsp_sfp_get_value_8472(TEMPERATURE, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get TEMPERATURE failed!\n", sfp_index + 1);
            }
            else {
                sfp_info[sfp_index].temperature = uchar2short(data) * 100 / 256;
                len = sprintf(buf,"%s%d.%02d\n",
                              sfp_info[sfp_index].temperature > 0 ? "" : "-", (short)(abs(sfp_info[sfp_index].temperature) / 100), 
                              (short)(abs(sfp_info[sfp_index].temperature) % 100));
            }
        }
        else if(bd->cage_type[sfp_index] == CAGE_TYPE_QSFP) {
            rv = bsp_sfp_get_value_8636(TEMPERATURE, slot_index, sfp_index, data);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TEMPERATURE failed!\n", sfp_index + 1);
            } else {
               sfp_info[sfp_index].temperature = uchar2short(data) * 100 / 256;
               len = sprintf(buf,"%s%d.%02d\n",
                             sfp_info[sfp_index].temperature > 0 ? "" : "-", 
                             (short)(abs(sfp_info[sfp_index].temperature) / 100), 
                             (short)(abs(sfp_info[sfp_index].temperature) % 100));
            }
        }
        break;
    case RXPOWER1:
        rv = bsp_sfp_get_value_8636(RXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get RXPOWER_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].rxpower1 = (unsigned short)uchar2short(data) * 100 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].rxpower1 / 100, 
                          sfp_info[sfp_index].rxpower1 % 100);
        }
        break;
    case RXPOWER2:
        rv = bsp_sfp_get_value_8636(RXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get RXPOWER_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].rxpower2 = (unsigned short)uchar2short(data + 2) * 100 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].rxpower2 / 100, 
                          sfp_info[sfp_index].rxpower2 % 100);
        }
        break;
    case RXPOWER3:
        rv = bsp_sfp_get_value_8636(RXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get RXPOWER_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].rxpower3 = (unsigned short)uchar2short(data + 4) * 100 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].rxpower3 / 100, 
                          sfp_info[sfp_index].rxpower3 % 100);
         }
         break;
    case RXPOWER4:
        rv = bsp_sfp_get_value_8636(RXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get RXPOWER_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].rxpower4 = (unsigned short)uchar2short(data + 6) * 100 / 1000;

            len = sprintf(buf, "%d.%02d\n",
                         sfp_info[sfp_index].rxpower4 / 100, 
                         sfp_info[sfp_index].rxpower4 % 100);
        }
        break; 
    case TXBIAS1:
        rv = bsp_sfp_get_value_8636(TXBIAS, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].txbias1 = (unsigned short)uchar2short(data) * 200 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].txbias1 / 100, 
                          sfp_info[sfp_index].txbias1 % 100);
        }
        break;
    case TXBIAS2:
        rv = bsp_sfp_get_value_8636(TXBIAS, slot_index, sfp_index, data);
        if (rv) {
              DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].txbias2 = (unsigned short)uchar2short(data + 2) * 200 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].txbias2 / 100, 
                          sfp_info[sfp_index].txbias2 % 100);
        }
        break;
    case TXBIAS3:
        rv = bsp_sfp_get_value_8636(TXBIAS, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
        }
        else {
            sfp_info[sfp_index].txbias3 = (unsigned short)uchar2short(data + 4) * 200 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].txbias3 / 100, 
                          sfp_info[sfp_index].txbias3 % 100);
        }
        break;
    case TXBIAS4:
        rv = bsp_sfp_get_value_8636(TXBIAS, slot_index, sfp_index, data);
         if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
         } else {
             sfp_info[sfp_index].txbias4 = (unsigned short)uchar2short(data + 6) * 200 / 1000;
             len = sprintf(buf, "%d.%02d\n",
                           sfp_info[sfp_index].txbias4 / 100, 
                           sfp_info[sfp_index].txbias4 % 100);
         }
         break; 
    case TXPOWER1:
        rv = bsp_sfp_get_value_8636(TXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].txpower1 = (unsigned short)uchar2short(data) * 100 / 1000;
             len = sprintf(buf, "%d.%02d\n",
                           sfp_info[sfp_index].txpower1 / 100, 
                           sfp_info[sfp_index].txpower1 % 100);
        }
        break;
    case TXPOWER2:
        rv = bsp_sfp_get_value_8636(TXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].txpower2 = (unsigned short)uchar2short(data + 2) * 100 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].txpower2 / 100, 
                          sfp_info[sfp_index].txpower2 % 100);
        }
        break;
    case TXPOWER3:
        rv = bsp_sfp_get_value_8636 (TXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
        } else {
            sfp_info[sfp_index].txpower3 = (unsigned short)uchar2short(data + 4) * 100 / 1000;
             len = sprintf(buf, "%d.%02d\n",
                           sfp_info[sfp_index].txpower3 / 100, 
                           sfp_info[sfp_index].txpower3 % 100);
        }
        break;
    case TXPOWER4:
        rv = bsp_sfp_get_value_8636(TXPOWER, slot_index, sfp_index, data);
        if (rv) {
             DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
        } else {
             sfp_info[sfp_index].txpower4 = (unsigned short)uchar2short(data + 6) * 100 / 1000;
             len = sprintf(buf, "%d.%02d\n",
                           sfp_info[sfp_index].txpower4 / 100, 
                           sfp_info[sfp_index].txpower4 % 100);
         }
         break;         
    case RXPOWER:
        rv = bsp_sfp_get_value_8472(RXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8472 %d get RXPOWER_8472 failed!\n", sfp_index + 1);
        } else {
           sfp_info[sfp_index].rxpower = (unsigned short)uchar2short(data) * 100 / 1000;
           len = sprintf(buf, "%d.%02d\n",
                         sfp_info[sfp_index].rxpower / 100, 
                         sfp_info[sfp_index].rxpower % 100);
        }
        break;
    case TXBIAS:
        rv = bsp_sfp_get_value_8472(TXBIAS, slot_index, sfp_index, data);
        if (rv) {
             DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXBIAS_8636 failed!\n", sfp_index + 1);
         } else {
             sfp_info[sfp_index].txbias = (unsigned short)uchar2short(data) * 200 / 1000;
             len = sprintf(buf, "%d.%02d\n",
                           sfp_info[sfp_index].txbias / 100, 
                           sfp_info[sfp_index].txbias % 100);
         }
         break; 
    case TXPOWER:
        rv = bsp_sfp_get_value_8472(TXPOWER, slot_index, sfp_index, data);
        if (rv) {
            DBG_ECHO(DEBUG_ERR, "sfp_8636 %d get TXPOWER_8636 failed!\n", sfp_index + 1);
        } else {
            
            sfp_info[sfp_index].txpower = (unsigned short)uchar2short(data) * 100 / 1000;
            len = sprintf(buf, "%d.%02d\n",
                          sfp_info[sfp_index].txpower / 100, 
                          sfp_info[sfp_index].txpower % 100);
        }
        break; 
    case DOM_RAW:
        rv = bsp_get_cage_power_on(slot_index, sfp_index, &tempu8);
        if (rv) {
            len = sprintf(buf, "get port %d power_on state failed!\n",sfp_index + 1);
        } else if (tempu8 == 0) {
            len = sprintf(buf, "port %d is power off, can't read eeprom\n",sfp_index + 1);
            break;
        }
        
        len = SFP_EEPROM_READ_BYTES;

        rv = bsp_get_optic_dom_raw(slot_index, sfp_index, buf, len);
        if(rv) {
            len = sprintf(buf, "read optic dom for slot %d port %d failed!\n", slot_index + 1, sfp_index + 1);
        }
        break;
    default:
        len  = sprintf(buf, "Not supported. %d\n", sfp_index + 1);
        break;
    }
    
    return len;
}

static ssize_t bsp_xcvr_sysfs_extra_dom_set_recall (struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    ssize_t len = count;
    struct kobject * okobjs = (struct kobject*)kobjs;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_xcvr_info * xcvr_ptr = container_of((struct kobject *)kobjs, struct st_xcvr_info, kobj_dom);
    int sfp_index = xcvr_ptr->sfp_index;
    int slot_index = xcvr_ptr->slot_index;
    u8 temp_value = 0;
    int set_value = 0;
    int ret = ERROR_SUCCESS;
    
    ret = bsp_sfp_get_value_8636(TX_DIS, slot_index, sfp_index, &temp_value);
    if (ret) {
        DBG_ECHO(DEBUG_ERR, "sfp 8636 get failed for sfp index %d slot index %d!", sfp_index, slot_index);
        len = -EIO;
        goto exit;
    }
    
    switch(attr->index) {
    case TX_DISABLE1:
        if (sscanf(buf, "%d", &set_value) <= 0) { 
            DBG_ECHO(DEBUG_ERR, "tx disable format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
			len = -EINVAL;
        } else {   
            if(set_value == 0) {
                temp_value &= ~TX1_DISABLE_BIT;
            } else if(set_value == 1) {
                temp_value |= TX1_DISABLE_BIT;
            } else {
                DBG_ECHO(DEBUG_ERR, "set_value = %d, not support for kobjs->name=%s, attr->name=%s", set_value, okobjs->name, attr->dev_attr.attr.name);
				len = -EINVAL;
            }

            ret = bsp_sfp_set_tx_disable_8636(slot_index, sfp_index, temp_value);
            if (ret) {
                DBG_ECHO(DEBUG_ERR, "tx1 disable failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
                len = -EIO;
            } 
        }
        break;
    case TX_DISABLE2:
        if (sscanf(buf, "%d", &set_value) <= 0) {
            DBG_ECHO(DEBUG_ERR, "TX2_DISABLE format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
			len = -EINVAL;
        } else {
            if(set_value == 0) {
                temp_value &= ~TX2_DISABLE_BIT;
            } else if(set_value == 1) {
                temp_value |= TX2_DISABLE_BIT;
            } else {
                DBG_ECHO(DEBUG_ERR, "set_value = %d, not support for kobjs->name=%s, attr->name=%s", set_value, okobjs->name, attr->dev_attr.attr.name);
				len = -EINVAL;
            }   

            ret = bsp_sfp_set_tx_disable_8636(slot_index, sfp_index, temp_value);
            if (ret) {
                DBG_ECHO(DEBUG_ERR, "tx2 disable failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
				len = -EIO;
            } 
        }
        break;
    case TX_DISABLE3:
        if (sscanf(buf, "%d", &set_value) <= 0) {
            DBG_ECHO(DEBUG_ERR, "TX3_DISABLE format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
			len = -EINVAL;
        } else {
            if(set_value == 0) {
                temp_value &= ~TX3_DISABLE_BIT;
            } else if(set_value == 1) {
                temp_value |= TX3_DISABLE_BIT;
            } else {
                DBG_ECHO(DEBUG_ERR, "set_value = %d, not support for kobjs->name=%s, attr->name=%s", set_value, okobjs->name, attr->dev_attr.attr.name);
				len = -EINVAL;
            }

            ret = bsp_sfp_set_tx_disable_8636(slot_index, sfp_index, temp_value);
            if (ret) {
                DBG_ECHO(DEBUG_ERR, "tx3 disable failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
				len = -EIO;
            } 
        }
        break;
    case TX_DISABLE4:
        if (sscanf(buf, "%d", &set_value) <= 0) {
            DBG_ECHO(DEBUG_ERR, "TX4_DISABLE format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
			len = -EINVAL;
        } else {
            if(set_value == 0) {
                temp_value &= ~TX4_DISABLE_BIT;
            } else if(set_value == 1) {
               temp_value |= TX4_DISABLE_BIT;
            } else {
                DBG_ECHO(DEBUG_ERR, "set_value = %d, not support for kobjs->name=%s, attr->name=%s", set_value, okobjs->name, attr->dev_attr.attr.name);
				len = -EINVAL;
            }   

            ret = bsp_sfp_set_tx_disable_8636(slot_index, sfp_index, temp_value);
            if (ret) {
                DBG_ECHO(DEBUG_ERR, "tx4 disable failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
				len = -EIO;
            }
        }
        break;
    default:
         DBG_ECHO(DEBUG_INFO, "not support write attribute %s -> %s", okobjs->name, attr->dev_attr.attr.name);
		 len = -EINVAL;
         break;
     }
exit:
    
    return len;
}

static ssize_t bsp_xcvr_sysfs_set_attr(struct device *kobjs, struct device_attribute *da, const char *buf, size_t count)
{
    ssize_t len = 0;
    struct kobject * okobjs = (struct kobject*)kobjs;
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct st_xcvr_info * xcvr_ptr = container_of((struct kobject *)kobjs, struct st_xcvr_info, kobj_xcvr);
    int sfp_index = xcvr_ptr->sfp_index;
    int slot_index = xcvr_ptr->slot_index;
    int temp = 0;
    int rv = 0;
    
    switch(attr->index) {
    case POWER_ON:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "power on format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
        } else if ((struct kobject*)kobjs == kobj_xcvr_root) {
            rv = bsp_set_all_port_power_on((u8)temp);
            if (rv) {
                DBG_ECHO(DEBUG_ERR, "power on %d all port failed", (int)temp);
            } else {
                DBG_ECHO(DEBUG_INFO, "power on %d all port done!", (int)temp);
            }
        } else if (ERROR_SUCCESS != bsp_set_cage_power_on(slot_index, sfp_index, (u8)temp)) {
            DBG_ECHO(DEBUG_ERR, "set power on slot_index %d port index %d failed!", slot_index, sfp_index);
        }
        break;
    case RESET:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "reset format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
        } else if (ERROR_SUCCESS != bsp_set_optical_reset(slot_index, sfp_index, (u8)temp)) {
            DBG_ECHO(DEBUG_ERR, "reset failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
        }
    case LPMODE:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "reset format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
        } else if (ERROR_SUCCESS != bsp_set_optical_lpmode(slot_index, sfp_index, (u8)temp)) {
            DBG_ECHO(DEBUG_ERR, "reset failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
        }
        break;
    case TX_DIS:
        if (sscanf(buf, "%d", &temp) <= 0) {
            DBG_ECHO(DEBUG_ERR, "tx dis format error '%s' , kobjs->name=%s, attr->name=%s", buf, okobjs->name, attr->dev_attr.attr.name);
        } else if (ERROR_SUCCESS != bsp_set_cage_tx_disable(slot_index, sfp_index, (u8)temp)) {
            DBG_ECHO(DEBUG_ERR, "tx dis set failed for kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
        }
        break;
    default:
        DBG_ECHO(DEBUG_INFO, "not support write attribute %s -> %s", okobjs->name, attr->dev_attr.attr.name);
        break;
    }
    
    len = count;
    
    DBG_ECHO(DEBUG_DBG, "store called kobjs->name=%s, attr->name=%s", okobjs->name, attr->dev_attr.attr.name);
    return count;
}

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static ssize_t bsp_default_debug_help_show (char *buf)
{
    ssize_t index = 0;

    index += sprintf (buf + index, "%s", "  read or write sfp or qsfp present/power_on/tx_dis:\n");
    index += sprintf (buf + index, "%s", "    you can 'cat /sys/switch/cpld/debug' get help.\n\n");
    index += sprintf (buf + index, "%s", "    eg:\n\n");
    index += sprintf (buf + index, "%s", "       root@sonic:/home/admin# cat /sys/switch/debug/cpld/board_cpld\n\n");
    index += sprintf (buf + index, "%s", "       ** !!!! cpld operation is dangerous, which may cause reboot/halt !!!!\n");
    index += sprintf (buf + index, "%s", "       ** use 'echo offset:value' to set register value\n");
    index += sprintf (buf + index, "%s", "       ** offset & value must be hex value, example: 'echo 0x0:0x17 > board_cpld'\n\n");
    index += sprintf (buf + index, "%s", "       hw address start: 0x2200\n");
    index += sprintf (buf + index, "%s", "       0x0000: 08 14 01 d3 10 05 00 14  70 01 01 ff 55 55 55 55\n");
    index += sprintf (buf + index, "%s", "       0x0010: 55 11 05 05 05 ff ff ff  ff ff ff ff ff ff ff ff\n");
    index += sprintf (buf + index, "%s", "       0x0020: ff ff ff 10 10 01 01 ff  ff 0d 0d 0d 0d 0d 0d 0d\n");
    index += sprintf (buf + index, "%s", "       0x0030: 0d 0d 05 01 ff fc fd 2f  01 01 01 01 7e 7f 7f 7f\n");
    index += sprintf (buf + index, "%s", "       0x0040: 7f 7f 7f 7f 7e 7e 7e 7e  02 f8 2f fe 80 81 81 81\n");
    index += sprintf (buf + index, "%s", "       0x0050: 81 81 81 80 17 fe ff ff  ff ff ff ff ff ff ff fe\n");
    index += sprintf (buf + index, "%s", "       0x0060: fe fe fe fe af 42 06 0f  f0 f1 ff 7b f9 fd fd fd\n");
    index += sprintf (buf + index, "%s", "       0x0070: 13 3e 38 08 1f 1f 1f 00  ff ff ff 0d e0 e0 f5 05\n");
    index += sprintf (buf + index, "%s", "       0x0080: 05 05 05 05 05 05 05 05  05 f4 ff ff ff fb ff 0d\n");
    index += sprintf (buf + index, "%s", "       0x0090: 0d 00 00 00 00 00 00 05  05 f4 ff ff ff fb ff 0d\n");
    index += sprintf (buf + index, "%s", "       0x00a0: 0d f4 ff ff ff fb ff ff  ff ff ff ff ff ff ff ff\n");
    index += sprintf (buf + index, "%s", "       0x00b0: ff ff ff ff ff ff ff ff  ff 00 00 00 00 00 00 00\n");
    index += sprintf (buf + index, "%s", "       0x00c0: 00 ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
    index += sprintf (buf + index, "%s", "       0x00d0: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
    index += sprintf (buf + index, "%s", "       0x00e0: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
    index += sprintf (buf + index, "%s", "       0x00f0: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff\n");
    index += sprintf (buf + index, "%s", "  read sfp or qsfp eeprom data:\n");
    index += sprintf (buf + index, "%s", "    you can run 'i2c_read.py' get help.\n\n");
    index += sprintf (buf + index, "%s", "    eg:\n\n");
    index += sprintf (buf + index, "%s", "       root@sonic:/home/admin# i2c_read.py 0x01 0x50 0x2 0x0 0x80\n");
    index += sprintf (buf + index, "%s", "       Read dev id 0x1 address 0x50 from 0x0 length 0x80\n");
    index += sprintf (buf + index, "%s", "       0000: 00 00 00 00 44 40 00 00  03 ff 00 00 00 00 00 0a  * ....D@.......... *\n");
    index += sprintf (buf + index, "%s", "       0010: 07 49 4e 4e 4f 4c 49 47  48 54 20 20 20 20 20 20  * .INNOLIGHT       *\n");
    index += sprintf (buf + index, "%s", "       0020: 20 02 44 7c 7f 54 52 2d  50 59 38 35 53 2d 4e 30  *  .D|.TR-PY85S-N0 *\n");
    index += sprintf (buf + index, "%s", "       0030: 30 2d 48 33 43 31 41 20  20 03 52 00 7b 00 1a 67  * 0-H3C1A  .R.{..g *\n");
    index += sprintf (buf + index, "%s", "       0040: 00 49 4e 48 42 59 30 34  32 30 31 39 30 20 20 20  * .INHBY0420190    *\n");
    index += sprintf (buf + index, "%s", "       0050: 20 31 38 30 32 32 37 20  20 68 f0 08 af 00 00 00  *  180227  h...... *\n");
    index += sprintf (buf + index, "%s", "       0060: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  * ................ *\n");
    index += sprintf (buf + index, "%s", "       0070: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  * ................ *\n");
    index += sprintf (buf + index, "%s", "  write sfp or qsfp i2c data:\n");
    index += sprintf (buf + index, "%s", "    you can run 'cat /sys/switch/debug/i2c/param' get help.\n\n");
    index += sprintf (buf + index, "%s", "    eg:\n\n");
    index += sprintf (buf + index, "%s", "       root@sonic:/home/admin# echo 'path 0x1 addr 0x50 write inner 0x5d value 0x05 mode 0x1'> /sys/switch/debug/i2c/param\n");
    index += sprintf (buf + index, "%s", "       root@sonic:/home/admin# cat /sys/switch/debug/i2c/do_wirte\n"); 
    index += sprintf (buf + index, "%s", "       write dev id 0x1 address 0x50 inner 0x5d value 0x5\n"); 
    index += sprintf (buf + index, "%s", "       success!\n"); 
    
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
    return bsp_default_debug_help_show(buf);
}

static ssize_t bsp_default_debug_store (struct kobject *kobj, struct kobj_attribute *attr, const char* buf, size_t count)
{
	return count;
}

static ssize_t bsp_default_sfp_present_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int rv = 0;
    int i = 0;
    ssize_t index = 0;
    u8 tmp_present = 0;
    int type = PDT_TYPE_BUTT;
    board_static_data *bd = bsp_get_board_data();

    if (!bd) 
        return snprintf (buf, PAGE_SIZE, " %s\n", "N/A");

    rv = bsp_get_product_type (&type);
    if (rv) {
        DBG_ECHO(DEBUG_ERR, "get product type return %d is error.\n", rv);
        return snprintf (buf, PAGE_SIZE, " %s\n", "N/A");
    }

    if (PDT_TYPE_S9820_4M_W1 != type) {
        for (i = 0; i < bd->optic_modlue_num; i++) { 
            rv = bsp_get_optical_present (MAIN_BOARD_SLOT_INDEX, i, &tmp_present);
            if (rv) {
                index += sprintf (buf + index, "%d", 0);
                DBG_ECHO(DEBUG_INFO, "get slot %d port index %d optical present return %d is error.\n", 
                         MAIN_BOARD_SLOT_INDEX, i, rv);
                continue;
            }

            index += sprintf (buf + index, "%d", tmp_present);
        }
    } 

    index += sprintf (buf + index, "%s", "\n");
    return index;
}
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static SENSOR_DEVICE_ATTR(power_on                  , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_xcvr_recall, bsp_xcvr_sysfs_set_attr,  POWER_ON);
static SENSOR_DEVICE_ATTR(module_present            , S_IRUGO, bsp_xcvr_sysfs_xcvr_recall, NULL,  MODULE_PRESENT);
static SENSOR_DEVICE_ATTR(tx_fault                  , S_IRUGO, bsp_xcvr_sysfs_xcvr_recall, NULL, TX_FAULT);
static SENSOR_DEVICE_ATTR(tx_dis                    , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_xcvr_recall, bsp_xcvr_sysfs_set_attr, TX_DIS);
static SENSOR_DEVICE_ATTR(pre_n                     , S_IRUGO, bsp_xcvr_sysfs_xcvr_recall, NULL, PRE_N);
static SENSOR_DEVICE_ATTR(rx_los                    , S_IRUGO, bsp_xcvr_sysfs_xcvr_recall, NULL, RX_LOS);
static SENSOR_DEVICE_ATTR(reset                     , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_xcvr_recall, bsp_xcvr_sysfs_set_attr, RESET);
static SENSOR_DEVICE_ATTR(lpmode                    , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_xcvr_recall, bsp_xcvr_sysfs_set_attr, LPMODE);
static SENSOR_DEVICE_ATTR(interrupt                 , S_IRUGO, bsp_xcvr_sysfs_xcvr_recall, NULL, INTERRUPT);
static SENSOR_DEVICE_ATTR(type                      , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, TYPE);
static SENSOR_DEVICE_ATTR(hw_version                , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, HW_VERSION);
static SENSOR_DEVICE_ATTR(serial_num                , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, SERIAL_NUM);
static SENSOR_DEVICE_ATTR(manufacture_name          , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, MANUFACTURE_NAME);
static SENSOR_DEVICE_ATTR(model_name                , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, MODEL_NAME);
static SENSOR_DEVICE_ATTR(connector                 , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, CONNECTOR);
static SENSOR_DEVICE_ATTR(encoding                  , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, ENCODING);
static SENSOR_DEVICE_ATTR(ext_identifier            , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, EXT_IDENTIFIER);
static SENSOR_DEVICE_ATTR(ext_rateselect_compliance , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, EXT_RATESELECT_COMPLIANCE);
static SENSOR_DEVICE_ATTR(cable_length              , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, CABLE_LENGTH);
static SENSOR_DEVICE_ATTR(nominal_bit_rate          , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, NOMINAL_BIT_RATE);
static SENSOR_DEVICE_ATTR(specification_compliance  , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, SPECIFICATION_COMPLIANCE);
static SENSOR_DEVICE_ATTR(vendor_date               , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, VENDOR_DATE);
static SENSOR_DEVICE_ATTR(vendor_oui                , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, VENDOR_OUI);
static SENSOR_DEVICE_ATTR(voltage                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, VOLTAGE);
static SENSOR_DEVICE_ATTR(temperature               , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TEMPERATURE);
static SENSOR_DEVICE_ATTR(txbias                    , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXBIAS);
static SENSOR_DEVICE_ATTR(rxpower                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, RXPOWER);
static SENSOR_DEVICE_ATTR(txpower                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXPOWER);
static SENSOR_DEVICE_ATTR(txbias1                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXBIAS1);
static SENSOR_DEVICE_ATTR(rxpower1                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, RXPOWER1);
static SENSOR_DEVICE_ATTR(txpower1                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXPOWER1);
static SENSOR_DEVICE_ATTR(txbias2                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXBIAS2);
static SENSOR_DEVICE_ATTR(rxpower2                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, RXPOWER2);
static SENSOR_DEVICE_ATTR(txpower2                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXPOWER2);
static SENSOR_DEVICE_ATTR(txbias3                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXBIAS3);
static SENSOR_DEVICE_ATTR(rxpower3                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, RXPOWER3);
static SENSOR_DEVICE_ATTR(txpower3                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXPOWER3);
static SENSOR_DEVICE_ATTR(txbias4                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXBIAS4);
static SENSOR_DEVICE_ATTR(rxpower4                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, RXPOWER4);
static SENSOR_DEVICE_ATTR(txpower4                  , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, TXPOWER4);
static SENSOR_DEVICE_ATTR(tx_disable1                  , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_dom_recall, bsp_xcvr_sysfs_extra_dom_set_recall, TX_DISABLE1);
static SENSOR_DEVICE_ATTR(tx_disable2                  , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_dom_recall, bsp_xcvr_sysfs_extra_dom_set_recall, TX_DISABLE2);
static SENSOR_DEVICE_ATTR(tx_disable3                  , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_dom_recall, bsp_xcvr_sysfs_extra_dom_set_recall, TX_DISABLE3);
static SENSOR_DEVICE_ATTR(tx_disable4                  , S_IRUGO|S_IWUSR, bsp_xcvr_sysfs_dom_recall, bsp_xcvr_sysfs_extra_dom_set_recall, TX_DISABLE4);
static SENSOR_DEVICE_ATTR(raw                       , S_IRUGO, bsp_xcvr_sysfs_eeprom_recall, NULL, RAW);
static SENSOR_DEVICE_ATTR(dom_raw                   , S_IRUGO, bsp_xcvr_sysfs_dom_recall, NULL, DOM_RAW);

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct kobj_attribute loglevel_att =
    __ATTR(loglevel, S_IRUGO | S_IWUSR, bsp_default_loglevel_show, bsp_default_loglevel_store);

static struct kobj_attribute debug_att =
    __ATTR(debug, S_IRUGO | S_IWUSR, bsp_default_debug_show, bsp_default_debug_store);

static struct kobj_attribute present_att =
    __ATTR(present, S_IRUGO, bsp_default_sfp_present_show, NULL);
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute *sfp100G_txdis_attr[] = {
    &sensor_dev_attr_tx_disable1.dev_attr.attr,
    &sensor_dev_attr_tx_disable2.dev_attr.attr,
    &sensor_dev_attr_tx_disable3.dev_attr.attr,
    &sensor_dev_attr_tx_disable4.dev_attr.attr,
    NULL
};

static struct attribute *all_power_on_attr[] = {
    &sensor_dev_attr_power_on.dev_attr.attr,
    NULL
};

static struct attribute *sfp25G_attributes[] = {
    &sensor_dev_attr_power_on.dev_attr.attr,
    &sensor_dev_attr_tx_fault.dev_attr.attr,
    &sensor_dev_attr_tx_dis.dev_attr.attr,
    &sensor_dev_attr_pre_n.dev_attr.attr,
    &sensor_dev_attr_rx_los.dev_attr.attr,
    NULL
};

static struct attribute *sfp25G_dom_attributes[] = {
    &sensor_dev_attr_voltage.dev_attr.attr,
    &sensor_dev_attr_temperature.dev_attr.attr,
    &sensor_dev_attr_txbias.dev_attr.attr,
    &sensor_dev_attr_rxpower.dev_attr.attr,
    &sensor_dev_attr_txpower.dev_attr.attr,
    &sensor_dev_attr_dom_raw.dev_attr.attr,
    NULL
};

static struct attribute *sfp100G_attributes[] = {
    &sensor_dev_attr_power_on.dev_attr.attr,
    &sensor_dev_attr_module_present.dev_attr.attr,
    &sensor_dev_attr_reset.dev_attr.attr,
    &sensor_dev_attr_lpmode.dev_attr.attr,
    &sensor_dev_attr_interrupt.dev_attr.attr,
    NULL
};

static struct attribute *sfp100G_dom_attributes[] = {
    &sensor_dev_attr_voltage.dev_attr.attr,
    &sensor_dev_attr_temperature.dev_attr.attr,
    &sensor_dev_attr_txbias1.dev_attr.attr,
    &sensor_dev_attr_rxpower1.dev_attr.attr,
    &sensor_dev_attr_txpower1.dev_attr.attr,
    &sensor_dev_attr_txbias2.dev_attr.attr,
    &sensor_dev_attr_rxpower2.dev_attr.attr,
    &sensor_dev_attr_txpower2.dev_attr.attr,
    &sensor_dev_attr_txbias3.dev_attr.attr,
    &sensor_dev_attr_rxpower3.dev_attr.attr,
    &sensor_dev_attr_txpower3.dev_attr.attr,
    &sensor_dev_attr_txbias4.dev_attr.attr,
    &sensor_dev_attr_rxpower4.dev_attr.attr,
    &sensor_dev_attr_txpower4.dev_attr.attr,
    &sensor_dev_attr_dom_raw.dev_attr.attr,
    NULL
};

static struct attribute *eeprom_attributes[] = {
    &sensor_dev_attr_type.dev_attr.attr,
    &sensor_dev_attr_hw_version.dev_attr.attr,
    &sensor_dev_attr_serial_num.dev_attr.attr,
    &sensor_dev_attr_manufacture_name.dev_attr.attr,
    &sensor_dev_attr_model_name.dev_attr.attr,
    &sensor_dev_attr_connector.dev_attr.attr,
    &sensor_dev_attr_encoding.dev_attr.attr,
    &sensor_dev_attr_ext_identifier.dev_attr.attr,
    &sensor_dev_attr_ext_rateselect_compliance.dev_attr.attr,
    &sensor_dev_attr_cable_length.dev_attr.attr,
    &sensor_dev_attr_nominal_bit_rate.dev_attr.attr,
    &sensor_dev_attr_specification_compliance.dev_attr.attr,
    &sensor_dev_attr_vendor_date.dev_attr.attr,
    &sensor_dev_attr_vendor_oui.dev_attr.attr,
    &sensor_dev_attr_raw.dev_attr.attr,
    NULL
};

/*Bgein: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/
static struct attribute *def_attrs[] = {
    &loglevel_att.attr,
    &debug_att.attr,
    &present_att.attr,
    NULL,
};

static struct attribute_group def_attr_group = {
    .attrs = def_attrs,
};
/*End: add by z10865 <zhai.guangcheng@h3c.com> for switch default attr*/

static struct attribute_group all_power_on_group = {
    .attrs = all_power_on_attr,
};

static struct attribute_group sfp25G_group = {
    .attrs = sfp25G_attributes,
};

static struct attribute_group sfp100G_group = {
    .attrs = sfp100G_attributes,
};

static struct attribute_group eeprom_group = {
    .attrs = eeprom_attributes,
};

static struct attribute_group sfp25G_dom_group = {
    .attrs = sfp25G_dom_attributes,
};

static struct attribute_group sfp100G_dom_group = {
    .attrs = sfp100G_dom_attributes,
};

static struct attribute_group sfp100G_dom_txdis_group = {
    .attrs = sfp100G_txdis_attr,
};

int bsp_xcvr_get_split_portname(char *src, char symbol, char dest[MAX_SUB_PORT_NUM][PORT_NAME_LEN])
{
    int len = 0;
    int i = 0;
    int start = 0;
    int found_num = 0;

    len = strlen(src) + 1;
    for (i = 0; i < len; i++) {
        if ((src[i] == symbol) || 
            (src[i] == '\0')) {
            strncpy(dest[found_num], src + start, i - start);
            found_num++;
            start= i + 1;
        }
    }

    return found_num;
}

int bsp_xcvr_get_port_info_per_line(char *file_data_src)
{
    int ret = ERROR_SUCCESS;
    int i = 0, j = 0;
    char line[128];
    char tmp_sub_port_names[PORT_NAME_LEN];
    char tmp_main_port_name[PORT_NAME_LEN];
    int  port_index = 0;
    int  sub_port_num = 0;
    char *file_data = file_data_src;
    char *port_config_start = NULL;
        
    if (NULL == file_data) { 
        DBG_ECHO (DEBUG_ERR, "invalid file data");
        return ERROR_FAILED;
    }

    memset (line, '\0', sizeof(line));
    memset (tmp_main_port_name, '\0', sizeof(tmp_main_port_name));

    if ((port_config_start = strstr(file_data, PORT_CONFIG_SEG)) == NULL) {
        DBG_ECHO (DEBUG_ERR, "Could't find %s segment, failed to read port config file!", PORT_CONFIG_SEG);
        return ERROR_FAILED;
    }

    file_data = port_config_start;
    while (*file_data++ != '\n');
    
    while (*file_data != '\0') {
        line[i] = *file_data;
        if (*file_data == '\n') {
            if (strchr(line, '#') != NULL) {
                
            } else {
                sscanf(line, "%s %d %d %s", tmp_main_port_name, &port_index, &sub_port_num, tmp_sub_port_names);
                
                strcpy(splitable_port_info[port_index].main_port_name, tmp_main_port_name); 
                splitable_port_info[port_index].cage_index = port_index;
                splitable_port_info[port_index].subport_num = sub_port_num;
                splitable_port_info[port_index].is_valid = 1;

                bsp_xcvr_get_split_portname(tmp_sub_port_names, ',', splitable_port_info[port_index].subport_name);

                DBG_ECHO(DEBUG_INFO, "main=%s index=%d subport_num=%d", splitable_port_info[port_index].main_port_name, splitable_port_info[port_index].cage_index, splitable_port_info[port_index].subport_num);
                for (j = 0; j < sub_port_num; j++) {
                    DBG_ECHO(DEBUG_INFO, "        subport[%d]=%s", j, splitable_port_info[port_index].subport_name[j]);
                }
            }
            
            memset (line, '\0', sizeof(line));
            memset (tmp_main_port_name, '\0', sizeof(tmp_main_port_name));
            i = -1;
        }
        
        file_data++;
        i++;
    }

    return ret;
}

int bsp_xcvr_read_splitable_port_file(char *path)
{
    int ret = ERROR_SUCCESS;
    int len  = 0;
    loff_t pos = 0;
    struct file *pfile = NULL;
    char *buf_data = NULL;

    if (!path) { 
        DBG_ECHO (DEBUG_ERR, "file path invalid");
        return ERROR_FAILED;
    }

    buf_data =  (char *)kmalloc(20 * 1024, GFP_KERNEL);
    if (!buf_data) { 
        DBG_ECHO (DEBUG_ERR, "Alloc failed.");
        return ERROR_FAILED;
    }
    
    pfile = filp_open (path, O_RDONLY, 0);
    if (IS_ERR (pfile)) {
        DBG_ECHO (DEBUG_ERR, "file %s filp_open error.", path);
        kfree(buf_data);
        return ERROR_FAILED;
    } else {
        memset (buf_data, '\0', 20 * 1024);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 19, 0)
        len = kernel_read (pfile, pos, buf_data, 20 * 1024);
#else
        len = kernel_read (pfile, buf_data, 20 * 1024, &pos);
#endif
        if (len < 0) {
            kfree(buf_data);
            return ERROR_FAILED;
        }
    }

    ret = bsp_xcvr_get_port_info_per_line(buf_data);
    if (ret) {
        DBG_ECHO (DEBUG_ERR, "Get data per line failed.");
        kfree(buf_data);
        return ERROR_FAILED;
    }

    kfree(buf_data);
    return ret;
}

int bsp_xcvr_init_splitable_port_info(void)
{
    board_static_data *bd = bsp_get_board_data();
    char port_config_file_path[MAX_FILE_NAME_LEN] = {0};
    
    sprintf(port_config_file_path, SUB_PORT_CFG_PATH, bd->onie_platform_name);
    return bsp_xcvr_read_splitable_port_file(port_config_file_path);
}
static int bsp_xcvr_create_node_for_board(int slot_index)
{
    int ret = ERROR_SUCCESS;
    board_static_data * bd = NULL;
    int slot_xcvr_number = 0;
    int xcvr_info_start_index = 0;
    int xcvr_index = 0;
    int port_index_in_slot = 0;
    struct attribute_group * sfp_group = NULL;
    struct attribute_group * dom_group = NULL;
    char slot_str[20] = {'\0'};
    char temp_str[20] = {'\0'};
    int xcvr_nodes_count = 0;
    int i = 0;
    
    if (slot_index == MAIN_BOARD_SLOT_INDEX) {
        bd = bsp_get_board_data();
    } else if (slot_index >= 0 && slot_index < MAX_SLOT_NUM) {
        bd = bsp_get_slot_data(slot_index);
        
        if (bd->initialized != TRUE) {
            DBG_ECHO(DEBUG_INFO, "slot index %d data not initialized, return\n", slot_index);
            ret = ERROR_SUCCESS;
            goto exit;
        }
        
        sprintf(slot_str, "%d-", slot_index + 1);
    } else {
        DBG_ECHO(DEBUG_ERR, "invalid slot index %d\n", slot_index);
    }
    
    if (bd->optic_modlue_num == 0) {
        DBG_ECHO(DEBUG_INFO, "no ports on slot index %d, no sysfs nodes created.\n", slot_index);
        ret = ERROR_SUCCESS;
        goto exit;
    } else {
        DBG_ECHO(DEBUG_INFO, "slot index %d start creating sysfs nodes...\n", slot_index);
    }

    bsp_set_all_ext_phy_reset(slot_index, 1);
    DBG_ECHO(DEBUG_INFO, "All external phys were reset!\n");
    
#if DEBUG_VERSION
    bsp_set_all_port_power_on(1);
    DBG_ECHO(DEBUG_INFO, "All port cage were power on!\n");
#else
    DBG_ECHO(DEBUG_INFO, "All port cage power on were not changed\n");
#endif

    for (port_index_in_slot = 0; 
         port_index_in_slot < bd->optic_modlue_num; 
         port_index_in_slot++) {
        
        if (bd->cage_type[port_index_in_slot] == CAGE_TYPE_QSFP)
            bsp_set_optical_reset(slot_index, port_index_in_slot, TRUE);
        
        if (bd->cage_type[port_index_in_slot] == CAGE_TYPE_SFP) {
            bsp_set_optical_txdis(slot_index, port_index_in_slot, FALSE); 
            DBG_ECHO(DEBUG_INFO, "All sfp cage txdis were unset\n");
        }
    }
    
    DBG_ECHO(DEBUG_INFO, "All qsfp cage is unset\n");
    ret += bsp_enable_slot_all_9548(slot_index);
    
    DBG_ECHO(DEBUG_INFO, "slot %d 9548 enabled is done.\n", slot_index);

    ret += bsp_enable_slot_all_9545(slot_index);
    DBG_ECHO(DEBUG_INFO, "slot %d 9545 enabled is done.\n", slot_index);

    slot_xcvr_number = bd->optic_modlue_num;
    
    GET_SLOT_XCVR_INFO_START_INDEX(slot_index, xcvr_info_start_index);
    
    ret = bsp_xcvr_init_splitable_port_info();
    if (ret) {        
        DBG_ECHO(DEBUG_ERR, "bsp_xcvr_read_splitable_port_file operate failed!");
    }

    for (xcvr_index = xcvr_info_start_index, port_index_in_slot = 0; 
         xcvr_index < slot_xcvr_number + xcvr_info_start_index; 
         xcvr_index++, port_index_in_slot++) {
        xcvr_info[xcvr_index].slot_index = slot_index;
        xcvr_info[xcvr_index].sfp_index = port_index_in_slot;
        xcvr_info[xcvr_index].xcvr_global_index = xcvr_index;
        
        switch (bd->port_speed[port_index_in_slot]) {
        case SPEED_25G:
            sprintf(temp_str, "Eth25GE%s%d", slot_str, (port_index_in_slot + 1));
            sfp_group = &sfp25G_group;
            dom_group = &sfp25G_dom_group;
            break;
        case SPEED_100G:
            sprintf(temp_str, "Eth100GE%s%d", slot_str, (port_index_in_slot + 1)); 
            sfp_group = &sfp100G_group;
            dom_group = &sfp100G_dom_group;
            break;
        default:
            DBG_ECHO(DEBUG_ERR, "slot %d port %d speed 0x%x not supported! \n", slot_index, port_index_in_slot, bd->port_speed[port_index_in_slot]);    
        }
        
        CHECK_IF_ERROR_GOTO_EXIT(kobject_init_and_add(&(xcvr_info[xcvr_index].kobj_xcvr),   &static_kobj_ktype, kobj_xcvr_root, temp_str), "xcvr kobj init failed for slot index %d port index %d", slot_index, port_index_in_slot);
        CHECK_IF_ERROR_GOTO_EXIT(kobject_init_and_add(&(xcvr_info[xcvr_index].kobj_eeprom), &static_kobj_ktype, &(xcvr_info[xcvr_index].kobj_xcvr), "eeprom"), "eeprom kobj init failed for slot index %d port index %d", slot_index, port_index_in_slot);
        CHECK_IF_ERROR_GOTO_EXIT(kobject_init_and_add(&(xcvr_info[xcvr_index].kobj_dom),    &static_kobj_ktype, &(xcvr_info[xcvr_index].kobj_eeprom), "dom"), "dom kobj init failed for slot index %d port index %d", slot_index, port_index_in_slot);
        CHECK_IF_ERROR_GOTO_EXIT(sysfs_create_group(&(xcvr_info[xcvr_index].kobj_xcvr),   sfp_group), "create sfp group failed for slot index %d port index %d", slot_index, port_index_in_slot);
        CHECK_IF_ERROR_GOTO_EXIT(sysfs_create_group(&(xcvr_info[xcvr_index].kobj_eeprom), &eeprom_group), "create eeprom group failed for slot index %d port index %d", slot_index, port_index_in_slot);
        CHECK_IF_ERROR_GOTO_EXIT(sysfs_create_group(&(xcvr_info[xcvr_index].kobj_dom),    dom_group), "create dom group failed for slot index %d port index %d", slot_index, port_index_in_slot);
    
        if (splitable_port_info[xcvr_index].subport_num != 0) {            
            for (i = 0 ; i < splitable_port_info[xcvr_index].subport_num; i++) {
                CHECK_IF_ERROR_GOTO_EXIT(sysfs_create_link(kobj_xcvr_root, &(xcvr_info[xcvr_index].kobj_xcvr), splitable_port_info[xcvr_index].subport_name[i]), "create subport symlink failed for port_index %d subport_index %d", xcvr_index, i);
            }
        }
        
        if (bd->port_speed[port_index_in_slot] == SPEED_100G)
        {
            CHECK_IF_ERROR_GOTO_EXIT(sysfs_create_group(&(xcvr_info[xcvr_index].kobj_dom), &sfp100G_dom_txdis_group), "create dom txdis_group failed for slot index %d port index %d", slot_index, port_index_in_slot);
        }

        xcvr_nodes_count++;
    }
      
exit:
    if (ret != ERROR_SUCCESS) {
        DBG_ECHO(DEBUG_ERR, "create xcvr node for slot index %d failed! %d xcvr nodes created.\n", slot_index, xcvr_nodes_count);
        release_slot_sfp_kobj(slot_index);
    } else {
        DBG_ECHO(DEBUG_INFO, "create xcvr node for slot index %d success! %d xcvr nodes created.", slot_index, xcvr_nodes_count);
    }
    
    return ret;
}

static int __init xcvr_init(void)
{
    int ret = ERROR_SUCCESS;
    int slot_index = 0; 
    board_static_data *mbd = bsp_get_board_data();
    
    INIT_PRINT("xcvr module init started\n");
    memset(sfp_info, 0, sizeof(sfp_info));

    kobj_xcvr_root = kobject_create_and_add("xcvr", kobj_switch);
    if (!kobj_xcvr_root)
    {
        DBG_ECHO(DEBUG_ERR, "kobj_xcvr_root create falled!\n");          
        ret = -ENOMEM;       
        goto exit;     
    }

    if (sysfs_create_group(kobj_xcvr_root, &def_attr_group) != 0) {
        DBG_ECHO(DEBUG_INFO, "create fan default attr faild.\n");
        ret = -ENOSYS;
        goto exit;
    }
    
    memset(xcvr_info, 0, sizeof(xcvr_info));
    ret = sysfs_create_group(kobj_xcvr_root, &all_power_on_group);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create root power on node failed");
        
    ret = bsp_xcvr_create_node_for_board(MAIN_BOARD_SLOT_INDEX);
    CHECK_IF_ERROR_GOTO_EXIT(ret, "create mainboard xcvr node failed!");
    
    for (slot_index = 0; slot_index < mbd->slot_num; slot_index++) {
        ret = bsp_xcvr_create_node_for_board(slot_index);
        CHECK_IF_ERROR_GOTO_EXIT(ret, "create slot index %d xcvr node failed!", slot_index);
    }

exit:
    if (ret != 0) {
        DBG_ECHO(DEBUG_ERR, "xcvr module init failed!\n");
        release_all_sfp_kobj();
    } else {
        INIT_PRINT("xcvr module finished and success!");
    }

    return -ret;
}

void release_slot_sfp_kobj(int slot_index)
{
    int i;
    int xcvr_start_index;
    
    GET_SLOT_XCVR_INFO_START_INDEX(slot_index, xcvr_start_index);
   
    for (i = xcvr_start_index; i < MAX_OPTIC_PER_SLOT + xcvr_start_index; i++) {
        if (xcvr_info[i].kobj_dom.state_initialized)
            kobject_put(&(xcvr_info[i].kobj_dom));
        
        if (xcvr_info[i].kobj_eeprom.state_initialized)
            kobject_put(&(xcvr_info[i].kobj_eeprom));
        
        if (xcvr_info[i].kobj_xcvr.state_initialized)
            kobject_put(&(xcvr_info[i].kobj_xcvr));
    }
    
    DBG_ECHO(DEBUG_INFO, "slot index %d xcvr released!\n", slot_index);
    return;
}

void release_all_sfp_kobj()
{
    int i;
    board_static_data *bd = bsp_get_board_data();

    release_slot_sfp_kobj(MAIN_BOARD_SLOT_INDEX);
    DBG_ECHO(DEBUG_INFO, "mainboard xcvr released!\n");
    
    for (i = 0; i < bd->slot_num; i++) {
        release_slot_sfp_kobj(i);
    }
    
    if ((kobj_xcvr_root != NULL) && 
        (kobj_xcvr_root->state_initialized)) {
        sysfs_remove_group (kobj_xcvr_root, &def_attr_group);
        kobject_put(kobj_xcvr_root);
        kobj_xcvr_root = NULL;
    }

    return;
}

static void __exit xcvr_exit(void)
{
    release_all_sfp_kobj();
    INIT_PRINT("module xcvr uninstalled !\n");
}

module_init(xcvr_init);
module_exit(xcvr_exit);
module_param (loglevel, int, 0644);
MODULE_PARM_DESC(loglevel, "the log level(err=0x01, warning=0x02, info=0x04, dbg=0x08).\n");
MODULE_AUTHOR("Wang Xue <wang.xue@h3c.com>");
MODULE_DESCRIPTION("h3c system transceiver driver");
MODULE_LICENSE("Dual BSD/GPL");
