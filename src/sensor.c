/*
 * Sensor Module implementation for Sensirion SHT3x
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sensor.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER (sensor, LOG_LEVEL_INF);

static const struct device* sht3xd_dev = DEVICE_DT_GET_ANY (sensirion_sht3xd);

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
        LOG_ERR ("sensor_sample_fetch failed: %d", ret);
        return ret;
    }

    ret = sensor_channel_get (sht3xd_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret != 0) {
        LOG_ERR ("sensor_channel_get(TEMP) failed: %d", ret);
        return ret;
    }

    ret = sensor_channel_get (sht3xd_dev, SENSOR_CHAN_HUMIDITY, &hum);
    if (ret != 0) {
        LOG_ERR ("sensor_channel_get(HUMIDITY) failed: %d", ret);
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
