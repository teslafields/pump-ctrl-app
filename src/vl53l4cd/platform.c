/*
 * VL53L4CD Platform Abstraction Layer Implementation for Zephyr RTOS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "platform.h"
#include "vl53l4cd_api.h"

uint8_t VL53L4CD_WrByte(Dev_t dev, uint16_t reg, uint8_t val)
{
    if (!dev || !dev->i2c_dev) {
        return VL53L4CD_ERROR_INVALID_ARGUMENT;
    }
    uint8_t buf[3];
    buf[0] = (uint8_t)(reg >> 8);
    buf[1] = (uint8_t)(reg & 0xFF);
    buf[2] = val;
    int ret = i2c_write(dev->i2c_dev, buf, 3, dev->i2c_addr);
    return (ret == 0) ? VL53L4CD_ERROR_NONE : 255;
}

uint8_t VL53L4CD_WrWord(Dev_t dev, uint16_t reg, uint16_t val)
{
    if (!dev || !dev->i2c_dev) {
        return VL53L4CD_ERROR_INVALID_ARGUMENT;
    }
    uint8_t buf[4];
    buf[0] = (uint8_t)(reg >> 8);
    buf[1] = (uint8_t)(reg & 0xFF);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val & 0xFF);
    int ret = i2c_write(dev->i2c_dev, buf, 4, dev->i2c_addr);
    return (ret == 0) ? VL53L4CD_ERROR_NONE : 255;
}

uint8_t VL53L4CD_WrDWord(Dev_t dev, uint16_t reg, uint32_t val)
{
    if (!dev || !dev->i2c_dev) {
        return VL53L4CD_ERROR_INVALID_ARGUMENT;
    }
    uint8_t buf[6];
    buf[0] = (uint8_t)(reg >> 8);
    buf[1] = (uint8_t)(reg & 0xFF);
    buf[2] = (uint8_t)(val >> 24);
    buf[3] = (uint8_t)(val >> 16);
    buf[4] = (uint8_t)(val >> 8);
    buf[5] = (uint8_t)(val & 0xFF);
    int ret = i2c_write(dev->i2c_dev, buf, 6, dev->i2c_addr);
    return (ret == 0) ? VL53L4CD_ERROR_NONE : 255;
}

uint8_t VL53L4CD_RdByte(Dev_t dev, uint16_t reg, uint8_t *val)
{
    if (!dev || !dev->i2c_dev || !val) {
        return VL53L4CD_ERROR_INVALID_ARGUMENT;
    }
    uint8_t reg_buf[2];
    reg_buf[0] = (uint8_t)(reg >> 8);
    reg_buf[1] = (uint8_t)(reg & 0xFF);
    int ret = i2c_write_read(dev->i2c_dev, dev->i2c_addr, reg_buf, 2, val, 1);
    return (ret == 0) ? VL53L4CD_ERROR_NONE : 255;
}

uint8_t VL53L4CD_RdWord(Dev_t dev, uint16_t reg, uint16_t *val)
{
    if (!dev || !dev->i2c_dev || !val) {
        return VL53L4CD_ERROR_INVALID_ARGUMENT;
    }
    uint8_t reg_buf[2];
    uint8_t rx_buf[2];
    reg_buf[0] = (uint8_t)(reg >> 8);
    reg_buf[1] = (uint8_t)(reg & 0xFF);
    int ret = i2c_write_read(dev->i2c_dev, dev->i2c_addr, reg_buf, 2, rx_buf, 2);
    if (ret == 0) {
        *val = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];
        return VL53L4CD_ERROR_NONE;
    }
    return 255;
}

uint8_t VL53L4CD_RdDWord(Dev_t dev, uint16_t reg, uint32_t *val)
{
    if (!dev || !dev->i2c_dev || !val) {
        return VL53L4CD_ERROR_INVALID_ARGUMENT;
    }
    uint8_t reg_buf[2];
    uint8_t rx_buf[4];
    reg_buf[0] = (uint8_t)(reg >> 8);
    reg_buf[1] = (uint8_t)(reg & 0xFF);
    int ret = i2c_write_read(dev->i2c_dev, dev->i2c_addr, reg_buf, 2, rx_buf, 4);
    if (ret == 0) {
        *val = ((uint32_t)rx_buf[0] << 24) |
               ((uint32_t)rx_buf[1] << 16) |
               ((uint32_t)rx_buf[2] << 8)  |
               (uint32_t)rx_buf[3];
        return VL53L4CD_ERROR_NONE;
    }
    return 255;
}

uint8_t VL53L4CD_WaitMs(Dev_t dev, uint32_t TimeMs)
{
    ARG_UNUSED(dev);
    if (TimeMs == 0) {
        return 0;
    }
    if (TimeMs < 10) {
        k_busy_wait(TimeMs * 1000);
    } else {
        k_msleep(TimeMs);
    }
    return 0;
}
