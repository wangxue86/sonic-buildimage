#ifndef __SENSOR_H
#define __SENSOR_H

#include <linux/hwmon-sysfs.h>


enum senosr_custom_attributes
{
    NUM_TEMP_SENSORS = SYSFS_ATTR_BUTT + 1,
    TEMP_ALIAS,
    TEMP_TYPE,
    TEMP_MAX,
    TEMP_MAX_HYST,
    TEMP_MIN,
    TEMP_INPUT
};




//custom node
static SENSOR_DEVICE_ATTR(num_temp_sensors, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, NUM_TEMP_SENSORS);
static SENSOR_DEVICE_ATTR(temp_alias, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_ALIAS);
static SENSOR_DEVICE_ATTR(temp_type, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_TYPE);
static SENSOR_DEVICE_ATTR(temp_min, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_MIN);
static SENSOR_DEVICE_ATTR(temp_max, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_MAX);
static SENSOR_DEVICE_ATTR(temp_input, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_INPUT);
static SENSOR_DEVICE_ATTR(temp_max_hyst, S_IRUGO, bsp_sysfs_temp_sensor_get_attr, NULL, TEMP_MAX_HYST);
















#endif

