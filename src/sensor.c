/*
 * Sensor Module implementation for Sensirion SHT3x, TI INA219,
 * and STMicroelectronics VL53L4CD Time-of-Flight
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sensor.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "vl53l4cd_api.h"

LOG_MODULE_REGISTER (sensor, LOG_LEVEL_INF);

static const struct device* sht3xd_dev = DEVICE_DT_GET_ANY (sensirion_sht3xd);
static const struct device* ina219_dev = DEVICE_DT_GET_ANY (ti_ina219);

static struct vl53l4cd_dev tof_dev_inst = { 0 };
static bool tof_initialized             = false;

/* --- SHT3x Environmental Sensor --- */

int env_sensor_init (void) {
    if (!sht3xd_dev) {
        LOG_ERR ("SHT3xD devicetree node not found!");
        return -ENODEV;
    }

    if (!device_is_ready (sht3xd_dev)) {
        LOG_ERR ("SHT3xD device %s is not ready!", sht3xd_dev->name);
        return -ENODEV;
    }

    LOG_INF ("SHT3xD device %s initialized and ready.", sht3xd_dev->name);
    return 0;
}

int env_sensor_read (struct env_reading* reading) {
    struct sensor_value temp, hum;
    int ret;

    if (!reading) {
        return -EINVAL;
    }

    reading->is_valid = false;

    if (!sht3xd_dev || !device_is_ready (sht3xd_dev)) {
        return -ENODEV;
    }

    ret = sensor_sample_fetch (sht3xd_dev);
    if (ret != 0) {
        LOG_ERR ("SHT3x sample_fetch failed: %d", ret);
        return ret;
    }

    ret = sensor_channel_get (sht3xd_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret != 0) {
        LOG_ERR ("SHT3x channel_get(TEMP) failed: %d", ret);
        return ret;
    }

    ret = sensor_channel_get (sht3xd_dev, SENSOR_CHAN_HUMIDITY, &hum);
    if (ret != 0) {
        LOG_ERR ("SHT3x channel_get(HUMIDITY) failed: %d", ret);
        return ret;
    }

    /* Convert to integer + 1-decimal-place fraction */
    reading->temp_c_int = temp.val1;
    reading->temp_c_dec = (temp.val2 < 0 ? -temp.val2 : temp.val2) / 100000;

    reading->hum_int = hum.val1;
    reading->hum_dec = (hum.val2 < 0 ? -hum.val2 : hum.val2) / 100000;

    reading->is_valid = true;
    return 0;
}

/* --- INA219 Current & Voltage Monitor --- */

int power_sensor_init (void) {
    if (!ina219_dev) {
        LOG_ERR ("INA219 devicetree node not found!");
        return -ENODEV;
    }

    if (!device_is_ready (ina219_dev)) {
        LOG_ERR ("INA219 device %s is not ready!", ina219_dev->name);
        return -ENODEV;
    }

    LOG_INF ("INA219 device %s initialized and ready.", ina219_dev->name);
    return 0;
}

int power_sensor_read (struct power_reading* reading) {
    struct sensor_value v_bus, i_bus, p_bus;
    int ret;

    if (!reading) {
        return -EINVAL;
    }

    reading->is_valid = false;

    if (!ina219_dev || !device_is_ready (ina219_dev)) {
        return -ENODEV;
    }

    ret = sensor_sample_fetch (ina219_dev);
    if (ret != 0) {
        k_msleep (10);
        ret = sensor_sample_fetch (ina219_dev);
        if (ret != 0) {
            LOG_ERR ("INA219 sample_fetch failed: %d", ret);
            return ret;
        }
    }

    ret = sensor_channel_get (ina219_dev, SENSOR_CHAN_VOLTAGE, &v_bus);
    if (ret != 0) {
        LOG_ERR ("INA219 channel_get(VOLTAGE) failed: %d", ret);
        return ret;
    }

    ret = sensor_channel_get (ina219_dev, SENSOR_CHAN_CURRENT, &i_bus);
    if (ret != 0) {
        LOG_ERR ("INA219 channel_get(CURRENT) failed: %d", ret);
        return ret;
    }

    ret = sensor_channel_get (ina219_dev, SENSOR_CHAN_POWER, &p_bus);
    if (ret != 0) {
        LOG_ERR ("INA219 channel_get(POWER) failed: %d", ret);
        return ret;
    }

    /* Bus voltage: Volts to millivolts */
    reading->bus_voltage_mv = (v_bus.val1 * 1000) + (v_bus.val2 / 1000);

    /* Current: Amperes to milliamps */
    reading->current_ma = (i_bus.val1 * 1000) + (i_bus.val2 / 1000);

    /* Power: Watts to milliwatts */
    reading->power_mw = (p_bus.val1 * 1000) + (p_bus.val2 / 1000);

    reading->is_valid = true;
    return 0;
}

