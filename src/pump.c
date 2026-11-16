/*
 * Pump MOSFET Actuator & BOOT Button Controller Implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pump.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER (pump, LOG_LEVEL_INF);

#define SW0_NODE DT_ALIAS (sw0)
#define PUMP_NODE DT_ALIAS (pump_gate)

#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Devicetree alias sw0 is not defined or not okay!"
#endif

#if !DT_NODE_HAS_STATUS_OKAY(PUMP_NODE)
#error "Devicetree alias pump_gate is not defined or not okay!"
#endif

static const struct gpio_dt_spec button_spec = GPIO_DT_SPEC_GET (SW0_NODE, gpios);
static const struct gpio_dt_spec pump_spec = GPIO_DT_SPEC_GET (PUMP_NODE, gpios);

static struct gpio_callback button_cb_data;
static struct k_work_delayable debounce_work;
static pump_state_cb_t state_change_cb;

static bool pump_active            = false;
static int64_t last_toggle_time_ms = 0;

static void debounce_work_handler (struct k_work* work) {
    /* Verify button is logically active (pressed) */
    int val = gpio_pin_get_dt (&button_spec);
    if (val > 0) {
        int64_t now = k_uptime_get ();
        if ((now - last_toggle_time_ms) >= 200) {
            last_toggle_time_ms = now;
            pump_toggle ();
        }
    }
}

static void button_pressed_isr (const struct device* dev, struct gpio_callback* cb, uint32_t pins) {
    /* Schedule debounce filter in 20 ms */
    k_work_schedule (&debounce_work, K_MSEC (20));
}

int pump_init (pump_state_cb_t cb) {
    int ret;

    state_change_cb     = cb;
    pump_active         = false;
    last_toggle_time_ms = 0;

    /* 1. Configure Pump MOSFET gate (GPIO 3) as output, starting inactive (LOW) */
    if (!gpio_is_ready_dt (&pump_spec)) {
        LOG_ERR ("Pump gate GPIO device %s is not ready!", pump_spec.port->name);
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt (&pump_spec, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR ("Failed to configure pump gate pin: %d", ret);
        return ret;
    }

    LOG_INF ("Pump MOSFET gate initialized on %s pin %d (state: OFF)",
    pump_spec.port->name, pump_spec.pin);

    /* 2. Configure BOOT Button (GPIO 9) as input with pull-up & interrupt */
    if (!gpio_is_ready_dt (&button_spec)) {
        LOG_ERR ("Button GPIO device %s is not ready!", button_spec.port->name);
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt (&button_spec, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR ("Failed to configure button pin: %d", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt (&button_spec, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR ("Failed to configure button interrupt: %d", ret);
        return ret;
    }

    k_work_init_delayable (&debounce_work, debounce_work_handler);

    gpio_init_callback (&button_cb_data, button_pressed_isr, BIT (button_spec.pin));
    ret = gpio_add_callback (button_spec.port, &button_cb_data);
    if (ret < 0) {
        LOG_ERR ("Failed to add button callback: %d", ret);
        return ret;
    }

    LOG_INF (
    "BOOT button interrupt initialized on %s pin %d (active: LOW/Press)",
    button_spec.port->name, button_spec.pin);

    return 0;
}

int pump_set_state (bool on) {
    int ret = gpio_pin_set_dt (&pump_spec, on ? 1 : 0);
    if (ret < 0) {
        LOG_ERR ("Failed to set pump GPIO state to %d (err: %d)", on, ret);
        return ret;
    }

    pump_active = on;
    LOG_INF (">>> Pump State Changed: %s (GPIO %d = %s) <<<", on ? "ON" : "OFF",
    pump_spec.pin, on ? "HIGH" : "LOW");

    if (state_change_cb != NULL) {
        state_change_cb (pump_active);
    }

    return 0;
}

int pump_toggle (void) {
    return pump_set_state (!pump_active);
}

bool pump_is_on (void) {
    return pump_active;
}
