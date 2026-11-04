/*
 * Main Application Coordinator for ESP32-C3 Pump Controller
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include "sensor.h"
#include "ui.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static void scan_i2c_bus(void)
{
	const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C0 device not ready!");
		return;
	}

	LOG_INF("--- Scanning I2C bus (SDA: GPIO 5, SCL: GPIO 6) ---");
	int found = 0;
	for (uint8_t addr = 0x04; addr <= 0x77; addr++) {
		struct i2c_msg msg;
		uint8_t dummy = 0;

		msg.buf = &dummy;
		msg.len = 0U;
		msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

		if (i2c_transfer(i2c_dev, &msg, 1, addr) == 0) {
			LOG_INF("  [+] Found device at I2C address: 0x%02X", addr);
			found++;
		}
	}
	LOG_INF("--- I2C scan finished. Devices found: %d ---", found);
}

int main(void)
{
	int ret;
	int32_t distance_mm = 0;
	bool is_valid = false;
	uint8_t status_code = 0;
	uint32_t last_log_time = 0;

	LOG_INF("Starting ESP32-C3 Pump Controller Application...");

	/* Probe I2C bus to inspect connected devices */
	scan_i2c_bus();

	/* Test reading Model ID (reg 0xC0) from VL53L0X at 0x29 */
	const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	uint8_t model_id = 0;
	int test_r = i2c_reg_read_byte(i2c_dev, 0x29, 0xC0, &model_id);
	LOG_INF("Direct test i2c_reg_read_byte(0x29, 0xC0): ret=%d, model_id=0x%02X (expected 0xEE)",
		test_r, model_id);

	/* 1. Initialize UI module */
	ret = ui_init();
	if (ret < 0) {
		LOG_ERR("UI initialization failed: %d", ret);
		return 0;
	}

	/* Brief splash screen */
	ui_demo_splash();

	/* 2. Initialize VL53L0X Time-of-Flight sensor */
	ret = sensor_init();
	if (ret < 0) {
		LOG_ERR("Sensor initialization failed: %d", ret);
		/* Still show UI indicating sensor error */
		ui_show_distance(0, false);
	}

	LOG_INF("Entering continuous distance measurement loop...");

	while (1) {
		ret = sensor_read_distance(&distance_mm, &is_valid, &status_code);
		if (ret < 0) {
			is_valid = false;
		}

		/* Update OLED UI */
		ui_show_distance(distance_mm, is_valid);

		/* Periodic serial log (every 1000 ms) */
		uint32_t now = k_uptime_get_32();
		if ((now - last_log_time) >= 1000) {
			last_log_time = now;
			if (is_valid) {
				LOG_INF("Measured Distance: %d mm", distance_mm);
			} else {
				LOG_WRN("Distance: Out of range / invalid (raw: %d mm, status: %u)",
					distance_mm, status_code);
			}
		}

		/* 10 Hz refresh rate for smooth display updates */
		k_sleep(K_MSEC(100));
	}

	return 0;
}
