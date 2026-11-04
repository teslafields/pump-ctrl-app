/*
 * Sensor Module for VL53L0X Time-of-Flight Distance Sensor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the VL53L0X sensor.
 *
 * @return 0 on success, negative errno on failure.
 */
int sensor_init (void);

/**
 * @brief Read distance in millimeters from the VL53L0X.
 *
 * @param distance_mm Pointer to store the measured distance in mm.
 * @param is_valid Pointer to store whether the measurement status was valid.
 * @param status_code Pointer to store the raw ST range status code.
 * @return 0 on success, negative errno on failure.
 */
int sensor_read_distance (int32_t* distance_mm, bool* is_valid, uint8_t* status_code);

#endif /* APP_SENSOR_H_ */
