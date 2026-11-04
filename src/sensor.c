/*
 * Self-contained VL53L0X Time-of-Flight Sensor Driver
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sensor.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER (sensor, LOG_LEVEL_INF);

#define VL53L0X_I2C_ADDR 0x29

/* VL53L0X Register Map */
#define REG_SYSRANGE_START 0x00
#define REG_SYSTEM_SEQUENCE_CONFIG 0x01
#define REG_SYSTEM_INTERMEASUREMENT_PERIOD 0x04
#define REG_SYSTEM_RANGE_CONFIG 0x09
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO 0x0A
#define REG_SYSTEM_INTERRUPT_CLEAR 0x0B
#define REG_SYSTEM_THRESH_HIGH 0x0C
#define REG_SYSTEM_THRESH_LOW 0x0E
#define REG_RESULT_INTERRUPT_STATUS 0x13
#define REG_RESULT_RANGE_STATUS 0x14
#define REG_RESULT_PEAK_SIGNAL_RATE_REF 0xB6
#define REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM 0x28
#define REG_MSRC_CONFIG_CONTROL 0x60
#define REG_PRE_RANGE_CONFIG_MIN_SNR 0x27
#define REG_PRE_RANGE_CONFIG_VALID_PHASE_LOW 0x56
#define REG_PRE_RANGE_CONFIG_VALID_PHASE_HIGH 0x57
#define REG_PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT 0x64
#define REG_FINAL_RANGE_CONFIG_MIN_SNR 0x67
#define REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW 0x47
#define REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH 0x48
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT 0x44
#define REG_PRE_RANGE_CONFIG_SIGMA_THRESH_HI 0x61
#define REG_PRE_RANGE_CONFIG_SIGMA_THRESH_LO 0x62
#define REG_PRE_RANGE_CONFIG_VCSEL_PERIOD 0x50
#define REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI 0x51
#define REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO 0x52
#define REG_SYSTEM_HISTOGRAM_BIN 0x81
#define REG_GPIO_HV_MUX_ACTIVE_HIGH 0x84
#define REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV 0x89
#define REG_I2C_SLAVE_DEVICE_ADDRESS 0x8A
#define REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI 0x71
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO 0x72
#define REG_IDENTIFICATION_MODEL_ID 0xC0
#define REG_IDENTIFICATION_REVISION_ID 0xC2
#define REG_GLOBAL_CONFIG_VCSEL_WIDTH 0x32
#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0 0xB0
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT 0xB6
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD 0x4E
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET 0x4F
#define REG_POWER_MANAGEMENT_GO1_POWER_FORCE 0x80
#define REG_MSRC_CONFIG_TIMEOUT_MACROP 0x46

#define decodeVcselPeriod(reg_val) (((reg_val) + 1) << 1)
#define calcMacroPeriod(vcsel_period_pclks) \
    ((((uint32_t)2304 * (vcsel_period_pclks) * 1655) + 500) / 1000)

static const struct device* i2c_dev;
static uint8_t stop_variable;
static uint32_t measurement_timing_budget_us;

struct SequenceStepEnables {
    bool tcc, dss, msrc, pre_range, final_range;
};

struct SequenceStepTimeouts {
    uint16_t pre_range_vcsel_period_pclks, final_range_vcsel_period_pclks;
    uint16_t msrc_dss_tcc_mclks, pre_range_mclks, final_range_mclks;
    uint32_t msrc_dss_tcc_us, pre_range_us, final_range_us;
};

static int write_reg (uint8_t reg, uint8_t val) {
    for (int retry = 0; retry < 5; retry++) {
        if (i2c_reg_write_byte (i2c_dev, VL53L0X_I2C_ADDR, reg, val) == 0) {
            return 0;
        }
        k_msleep (2);
    }
    return -EIO;
}

static int read_reg (uint8_t reg, uint8_t* val) {
    for (int retry = 0; retry < 5; retry++) {
        if (i2c_reg_read_byte (i2c_dev, VL53L0X_I2C_ADDR, reg, val) == 0) {
            return 0;
        }
        k_msleep (2);
    }
    return -EIO;
}

