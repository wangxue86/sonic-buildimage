#ifndef __I2C_DEV_REGISTER_H
#define __I2C_DEV_REGISTER_H

#define REG_ADDR_LM75_TEMP                       0x0
#define LM75_DEFAULT_HYST                        75
#define REG_ADDR_MAX6696_TEMP_LOCAL              0x0
#define REG_ADDR_MAX6696_TEMP_REMOTE             0x1
#define REG_ADDR_MAX6696_WRITE_CONFIG            0x9
#define REG_ADDR_MAX6696_READ_CONFIG             0x3
#define REG_ADDR_MAX6696_READ_ALERT_HI_LOCAL     0x5
#define REG_ADDR_MAX6696_READ_ALERT_LO_LOCAL     0x6
#define REG_ADDR_MAX6696_READ_ALERT_HI_REMOTE    0x7
#define REG_ADDR_MAX6696_READ_ALERT_LO_REMOTE    0x8
#define REG_ADDR_MAX6696_WRITE_ALERT_HI_LOCAL    0xB
#define REG_ADDR_MAX6696_WRITE_ALERT_LO_LOCAL    0xC
#define REG_ADDR_MAX6696_RW_OT2_LOCAL            0x17
#define REG_ADDR_MAX6696_RW_OT2_REMOTE           0x16
#define MAX6696_SPOT_SELECT_BIT                  3

#define REG_ADDR_PW650W_VOUT                    0x8B       
#define REG_ADDR_PW650W_IOUT                    0x8C       
#define REG_ADDR_PW650W_TSTATUS                 0x7D      
#define REG_ADDR_PW650W_TEMPER                  0x8D       
#define REG_ADDR_PW650W_FAN_1                   0x90       
#define REG_ADDR_PW650W_FAN_2                   0x91      
#define REG_ADDR_PW650W_SN                      0xA0      
#define REG_ADDR_PW650W_PRONUMB                 0x15       
#define REG_ADDR_PW650W_PIN                     0x97      
#define REG_ADDR_PW650W_POUT                    0x96      
#define REG_ADDR_PW650W_HW_VER                  0x9B      
#define REG_ADDR_PW650W_FW_VER                  0x9B      
#define PW650_VOUT_BYTE_COUNT                   2
#define PW650_IOUT_BYTE_COUNT                   2
#define PW650_VIN_BYTE_COUNT                    2
#define PW650_IIN_BYTE_COUNT                    2
#define PW650_STATEMPURE_BYTE_COUNT             1
#define PW650_TEMPURE_BYTE_COUNT                2
#define PW650_FAN_BYTE_COUNT                    2
#define PW650_SN_BYTE_COUNT                     20
#define PW650_PRONUMB_BYTE_COUNT                12
#define PW650_PIN_BYTE_COUNT                    2
#define PW650_POUT_BYTE_COUNT                   2
#define REG_ADDR_PW1600W_VOUT                   0x8B       
#define REG_ADDR_PW1600W_IOUT                   0x8C       
#define REG_ADDR_PW1600W_VIN                    0x88       
#define REG_ADDR_PW1600W_IIN                    0x89       
#define REG_ADDR_PW1600W_HW_VER                 0x9B      
#define REG_ADDR_PW1600W_FW_VER                 0xD9    
#define REG_ADDR_PW1600_SN                      0x60
#define REG_ADDR_PW1600W_PDTNAME                0x40
#define REG_ADDR_PW1600W_FAN                    0x90
#define REG_ADDR_PW1600W_TSTATUS                0x7D      
#define REG_ADDR_PW1600W_TEMPER                 0x8D      
#define REG_ADDR_PW1600W_PIN                    0x97      
#define REG_ADDR_PW1600W_POUT                   0x96      
#define REG_ADDR_PW1600W_INVOL_TYPE             0x80      
#define PW1600_VOUT_BYTE_COUNT                  2
#define PW1600_IOUT_BYTE_COUNT                  2
#define PW1600_VIN_BYTE_COUNT                   2
#define PW1600_IIN_BYTE_COUNT                   2
#define PW1600_VIN_TYPE_BYTE_COUNT              1
#define PW1600_SN_BYTE_COUNT                    20
#define PW1600_PRONUMB_BYTE_COUNT               33
#define PW1600_FAN_BYTE_COUNT                   2
#define PW1600_STATEMPURE_BYTE_COUNT            1
#define PW1600_TEMPURE_BYTE_COUNT               2
#define PW1600_PIN_BYTE_COUNT                   2
#define PW1600_POUT_BYTE_COUNT                  2
#define PSU_MAX_SN_LEN                          64
#define PSU_MAX_SENSOR_NUM_MAX                  2
#define PSU_MAX_FAN_NUM_MAX                     2
#define PSU_MAX_PRODUCT_NUM_LEN                 64
#define PSU_MAX_PRODUCT_NAME_LEN                64
#define PSU_MAX_HW_VERSION_LEN                  4
#define PSU_MAX_FW_VERSION_LEN                  4
#define PSU_MAX_INFO_LEN                        128
#define ADM11661_MAX_INFO_LEN                   128
#define REG_ADDR_FAN_SN                         0x28   
#define REG_ADDR_FAN_PDT_NAME                   0x08  
#define REG_ADDR_FAN_HW_VER                     0x0
#define FAN_SN_BYTE_COUNT                       64
#define FAN_PDT_NAME_BYTE_COUNT                 32
#define FAN_HW_VER_BYTE_COUNT                   8
#define ISL68127_REG_VALUE_MAX_LEN              2
#define REG_ADDR_ISL68127_CMD_VOUT              0x21
#define REG_ADDR_ISL68127_CMD_PAGE              0x0
#define REG_ADDR_TX_DISABLE_8636                86  
#define TX1_DISABLE_BIT                         1
#define TX2_DISABLE_BIT                         2
#define TX3_DISABLE_BIT                         4
#define TX4_DISABLE_BIT                         8
#define REG_ADDR_TEMPERATURE_8636               22
#define REG_ADDR_VOLTAGE_8636                   26
#define REG_ADDR_RX_POWER_8636                  34
#define REG_ADDR_TX_BIAS_8636                   42
#define REG_ADDR_TX_POWER_8636                  50
#define REG_ADDR_MANUFACTURE_NAME_8636          148
#define REG_ADDR_MODEL_NAME_8636                168
#define REG_ADDR_CABLE_LENGTH_8636              142
#define REG_ADDR_CONNECTOR_8636                 130
#define REG_ADDR_HW_VERSION_8636                184
#define REG_ADDR_INFO_BULK_8636                 128
#define REG_ADDR_VENDOR_DATE_8636               212
#define REG_ADDR_VENDOR_OUI_8636                165
#define REG_ADDR_SERIAL_NUM_8636                196
#define REG_ADDR_TEMPERATURE_8472               96
#define REG_ADDR_VOLTAGE_8472                   98
#define REG_ADDR_RX_POWER_8472                  104
#define REG_ADDR_TX_BIAS_8472                   100
#define REG_ADDR_TX_POWER_8472                  102
#define REG_ADDR_MANUFACTURE_NAME_8472          20
#define REG_ADDR_MODEL_NAME_8472                40
#define REG_ADDR_DIAG_SUPPORT_8472              0x5c
#define REG_ADDR_SERIAL_NUM_8472                68
#define REG_ADDR_CONNECTOR_8472                 2
#define REG_ADDR_HW_VERSION_8472                56
#define REG_ADDR_INFO_BULK_8472                 0
#define REG_ADDR_VENDOR_DATE_8472               84
#define REG_ADDR_VENDOR_OUI_8472                37
#define REG_ADDR_CABLE_LENGTH_8472              18
#define REG_ADDR_DEV_ADM1166_BASE               0xa0
#define REG_ADDR_ADM1166_RRCTRL                 0x82
#define ADM1166_RRCTRL_OPERATION_ENABLE         1
#define ADM1166_RRCTRL_OPERATION_STOPWRITE      2
#define ADM1166_RRCTRL_REG_GO                   1 << 0
#define ADM1166_RRCTRL_REG_ENABLE               1 << 1
#define ADM1166_RRCTRL_REG_STOPWRITE            1 << 3

