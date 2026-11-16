/*
 * Pump MOSFET Actuator & BOOT Button Controller
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_PUMP_H_
#define APP_PUMP_H_

#include <stdbool.h>

/**
 * @brief Callback invoked when pump state changes.
 *
 * @param is_on True if pump turned ON, false if turned OFF.
 */
typedef void (*pump_state_cb_t)(bool is_on);

/**
 * @brief Initialize the pump MOSFET gate GPIO and the BOOT button interrupt.
 *
 * @param cb Optional callback function called when button toggles the pump.
 * @return 0 on success, negative errno on failure.
 */
int pump_init(pump_state_cb_t cb);

/**
 * @brief Set the pump state.
 *
 * @param on True to turn pump ON (active high), false to turn pump OFF (inactive).
 * @return 0 on success, negative errno on failure.
 */
int pump_set_state(bool on);

/**
 * @brief Toggle current pump state (ON -> OFF or OFF -> ON).
 *
 * @return 0 on success, negative errno on failure.
 */
int pump_toggle(void);

/**
 * @brief Query current pump state.
 *
 * @return True if ON, false if OFF.
 */
bool pump_is_on(void);

#endif /* APP_PUMP_H_ */
