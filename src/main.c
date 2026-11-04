/*
 * Main Application Coordinator for ESP32-C3 Pump Controller
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "sensor.h"
#include "ui.h"

LOG_MODULE_REGISTER (main, LOG_LEVEL_INF);

static void scan_i2c_bus (void) {
    const struct device* i2c_dev = DEVICE_DT_GET (DT_NODELABEL (i2c0));

    if (!device_is_ready (i2c_dev)) {
        LOG_ERR ("I2C0 device not ready!");
        return;
    }

    LOG_INF ("--- Scanning I2C bus (SDA: GPIO 5, SCL: GPIO 6) ---");
    int found = 0;
    for (uint8_t addr = 0x04; addr <= 0x77; addr++) {
        struct i2c_msg msg;
        uint8_t dummy = 0;

        msg.buf   = &dummy;
        msg.len   = 0U;
        msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        if (i2c_transfer (i2c_dev, &msg, 1, addr) == 0) {
            LOG_INF ("  [+] Found device at I2C address: 0x%02X", addr);
            found++;
        }
    }
    LOG_INF ("--- I2C scan finished. Devices found: %d ---", found);
}

int main (void) {
    int ret;
    struct env_reading reading = { 0 };
    uint32_t last_log_time     = 0;

    LOG_INF ("Starting ESP32-C3 Pump Controller Application...");

    /* 1. Probe I2C bus to inspect connected devices */
    scan_i2c_bus ();

    /* 2. Initialize UI module */
    ret = ui_init ();
    if (ret < 0) {
        LOG_ERR ("UI initialization failed: %d", ret);
        return 0;
    }

    /* Brief splash screen */
    ui_demo_splash ();

    /* 3. Initialize SHT3x environmental sensor */
    ret = env_sensor_init ();
    if (ret < 0) {
        LOG_ERR ("Environment sensor initialization failed: %d", ret);
        ui_show_env (0, 0, 0, 0, false);
    }

    LOG_INF ("Entering continuous environmental measurement loop...");

    while (1) {
        ret = env_sensor_read (&reading);
        if (ret < 0) {
            reading.is_valid = false;
        }

        /* Update 0.42\" OLED UI */
        ui_show_env (reading.temp_c_int, reading.temp_c_dec,
                     reading.hum_int, reading.hum_dec,
                     reading.is_valid);

        /* Periodic serial log (every 1000 ms) */
        uint32_t now = k_uptime_get_32 ();
        if ((now - last_log_time) >= 1000) {
            last_log_time = now;
            if (reading.is_valid) {
                LOG_INF ("Environment: Temperature: %d.%d C | Humidity: %d.%d %%RH",
                         reading.temp_c_int, reading.temp_c_dec,
                         reading.hum_int, reading.hum_dec);
            } else {
                LOG_WRN ("Environment: Failed to read from SHT3x (err: %d)", ret);
            }
        }

        /* Refresh sensor readout every 1000 ms */
        k_sleep (K_MSEC (1000));
    }

    return 0;
}
