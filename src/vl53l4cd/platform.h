/*
 * VL53L4CD Platform Abstraction Layer for Zephyr RTOS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VL53L4CD_PLATFORM_H_
#define VL53L4CD_PLATFORM_H_

#include <stdint.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

/**
 * @brief VL53L4CD Device Structure
 */
struct vl53l4cd_dev {
    const struct device *i2c_dev;
    uint16_t i2c_addr; /* 7-bit I2C address, default 0x29 */
};

typedef struct vl53l4cd_dev* Dev_t;

/**
 * @brief Read an 8-bit byte from a 16-bit register address.
 */
uint8_t VL53L4CD_RdByte(Dev_t dev, uint16_t reg, uint8_t *val);

/**
 * @brief Write an 8-bit byte to a 16-bit register address.
 */
uint8_t VL53L4CD_WrByte(Dev_t dev, uint16_t reg, uint8_t val);

/**
 * @brief Read a 16-bit word from a 16-bit register address (Big-Endian).
 */
uint8_t VL53L4CD_RdWord(Dev_t dev, uint16_t reg, uint16_t *val);

/**
 * @brief Write a 16-bit word to a 16-bit register address (Big-Endian).
 */
uint8_t VL53L4CD_WrWord(Dev_t dev, uint16_t reg, uint16_t val);

/**
 * @brief Read a 32-bit double-word from a 16-bit register address (Big-Endian).
 */
uint8_t VL53L4CD_RdDWord(Dev_t dev, uint16_t reg, uint32_t *val);

/**
 * @brief Write a 32-bit double-word to a 16-bit register address (Big-Endian).
 */
uint8_t VL53L4CD_WrDWord(Dev_t dev, uint16_t reg, uint32_t val);

/**
 * @brief Wait for a specified number of milliseconds.
 */
uint8_t VL53L4CD_WaitMs(Dev_t dev, uint32_t TimeMs);

#endif /* VL53L4CD_PLATFORM_H_ */
