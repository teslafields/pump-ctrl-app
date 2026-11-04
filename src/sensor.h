/*
 * Sensor Module for Environmental & Distance Sensing
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Environmental measurement data from SHT3x
 */
struct env_reading {
    int32_t temp_c_int; /* Degrees Celsius integer part */
    int32_t temp_c_dec; /* Degrees Celsius decimal tenths (0-9) */
    int32_t hum_int;    /* Relative Humidity integer % */
    int32_t hum_dec;    /* Relative Humidity decimal tenths (0-9) */
    bool is_valid;      /* True if reading was successful */
};

/**
 * @brief Initialize SHT3x environmental sensor.
 *
 * @return 0 on success, negative errno on failure.
 */
int env_sensor_init (void);

/**
 * @brief Read temperature and humidity from SHT3x.
 *
 * @param reading Pointer to env_reading struct to populate.
 * @return 0 on success, negative errno on failure.
 */
int env_sensor_read (struct env_reading* reading);

#endif /* APP_SENSOR_H_ */
