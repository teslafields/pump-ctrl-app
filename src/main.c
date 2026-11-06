/*
 * Main Application Coordinator for ESP32-C3 Pump Controller
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "pump.h"
#include "sensor.h"
#include "ui.h"

LOG_MODULE_REGISTER (main, LOG_LEVEL_INF);

static K_SEM_DEFINE (refresh_sem, 0, 1);

static void pump_event_handler (bool is_on) {
    ARG_UNUSED (is_on);
    /* Wake main measurement loop immediately on pump state change */
    k_sem_give (&refresh_sem);
}

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
    struct env_reading env     = { 0 };
    struct power_reading pwr   = { 0 };
    uint32_t last_log_time     = 0;
    bool prev_pump_state       = false;

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
        LOG_WRN ("Environment sensor (SHT3x) not ready (err: %d)", ret);
    }

    /* 4. Initialize INA219 power/current sensor */
    ret = power_sensor_init ();
    if (ret < 0) {
        LOG_WRN ("Power sensor (INA219) not ready (err: %d)", ret);
    }

    /* 5. Initialize Pump controller (GPIO 3 gate, GPIO 9 BOOT button) */
    ret = pump_init (pump_event_handler);
    if (ret < 0) {
        LOG_ERR ("Pump controller initialization failed (err: %d)", ret);
    }

    LOG_INF ("Entering continuous sensor measurement loop...");

    while (1) {
        /* Read sensors */
        ret = env_sensor_read (&env);
        if (ret < 0) {
            env.is_valid = false;
        }

        ret = power_sensor_read (&pwr);
        if (ret < 0) {
            pwr.is_valid = false;
        }

        bool current_pump_state = pump_is_on ();

        /* Trigger immediate log if pump state changed */
        if (current_pump_state != prev_pump_state) {
            prev_pump_state = current_pump_state;
            last_log_time   = 0;
        }

        /* Update OLED UI */
        ui_show_dashboard (&env, &pwr, current_pump_state, -1);

        /* Periodic serial log (every 1000 ms, or immediately upon state change) */
        uint32_t now = k_uptime_get_32 ();
        if ((now - last_log_time) >= 1000) {
            last_log_time = now;

            const char* pump_str = current_pump_state ? "ON" : "OFF";

            if (pwr.is_valid && env.is_valid) {
                LOG_INF ("PUMP: %3s | INA219: %d.%02d V | %4d mA | %4d mW  ||  SHT30: %d.%d C | %d.%d %%RH",
                         pump_str,
                         pwr.bus_voltage_mv / 1000,
                         (pwr.bus_voltage_mv % 1000) / 10,
                         pwr.current_ma,
                         pwr.power_mw,
                         env.temp_c_int, env.temp_c_dec,
                         env.hum_int, env.hum_dec);
            } else if (pwr.is_valid) {
                LOG_INF ("PUMP: %3s | INA219: %d.%02d V | %4d mA | %4d mW  ||  SHT30: [OFFLINE]",
                         pump_str,
                         pwr.bus_voltage_mv / 1000,
                         (pwr.bus_voltage_mv % 1000) / 10,
                         pwr.current_ma,
                         pwr.power_mw);
            } else if (env.is_valid) {
                LOG_INF ("PUMP: %3s | INA219: [OFFLINE]  ||  SHT30: %d.%d C | %d.%d %%RH",
                         pump_str,
                         env.temp_c_int, env.temp_c_dec,
                         env.hum_int, env.hum_dec);
            } else {
                LOG_WRN ("PUMP: %3s | All I2C sensors offline!", pump_str);
            }
        }

        /* Wait up to 1000 ms, or wake immediately if button toggles pump */
        k_sem_take (&refresh_sem, K_MSEC (1000));
    }

    return 0;
}
