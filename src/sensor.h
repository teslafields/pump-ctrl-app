/*
 * Sensor Module for Environmental (SHT3x), Power Monitoring (INA219),
 * and Time-of-Flight Distance (VL53L4CD)
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
 * @brief Power & current measurement data from INA219
 */
struct power_reading {
    int32_t bus_voltage_mv; /* Bus voltage in millivolts (e.g. 5040 = 5.04 V) */
    int32_t current_ma;     /* Current in milliamps (e.g. 23 mA) */
    int32_t power_mw;       /* Power in milliwatts (e.g. 115 mW) */
    bool is_valid;          /* True if reading was successful */
};

/**
 * @brief Distance measurement data from VL53L4CD
 */
struct distance_reading {
    int32_t distance_mm;   /* Measured distance in millimeters */
    uint8_t range_status;  /* 0 = Valid measurement */
    bool is_valid;         /* True if reading was successful and valid */
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

/**
 * @brief Initialize INA219 power and current sensor.
 *
 * @return 0 on success, negative errno on failure.
 */
int power_sensor_init (void);

/**
 * @brief Read bus voltage, current, and power from INA219.
 *
 * @param reading Pointer to power_reading struct to populate.
 * @return 0 on success, negative errno on failure.
 */
int power_sensor_read (struct power_reading* reading);

/**
 * @brief Initialize VL53L4CD Time-of-Flight distance sensor.
 *
 * @return 0 on success, negative errno on failure.
 */
int tof_sensor_init (void);

/**
 * @brief Read distance from VL53L4CD.
 *
 * @param reading Pointer to distance_reading struct to populate.
 * @return 0 on success, -EBUSY if data not ready yet, negative errno on failure.
 */
int tof_sensor_read (struct distance_reading* reading);

#endif /* APP_SENSOR_H_ */
