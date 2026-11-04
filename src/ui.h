/*
 * UI Module for 01Space ESP32-C3 0.42" OLED (SSD1306 72x40)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_UI_H_
#define APP_UI_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize display and character framebuffer.
 *
 * @return 0 on success, negative errno on failure.
 */
int ui_init (void);

/**
 * @brief Run one full cycle of the showcase demo scenes.
 */
void ui_run_demo_cycle (void);

/* Individual demo scenes */
void ui_demo_splash (void);
void ui_demo_boxes (void);
void ui_demo_circles (void);
void ui_demo_lines (void);
void ui_demo_inversion (void);
void ui_demo_bouncing_box (void);
void ui_demo_text_stats (void);

/**
 * @brief Display the measured distance dashboard on OLED.
 *
 * @param distance_mm Measured distance in millimeters.
 * @param is_valid True if distance measurement is valid.
 */
void ui_show_distance (int32_t distance_mm, bool is_valid);

/**
 * @brief Display temperature and humidity environment dashboard on OLED.
 *
 * @param temp_int Temperature integer part in °C
 * @param temp_dec Temperature decimal tenths (0-9)
 * @param hum_int Relative Humidity integer %
 * @param hum_dec Relative Humidity decimal tenths (0-9)
 * @param is_valid True if sensor reading is valid
 */
void ui_show_env (int32_t temp_int, int32_t temp_dec, int32_t hum_int, int32_t hum_dec, bool is_valid);

#endif /* APP_UI_H_ */