static int write_reg16 (uint8_t reg, uint16_t val) {
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    for (int retry = 0; retry < 5; retry++) {
        if (i2c_burst_write (i2c_dev, VL53L0X_I2C_ADDR, reg, buf, 2) == 0) {
            return 0;
        }
        k_msleep (2);
    }
    return -EIO;
}

static int read_reg16 (uint8_t reg, uint16_t* val) {
    uint8_t buf[2];
    for (int retry = 0; retry < 5; retry++) {
        if (i2c_burst_read (i2c_dev, VL53L0X_I2C_ADDR, reg, buf, 2) == 0) {
            *val = ((uint16_t)buf[0] << 8) | buf[1];
            return 0;
        }
        k_msleep (2);
    }
    return -EIO;
}

static int write_multi (uint8_t reg, const uint8_t* src, uint8_t count) {
    for (int retry = 0; retry < 5; retry++) {
        if (i2c_burst_write (i2c_dev, VL53L0X_I2C_ADDR, reg, src, count) == 0) {
            return 0;
        }
        k_msleep (2);
    }
    return -EIO;
}

static int read_multi (uint8_t reg, uint8_t* dst, uint8_t count) {
    for (int retry = 0; retry < 5; retry++) {
        if (i2c_burst_read (i2c_dev, VL53L0X_I2C_ADDR, reg, dst, count) == 0) {
            return 0;
        }
        k_msleep (2);
    }
    return -EIO;
}

static uint16_t decode_timeout (uint16_t reg_val) {
    return (uint16_t)((reg_val & 0x00FF) << (uint16_t)((reg_val & 0xFF00) >> 8)) + 1;
}

static uint16_t encode_timeout (uint32_t timeout_mclks) {
    uint32_t ls_byte = 0;
    uint16_t ms_byte = 0;

    if (timeout_mclks > 0) {
        ls_byte = timeout_mclks - 1;
        while ((ls_byte & 0xFFFFFF00) > 0) {
            ls_byte >>= 1;
            ms_byte++;
        }
        return (ms_byte << 8) | (ls_byte & 0xFF);
    }
    return 0;
}

static uint32_t timeout_mclks_to_us (uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks) {
    uint32_t macro_period_ns = calcMacroPeriod (vcsel_period_pclks);
    return ((timeout_period_mclks * macro_period_ns) + 500) / 1000;
}