/* --- VL53L4CD Time-of-Flight Distance Sensor --- */

int tof_sensor_init (void) {
    const struct device* i2c_dev = DEVICE_DT_GET (DT_NODELABEL (i2c0));
    VL53L4CD_Error status;
    uint16_t sensor_id = 0;

    if (!device_is_ready (i2c_dev)) {
        LOG_ERR ("I2C0 device not ready for VL53L4CD!");
        return -ENODEV;
    }

    tof_dev_inst.i2c_dev  = i2c_dev;
    tof_dev_inst.i2c_addr = 0x29;

    /* Check sensor identification (0xEBAA) */
    status = VL53L4CD_GetSensorId (&tof_dev_inst, &sensor_id);
    if (status != VL53L4CD_ERROR_NONE || sensor_id != 0xEBAA) {
        LOG_DBG ("VL53L4CD not detected (err: %d, ID: 0x%04X)", status, sensor_id);
        tof_initialized = false;
        return -ENODEV;
    }

    uint8_t sys_status = 0;
    VL53L4CD_RdByte (&tof_dev_inst, 0x00E5, &sys_status);
    LOG_INF ("VL53L4CD detected (ID: 0x%04X, Status: 0x%02X). Initializing...", sensor_id, sys_status);

    status = VL53L4CD_SensorInit (&tof_dev_inst);
    if (status != VL53L4CD_ERROR_NONE) {
        LOG_ERR ("VL53L4CD_SensorInit failed: %d", status);
        tof_initialized = false;
        return -EIO;
    }

    /* 50ms timing budget, continuous ranging mode */
    status = VL53L4CD_SetRangeTiming (&tof_dev_inst, 50, 0);
    if (status != VL53L4CD_ERROR_NONE) {
        LOG_ERR ("VL53L4CD_SetRangeTiming failed: %d", status);
        tof_initialized = false;
        return -EIO;
    }

    status = VL53L4CD_StartRanging (&tof_dev_inst);
    if (status != VL53L4CD_ERROR_NONE) {
        LOG_ERR ("VL53L4CD_StartRanging failed: %d", status);
        tof_initialized = false;
        return -EIO;
    }

    tof_initialized = true;
    LOG_INF ("VL53L4CD continuous ranging started successfully.");
    return 0;
}

int tof_sensor_read (struct distance_reading* reading) {
    if (!reading) {
        return -EINVAL;
    }

    reading->is_valid = false;

    if (!tof_initialized) {
        int ret = tof_sensor_init ();
        if (ret < 0) {
            return ret;
        }
    }

    uint8_t is_ready      = 0;
    VL53L4CD_Error status = VL53L4CD_CheckForDataReady (&tof_dev_inst, &is_ready);
    if (status != VL53L4CD_ERROR_NONE) {
        LOG_DBG ("VL53L4CD CheckForDataReady err: %d", status);
        tof_initialized = false;
        return -EIO;
    }

    if (!is_ready) {
        return -EBUSY;
    }

    VL53L4CD_ResultsData_t results;
    status = VL53L4CD_GetResult (&tof_dev_inst, &results);
    VL53L4CD_ClearInterrupt (&tof_dev_inst);

    if (status != VL53L4CD_ERROR_NONE) {
        LOG_DBG ("VL53L4CD GetResult err: %d", status);
        return -EIO;
    }

    reading->distance_mm  = results.distance_mm;
    reading->range_status = results.range_status;
    reading->is_valid     = (results.range_status == 0);

    return 0;
}