typedef enum  {
    MAX6696_LOCAL_SOPT_INDEX = 0, 
    MAX6696_REMOTE_CHANNEL1_SOPT_INDEX,
    MAX6696_REMOTE_CHANNEL2_SOPT_INDEX,
    MAX6696_SPOT_NUM
} MAX6696_SPOT_INDEX;

typedef enum  {
    MAX6696_LOCAL_HIGH_ALERT = 0, 
    MAX6696_LOCAL_LOW_ALERT,
    MAX6696_LOCAL_OT2_LIMIT,
    MAX6696_REMOTE_CHANNEL1_HIGH_ALERT,
    MAX6696_REMOTE_CHANNEL1_LOW_ALERT,
    MAX6696_REMOTE_CHANNEL1_OT2_LIMIT,
    MAX6696_REMOTE_CHANNEL2_HIGH_ALERT,
    MAX6696_REMOTE_CHANNEL2_LOW_ALERT,
    MAX6696_REMOTE_CHANNEL2_OT2_LIMIT,
    SET_MAX6696_LOCAL_HIGH_ALERT,
    SET_MAX6696_REMOTE_CHANNEL1_HIGH_ALERT,
    SET_MAX6696_REMOTE_CHANNEL2_HIGH_ALERT,
    SET_MAX6696_LOCAL_OT2_LIMIT,
    SET_MAX6696_REMOTE_CHANNEL1_OT2_LIMIT,
    SET_MAX6696_REMOTE_CHANNEL2_OT2_LIMIT,
    SET_MAX6696_LOCAL_LOW_ALERT,
    SET_MAX6696_REMOTE_CHANNEL1_LOW_ALERT,
    SET_MAX6696_REMOTE_CHANNEL2_LOW_ALERT,
    MAX6696_LIMIT_BUTT
} MAX6696_LIMIT_INDEX;
#endif