static uint32_t timeout_us_to_mclks (uint32_t timeout_period_us, uint8_t vcsel_period_pclks) {
    uint32_t macro_period_ns = calcMacroPeriod (vcsel_period_pclks);
    return (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
}

static void get_sequence_step_enables (struct SequenceStepEnables* enables) {
    uint8_t sequence_config = 0;
    read_reg (REG_SYSTEM_SEQUENCE_CONFIG, &sequence_config);

    enables->tcc         = (sequence_config >> 4) & 0x1;
    enables->dss         = (sequence_config >> 3) & 0x1;
    enables->msrc        = (sequence_config >> 2) & 0x1;
    enables->pre_range   = (sequence_config >> 6) & 0x1;
    enables->final_range = (sequence_config >> 7) & 0x1;
}

static void get_sequence_step_timeouts (const struct SequenceStepEnables* enables,
struct SequenceStepTimeouts* timeouts) {
    uint8_t period = 0;
    uint16_t reg16 = 0;
    uint8_t reg8   = 0;

    read_reg (REG_PRE_RANGE_CONFIG_VCSEL_PERIOD, &period);
    timeouts->pre_range_vcsel_period_pclks = decodeVcselPeriod (period);

    read_reg (REG_MSRC_CONFIG_TIMEOUT_MACROP, &reg8);
    timeouts->msrc_dss_tcc_mclks = reg8 + 1;
    timeouts->msrc_dss_tcc_us    = timeout_mclks_to_us (
    timeouts->msrc_dss_tcc_mclks, timeouts->pre_range_vcsel_period_pclks);

    read_reg16 (REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI, &reg16);
    timeouts->pre_range_mclks = decode_timeout (reg16);
    timeouts->pre_range_us    = timeout_mclks_to_us (
    timeouts->pre_range_mclks, timeouts->pre_range_vcsel_period_pclks);

    read_reg (REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD, &period);
    timeouts->final_range_vcsel_period_pclks = decodeVcselPeriod (period);

    read_reg16 (REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI, &reg16);
    timeouts->final_range_mclks = decode_timeout (reg16);

    if (enables->pre_range) {
        timeouts->final_range_mclks -= timeouts->pre_range_mclks;
    }

    timeouts->final_range_us = timeout_mclks_to_us (
    timeouts->final_range_mclks, timeouts->final_range_vcsel_period_pclks);
}

static uint32_t get_measurement_timing_budget (void) {
    struct SequenceStepEnables enables;
    struct SequenceStepTimeouts timeouts;

    const uint16_t StartOverhead      = 1910;
    const uint16_t EndOverhead        = 960;
    const uint16_t MsrcOverhead       = 660;
    const uint16_t TccOverhead        = 590;
    const uint16_t DssOverhead        = 690;
    const uint16_t PreRangeOverhead   = 660;
    const uint16_t FinalRangeOverhead = 550;

    uint32_t budget_us = StartOverhead + EndOverhead;

    get_sequence_step_enables (&enables);
    get_sequence_step_timeouts (&enables, &timeouts);

    if (enables.tcc) {
        budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
    }
    if (enables.dss) {
        budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
    } else if (enables.msrc) {
        budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
    }
    if (enables.pre_range) {
        budget_us += (timeouts.pre_range_us + PreRangeOverhead);
    }
    if (enables.final_range) {
        budget_us += (timeouts.final_range_us + FinalRangeOverhead);
    }

    measurement_timing_budget_us = budget_us;
    return budget_us;
}

static bool set_measurement_timing_budget (uint32_t budget_us) {
    struct SequenceStepEnables enables;
    struct SequenceStepTimeouts timeouts;

    const uint16_t StartOverhead      = 1910;
    const uint16_t EndOverhead        = 960;
    const uint16_t MsrcOverhead       = 660;
    const uint16_t TccOverhead        = 590;
    const uint16_t DssOverhead        = 690;
    const uint16_t PreRangeOverhead   = 660;
    const uint16_t FinalRangeOverhead = 550;

    uint32_t used_budget_us = StartOverhead + EndOverhead;

    get_sequence_step_enables (&enables);
    get_sequence_step_timeouts (&enables, &timeouts);

    if (enables.tcc) {
        used_budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
    }
    if (enables.dss) {
        used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
    } else if (enables.msrc) {
        used_budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
    }
    if (enables.pre_range) {
        used_budget_us += (timeouts.pre_range_us + PreRangeOverhead);
    }
    if (enables.final_range) {
        used_budget_us += FinalRangeOverhead;

        if (used_budget_us > budget_us) {
            return false;
        }

        uint32_t final_range_timeout_us    = budget_us - used_budget_us;
        uint32_t final_range_timeout_mclks = timeout_us_to_mclks (
        final_range_timeout_us, timeouts.final_range_vcsel_period_pclks);

        if (enables.pre_range) {
            final_range_timeout_mclks += timeouts.pre_range_mclks;
        }

        write_reg16 (REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
        encode_timeout (final_range_timeout_mclks));

        measurement_timing_budget_us = budget_us;
    }
    return true;
}

static bool perform_single_ref_calibration (uint8_t vhv_init_byte) {
    write_reg (REG_SYSRANGE_START, 0x01 | vhv_init_byte);

    uint8_t status   = 0;
    uint8_t sysrange = 0;
    for (int i = 0; i < 50; i++) {
        read_reg (REG_RESULT_INTERRUPT_STATUS, &status);
        read_reg (REG_SYSRANGE_START, &sysrange);
        if (i < 5 || (i % 10 == 0)) {
            LOG_INF ("RefCal poll [%d]: sysrange=0x%02x, int_status=0x%02x", i,
            sysrange, status);
        }
        if ((status & 0x07) != 0) {
            LOG_INF ("RefCal complete at loop %d (status=0x%02x)!", i, status);
            break;
        }
        k_msleep (5);
    }

    write_reg (REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    write_reg (REG_SYSRANGE_START, 0x00);
    return (status & 0x07) != 0;
}

static bool get_spad_info (uint8_t* count, bool* type_is_aperture) {
    uint8_t tmp = 0;

    write_reg (0x80, 0x01);
    write_reg (0xFF, 0x01);
    write_reg (0x00, 0x00);

    write_reg (0xFF, 0x06);
    read_reg (0x83, &tmp);
    write_reg (0x83, tmp | 0x04);
    write_reg (0xFF, 0x07);
    write_reg (0x81, 0x01);

    write_reg (0x80, 0x01);

    write_reg (0x94, 0x6b);
    write_reg (0x83, 0x00);

    uint32_t start = k_uptime_get_32 ();
    tmp            = 0;
    while (tmp == 0x00) {
        read_reg (0x83, &tmp);
        if ((k_uptime_get_32 () - start) > 500) {
            LOG_ERR ("get_spad_info timeout");
            return false;
        }
        k_msleep (1);
    }

    write_reg (0x83, 0x01);
    read_reg (0x92, &tmp);

    *count            = tmp & 0x7f;
    *type_is_aperture = (tmp >> 7) & 0x01;

    write_reg (0x81, 0x00);
    write_reg (0xFF, 0x06);
    read_reg (0x83, &tmp);
    write_reg (0x83, tmp & ~0x04);
    write_reg (0xFF, 0x01);
    write_reg (0x00, 0x01);

    write_reg (0xFF, 0x00);
    write_reg (0x80, 0x00);

    return true;
}

int sensor_init (void) {
    uint8_t model_id = 0;
    uint8_t val      = 0;

    LOG_INF ("Initializing VL53L0X Time-of-Flight sensor...");

    i2c_dev = DEVICE_DT_GET (DT_NODELABEL (i2c0));
    if (!device_is_ready (i2c_dev)) {
        LOG_ERR ("I2C0 device not ready!");
        return -ENODEV;
    }

    /* Wait for sensor to finish internal power-on boot sequence */
    k_msleep (50);

    /* 1. Verify Model ID (0xEE) */
    for (int retry = 0; retry < 10; retry++) {
        if (read_reg (REG_IDENTIFICATION_MODEL_ID, &model_id) == 0 && model_id == 0xEE) {
            break;
        }
        k_msleep (10);
    }

    if (model_id != 0xEE) {
        LOG_ERR ("VL53L0X not found (Model ID: 0x%02X, expected 0xEE)", model_id);
        return -ENODEV;
    }
    LOG_INF ("VL53L0X identified (Model ID: 0x%02X)", model_id);

    /* Dump registers 0x00 to 0x1F at power on */
    for (uint8_t r = 0; r <= 0x1F; r++) {
        uint8_t b = 0;
        read_reg (r, &b);
        LOG_INF ("POR Reg 0x%02X = 0x%02X", r, b);
    }

    /* 2. Configure 2V8 I/O mode for 3.3V board supply */
    read_reg (REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, &val);
    write_reg (REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, val | 0x01);

    /* 3. Set I2C standard mode */
    write_reg (0x88, 0x00);

    /* 4. Read stop_variable */
    write_reg (0x80, 0x01);
    write_reg (0xFF, 0x01);
    write_reg (0x00, 0x00);
    read_reg (0x91, &stop_variable);
    write_reg (0x00, 0x01);
    write_reg (0xFF, 0x00);
    write_reg (0x80, 0x00);
    LOG_INF ("Stop variable: 0x%02X", stop_variable);

    /* 5. Disable SIGNAL_RATE_MSRC and PRE_RANGE limit checks */
    read_reg (REG_MSRC_CONFIG_CONTROL, &val);
    write_reg (REG_MSRC_CONFIG_CONTROL, val | 0x12);

    /* 6. Set final range signal rate limit to 0.25 MCPS (32 in Q9.7) */
    write_reg16 (REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, 32);

    write_reg (REG_SYSTEM_SEQUENCE_CONFIG, 0xFF);

    /* 7. Get SPAD info & configure reference SPADs */
    uint8_t spad_count         = 0;
    bool spad_type_is_aperture = false;
    if (!get_spad_info (&spad_count, &spad_type_is_aperture)) {
        LOG_ERR ("Failed to get SPAD info");
        return -EIO;
    }
    LOG_INF ("SPAD count: %d, is_aperture: %d", spad_count, spad_type_is_aperture);

    uint8_t ref_spad_map[6];
    read_multi (REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

    write_reg (0xFF, 0x01);
    write_reg (REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    write_reg (REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
    write_reg (0xFF, 0x00);
    write_reg (REG_GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4);

    uint8_t first_spad_to_enable = spad_type_is_aperture ? 12 : 0;
    uint8_t spads_enabled        = 0;
    for (uint8_t i = 0; i < 48; i++) {
        if (i < first_spad_to_enable || spads_enabled == spad_count) {
            ref_spad_map[i / 8] &= ~(1 << (i % 8));
        } else if ((ref_spad_map[i / 8] >> (i % 8)) & 0x1) {
            spads_enabled++;
        }
    }
    write_multi (REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

    /* 8. Load default tuning settings */
    write_reg (0xFF, 0x01);
    write_reg (0x00, 0x00);
    write_reg (0xFF, 0x00);
    write_reg (0x09, 0x00);
    write_reg (0x10, 0x00);
    write_reg (0x11, 0x00);
    write_reg (0x24, 0x01);
    write_reg (0x25, 0xFF);
    write_reg (0x75, 0x00);
    write_reg (0xFF, 0x01);
    write_reg (0x4E, 0x2C);
    write_reg (0x48, 0x00);
    write_reg (0x30, 0x20);
    write_reg (0xFF, 0x00);
    write_reg (0x30, 0x09);
    write_reg (0x54, 0x00);
    write_reg (0x31, 0x04);
    write_reg (0x32, 0x03);
    write_reg (0x40, 0x83);
    write_reg (0x46, 0x25);
    write_reg (0x60, 0x00);
    write_reg (0x27, 0x00);
    write_reg (0x50, 0x06);
    write_reg (0x51, 0x00);
    write_reg (0x52, 0x96);
    write_reg (0x56, 0x08);
    write_reg (0x57, 0x30);
    write_reg (0x61, 0x00);
    write_reg (0x62, 0x00);
    write_reg (0x64, 0x00);
    write_reg (0x65, 0x00);
    write_reg (0x66, 0xA0);
    write_reg (0xFF, 0x01);
    write_reg (0x22, 0x32);
    write_reg (0x47, 0x14);
    write_reg (0x49, 0xFF);
    write_reg (0x4A, 0x00);
    write_reg (0xFF, 0x00);
    write_reg (0x7A, 0x0A);
    write_reg (0x7B, 0x00);
    write_reg (0x78, 0x21);
    write_reg (0xFF, 0x01);
    write_reg (0x23, 0x34);
    write_reg (0x42, 0x00);
    write_reg (0x44, 0xFF);
    write_reg (0x45, 0x26);
    write_reg (0x46, 0x05);
    write_reg (0x40, 0x40);
    write_reg (0x0E, 0x06);
    write_reg (0x20, 0x1A);
    write_reg (0x43, 0x40);
    write_reg (0xFF, 0x00);
    write_reg (0x34, 0x03);
    write_reg (0x35, 0x44);
    write_reg (0xFF, 0x01);
    write_reg (0x31, 0x04);
    write_reg (0x4B, 0x09);
    write_reg (0x4C, 0x05);
    write_reg (0x4D, 0x04);
    write_reg (0xFF, 0x00);
    write_reg (0x44, 0x00);
    write_reg (0x45, 0x20);
    write_reg (0x47, 0x08);
    write_reg (0x48, 0x28);
    write_reg (0x67, 0x00);
    write_reg (0x70, 0x04);
    write_reg (0x71, 0x01);
    write_reg (0x72, 0xFE);
    write_reg (0x76, 0x00);
    write_reg (0x77, 0x00);
    write_reg (0xFF, 0x01);
    write_reg (0x0D, 0x01);
    write_reg (0xFF, 0x00);
    write_reg (0x80, 0x01);
    write_reg (0x01, 0xF8);
    write_reg (0xFF, 0x01);
    write_reg (0x8E, 0x01);
    write_reg (0x00, 0x01);
    write_reg (0xFF, 0x00);
    write_reg (0x80, 0x00);

    /* 9. Set interrupt config to new sample ready (active low) */
    write_reg (REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    read_reg (REG_GPIO_HV_MUX_ACTIVE_HIGH, &val);
    write_reg (REG_GPIO_HV_MUX_ACTIVE_HIGH, val & ~0x10);
    write_reg (REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    /* 10. Disable MSRC and TCC by default */
    write_reg (REG_SYSTEM_SEQUENCE_CONFIG, 0xE8);

    /* 11. Recalculate timing budget */
    uint32_t budget = get_measurement_timing_budget ();
    set_measurement_timing_budget (budget);
    LOG_INF ("Timing budget: %u us", budget);

    /* 12. Reference calibration (VHV & Phase) */
    write_reg (REG_SYSTEM_SEQUENCE_CONFIG, 0x01);
    if (!perform_single_ref_calibration (0x40)) {
        LOG_WRN ("VHV calibration skipped or timed out");
    }

    write_reg (REG_SYSTEM_SEQUENCE_CONFIG, 0x02);
    if (!perform_single_ref_calibration (0x00)) {
        LOG_WRN ("Phase calibration skipped or timed out");
    }

    /* Restore sequence config */
    write_reg (REG_SYSTEM_SEQUENCE_CONFIG, 0xE8);

    LOG_INF ("VL53L0X sensor successfully initialized!");
    return 0;
}

int sensor_read_distance (int32_t* distance_mm, bool* is_valid, uint8_t* status_code) {
    uint8_t int_status   = 0;
    uint8_t range_status = 0;
    uint16_t range       = 0;

    if (distance_mm == NULL || is_valid == NULL) {
        return -EINVAL;
    }

    *distance_mm = 0;
    *is_valid    = false;
    if (status_code) {
        *status_code = 0;
    }

    if (i2c_dev == NULL || !device_is_ready (i2c_dev)) {
        return -ENODEV;
    }

    /* Start single ranging measurement */
    write_reg (0x80, 0x01);
    write_reg (0xFF, 0x01);
    write_reg (0x00, 0x00);
    write_reg (0x91, stop_variable);
    write_reg (0x00, 0x01);
    write_reg (0xFF, 0x00);
    write_reg (0x80, 0x00);

    write_reg (REG_SYSRANGE_START, 0x01);

    /* Wait until start bit has cleared */
    uint8_t sysrange_start = 0x01;
    uint32_t start_time    = k_uptime_get_32 ();
    while (sysrange_start & 0x01) {
        read_reg (REG_SYSRANGE_START, &sysrange_start);
        if ((k_uptime_get_32 () - start_time) > 100) {
            LOG_WRN ("SYSRANGE_START clear timeout");
            return -ETIMEDOUT;
        }
        k_msleep (1);
    }

    /* Wait until measurement data ready */
    start_time     = k_uptime_get_32 ();
    int loop_count = 0;
    while ((int_status & 0x07) == 0) {
        read_reg (REG_RESULT_INTERRUPT_STATUS, &int_status);
        read_reg (REG_SYSRANGE_START, &sysrange_start);
        read_reg (REG_RESULT_RANGE_STATUS, &range_status);
        if (loop_count < 3 || (loop_count % 20 == 0)) {
            LOG_INF ("Ranging poll [%d]: sysrange=0x%02x, int_status=0x%02x, "
                     "range_status=0x%02x",
            loop_count, sysrange_start, int_status, range_status);
        }
        loop_count++;
        if ((k_uptime_get_32 () - start_time) > 200) {
            LOG_WRN ("Measurement timeout (int_status=0x%02X)", int_status);
            write_reg (REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
            return -ETIMEDOUT;
        }
        k_msleep (2);
    }

    /* Read raw distance in mm (registers 0x1E and 0x1F) */
    read_reg16 (REG_RESULT_RANGE_STATUS + 10, &range);
    read_reg (REG_RESULT_RANGE_STATUS, &range_status);

    /* Clear interrupt */
    write_reg (REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    if (status_code) {
        *status_code = (range_status >> 3) & 0x0F;
    }

    *distance_mm = (int32_t)range;

    /* Range status 0 indicates a valid reading */
    uint8_t r_status = (range_status >> 3) & 0x0F;
    if (r_status == 0 && range > 20 && range < 2000) {
        *is_valid = true;
    } else {
        *is_valid = false;
    }

    return 0;
}
