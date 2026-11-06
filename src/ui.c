/*
 * UI Module for 01Space ESP32-C3 0.42" OLED (SSD1306 72x40)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui.h"

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "font6x8.h"

LOG_MODULE_REGISTER (ui, LOG_LEVEL_INF);

#define DISPLAY_NODE DT_CHOSEN (zephyr_display)

static const struct device* display_dev = DEVICE_DT_GET (DISPLAY_NODE);

static uint16_t x_res;
static uint16_t y_res;
static int font_small_idx = -1;
static int font_large_idx = -1;

/* Helper to draw a filled box using inversion */
static void
draw_filled_box (const struct device* dev, int16_t x, int16_t y, int16_t w, int16_t h) {
    cfb_invert_area (dev, x, y, w, h);
}

/* Scene 1: Welcome & Splash Screen */
void ui_demo_splash (void) {
    LOG_INF ("[Scene 1] Splash Screen");

    cfb_framebuffer_clear (display_dev, false);

    /* Outer frame border */
    struct cfb_position p1 = { 0, 0 };
    struct cfb_position p2 = { x_res - 1, y_res - 1 };
    cfb_draw_rect (display_dev, &p1, &p2);

    /* Banner text */
    if (font_large_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_large_idx);
        cfb_draw_text (display_dev, "NIDIA", 6, 4);
    }

    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, "ESP32-C3 OLED", 4, 26);
    }

    cfb_framebuffer_finalize (display_dev);
    k_sleep (K_MSEC (2500));
}

/* Scene 2: Expanding Nested Boxes & Frames */
void ui_demo_boxes (void) {
    LOG_INF ("[Scene 2] Boxes and Frames");

    for (int step = 0; step < 8; step++) {
        cfb_framebuffer_clear (display_dev, false);

        int16_t margin         = step * 2;
        struct cfb_position tl = { margin, margin };
        struct cfb_position br = { x_res - 1 - margin, y_res - 1 - margin };
        cfb_draw_rect (display_dev, &tl, &br);

        /* Inner solid box */
        if (step > 2) {
            draw_filled_box (display_dev, (x_res / 2) - 6, (y_res / 2) - 4, 12, 8);
        }

        if (font_small_idx >= 0) {
            cfb_framebuffer_set_font (display_dev, font_small_idx);
            cfb_draw_text (display_dev, "FERNANDEZ", 12, 2);
        }

        cfb_framebuffer_finalize (display_dev);
        k_sleep (K_MSEC (200));
    }
    k_sleep (K_MSEC (1000));
}

/* Scene 3: Concentric Circles */
void ui_demo_circles (void) {
    LOG_INF ("[Scene 3] Concentric Circles");

    struct cfb_position center = { x_res / 2, y_res / 2 };

    for (int max_r = 4; max_r <= 18; max_r += 3) {
        cfb_framebuffer_clear (display_dev, false);

        for (int r = 3; r <= max_r; r += 4) {
            cfb_draw_circle (display_dev, &center, r);
        }

        if (font_small_idx >= 0) {
            cfb_framebuffer_set_font (display_dev, font_small_idx);
            cfb_draw_text (display_dev, "ROS", 16, 1);
        }

        cfb_framebuffer_finalize (display_dev);
        k_sleep (K_MSEC (250));
    }
    k_sleep (K_MSEC (1200));
}

/* Scene 4: Fan & Grid Lines */
void ui_demo_lines (void) {
    LOG_INF ("[Scene 4] Fan Lines");

    for (int i = 0; i <= x_res; i += 12) {
        cfb_framebuffer_clear (display_dev, false);

        struct cfb_position top    = { i, 0 };
        struct cfb_position bot    = { x_res - i, y_res - 1 };
        struct cfb_position origin = { 0, y_res - 1 };

        cfb_draw_line (display_dev, &origin, &top);
        cfb_draw_line (display_dev, &origin, &bot);

        struct cfb_position corner_r = { x_res - 1, 0 };
        cfb_draw_line (display_dev, &corner_r, &bot);

        if (font_small_idx >= 0) {
            cfb_framebuffer_set_font (display_dev, font_small_idx);
            cfb_draw_text (display_dev, "LA", 38, 2);
        }

        cfb_framebuffer_finalize (display_dev);
        k_sleep (K_MSEC (150));
    }
    k_sleep (K_MSEC (800));
}

/* Scene 5: Live Stats and Multi-line Text */
void ui_demo_text_stats (void) {
    LOG_INF ("[Scene 5] Text and System Stats");

    if (font_small_idx < 0) {
        return;
    }

    cfb_framebuffer_set_font (display_dev, font_small_idx);

    for (int count = 0; count < 10; count++) {
        cfb_framebuffer_clear (display_dev, false);

        uint32_t uptime_sec = (uint32_t)(k_uptime_get () / 1000);

        char buf[20];
        cfb_draw_text (display_dev, "ESP32-C3", 2, 1);
        cfb_draw_text (display_dev, "72x40 OLED", 2, 11);

        snprintf (buf, sizeof (buf), "Uptime: %us", uptime_sec);
        cfb_draw_text (display_dev, buf, 2, 21);

        snprintf (buf, sizeof (buf), "Tick: %d", count);
        cfb_draw_text (display_dev, buf, 2, 31);

        /* Right decorative vertical bar */
        struct cfb_position p1 = { x_res - 3, 0 };
        struct cfb_position p2 = { x_res - 3, y_res - 1 };
        cfb_draw_line (display_dev, &p1, &p2);

        cfb_framebuffer_finalize (display_dev);
        k_sleep (K_MSEC (300));
    }
}

/* Scene 6: Inversion & Split Screen */
void ui_demo_inversion (void) {
    LOG_INF ("[Scene 6] Invert Modes");

    cfb_framebuffer_clear (display_dev, false);

    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, "LA", 18, 4);
        cfb_draw_text (display_dev, "BESTIA", 24, 18);
    }

    /* Half-screen inverted block */
    draw_filled_box (display_dev, 0, 28, x_res, 12);
    cfb_framebuffer_finalize (display_dev);
    k_sleep (K_MSEC (1000));

    /* Invert entire display */
    cfb_framebuffer_invert (display_dev);
    cfb_framebuffer_finalize (display_dev);
    k_sleep (K_MSEC (800));

    /* Revert back */
    cfb_framebuffer_invert (display_dev);
    cfb_framebuffer_finalize (display_dev);
    k_sleep (K_MSEC (400));
}

/* Scene 7: Real-time Bouncing Box Screensaver */
void ui_demo_bouncing_box (void) {
    LOG_INF ("[Scene 7] Bouncing Box Screensaver");

    int16_t box_size = 8;
    int16_t x = 4, y = 4;
    int16_t dx = 2, dy = 1;

    for (int frame = 0; frame < 60; frame++) {
        cfb_framebuffer_clear (display_dev, false);

        /* Draw bouncing filled box */
        draw_filled_box (display_dev, x, y, box_size, box_size);

        /* Draw screen border */
        struct cfb_position tl = { 0, 0 };
        struct cfb_position br = { x_res - 1, y_res - 1 };
        cfb_draw_rect (display_dev, &tl, &br);

        /* Update position */
        x += dx;
        y += dy;

        if (x <= 1 || (x + box_size) >= (x_res - 1)) {
            dx = -dx;
        }
        if (y <= 1 || (y + box_size) >= (y_res - 1)) {
            dy = -dy;
        }

        cfb_framebuffer_finalize (display_dev);
        k_sleep (K_MSEC (40)); /* ~25 fps */
    }
}

void ui_run_demo_cycle (void) {
    ui_demo_splash ();
    ui_demo_boxes ();
    ui_demo_circles ();
    ui_demo_lines ();
    ui_demo_inversion ();
    ui_demo_bouncing_box ();
    ui_demo_text_stats ();
}

void ui_show_distance (int32_t distance_mm, bool is_valid) {
    cfb_framebuffer_clear (display_dev, false);

    /* 1. Header (6x8 font): "DISTANCE" centered */
    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, "DISTANCE", 12, 1);
    }

    /* Divider line below header */
    struct cfb_position d1 = { 0, 10 };
    struct cfb_position d2 = { x_res - 1, 10 };
    cfb_draw_line (display_dev, &d1, &d2);

    /* 2. Middle Value Readout */
    char val_str[16];
    if (is_valid) {
        snprintf (val_str, sizeof (val_str), "%d mm", (int)distance_mm);
    } else {
        snprintf (val_str, sizeof (val_str), "--- mm");
    }

    if (font_large_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_large_idx);
        int16_t len   = (int16_t)strlen (val_str);
        int16_t x_pos = (x_res - len * 10) / 2;
        if (x_pos < 0) {
            x_pos = 0;
        }
        cfb_draw_text (display_dev, val_str, x_pos, 13);
    } else if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, val_str, 16, 15);
    }

    /* 3. Bottom Distance Gauge Bar (0 - 1000 mm scale) */
    struct cfb_position g_tl = { 3, 31 };
    struct cfb_position g_br = { x_res - 4, 38 };
    cfb_draw_rect (display_dev, &g_tl, &g_br);

    if (is_valid && distance_mm > 0) {
        int32_t bar_w = (distance_mm * (x_res - 8)) / 1000;
        if (bar_w > (x_res - 8)) {
            bar_w = x_res - 8;
        }
        if (bar_w < 1) {
            bar_w = 1;
        }
        draw_filled_box (display_dev, 4, 32, (int16_t)bar_w, 6);
    }

    cfb_framebuffer_finalize (display_dev);
}

void ui_show_env (int32_t temp_int, int32_t temp_dec, int32_t hum_int, int32_t hum_dec, bool is_valid) {
    cfb_framebuffer_clear (display_dev, false);

    /* 1. Header (6x8 font): "ENV SENSOR" centered at y = 1 */
    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, "ENV SENSOR", 6, 1);
    }

    /* Divider line below header */
    struct cfb_position d1 = { 0, 10 };
    struct cfb_position d2 = { x_res - 1, 10 };
    cfb_draw_line (display_dev, &d1, &d2);

    /* 2. Middle Value Readouts */
    char t_str[16];
    char h_str[16];
    if (is_valid) {
        snprintf (t_str, sizeof (t_str), "T: %2d.%d C", (int)temp_int, (int)temp_dec);
        snprintf (h_str, sizeof (h_str), "H: %2d.%d %%", (int)hum_int, (int)hum_dec);
    } else {
        snprintf (t_str, sizeof (t_str), "T: --.- C");
        snprintf (h_str, sizeof (h_str), "H: --.- %%");
    }

    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, t_str, 6, 13);
        cfb_draw_text (display_dev, h_str, 6, 22);
    }

    /* 3. Bottom Humidity Bar (0 - 100% scale) */
    struct cfb_position g_tl = { 3, 32 };
    struct cfb_position g_br = { x_res - 4, 38 };
    cfb_draw_rect (display_dev, &g_tl, &g_br);

    if (is_valid && hum_int > 0) {
        int32_t max_bar_w = x_res - 8;
        int32_t bar_w = (hum_int * max_bar_w) / 100;
        if (bar_w > max_bar_w) {
            bar_w = max_bar_w;
        }
        if (bar_w < 1) {
            bar_w = 1;
        }
        draw_filled_box (display_dev, 4, 33, (int16_t)bar_w, 5);
    }

    cfb_framebuffer_finalize (display_dev);
}

void ui_show_dashboard (const struct env_reading* env, const struct power_reading* pwr) {
    cfb_framebuffer_clear (display_dev, false);

    /* Line 0 (y = 1): Environmental summary: " 22.1C 40% " */
    char env_str[16];
    if (env && env->is_valid) {
        snprintf (env_str, sizeof (env_str), " %2d.%dC %2d%%",
                  (int)env->temp_c_int, (int)env->temp_c_dec,
                  (int)env->hum_int);
    } else {
        snprintf (env_str, sizeof (env_str), " --.-C --%%");
    }

    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, env_str, 2, 1);
    }

    /* Divider line below header */
    struct cfb_position d1 = { 0, 10 };
    struct cfb_position d2 = { x_res - 1, 10 };
    cfb_draw_line (display_dev, &d1, &d2);

    /* Line 1 (y = 12): Bus Voltage */
    /* Line 2 (y = 21): Bus Current */
    /* Line 3 (y = 30): Power */
    char v_str[24];
    char i_str[24];
    char p_str[24];

    if (pwr && pwr->is_valid) {
        int v_volts = pwr->bus_voltage_mv / 1000;
        int v_cents = (pwr->bus_voltage_mv % 1000) / 10;
        if (v_cents < 0) {
            v_cents = -v_cents;
        }
        snprintf (v_str, sizeof (v_str), "V: %2d.%02d V", v_volts, v_cents);

        if (pwr->current_ma >= 1000 || pwr->current_ma <= -1000) {
            int i_amps = pwr->current_ma / 1000;
            int i_cents = (pwr->current_ma % 1000) / 10;
            if (i_cents < 0) {
                i_cents = -i_cents;
            }
            snprintf (i_str, sizeof (i_str), "I: %2d.%02d A", i_amps, i_cents);
        } else {
            snprintf (i_str, sizeof (i_str), "I: %4d mA", (int)pwr->current_ma);
        }

        if (pwr->power_mw >= 1000) {
            int p_watts = pwr->power_mw / 1000;
            int p_cents = (pwr->power_mw % 1000) / 10;
            if (p_cents < 0) {
                p_cents = -p_cents;
            }
            snprintf (p_str, sizeof (p_str), "P: %2d.%02d W", p_watts, p_cents);
        } else {
            snprintf (p_str, sizeof (p_str), "P: %4d mW", (int)pwr->power_mw);
        }
    } else {
        snprintf (v_str, sizeof (v_str), "V: --.-- V");
        snprintf (i_str, sizeof (i_str), "I: ---- mA");
        snprintf (p_str, sizeof (p_str), "P: ---- mW");
    }

    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (display_dev, font_small_idx);
        cfb_draw_text (display_dev, v_str, 2, 12);
        cfb_draw_text (display_dev, i_str, 2, 21);
        cfb_draw_text (display_dev, p_str, 2, 30);
    }

    cfb_framebuffer_finalize (display_dev);
}

int ui_init (void) {
    int ret;

    LOG_INF ("Initializing 01Space ESP32-C3 0.42\" OLED UI...");

    if (!device_is_ready (display_dev)) {
        LOG_ERR ("Display device %s is not ready!", display_dev->name);
        return -ENODEV;
    }

    LOG_INF ("Display device %s is ready", display_dev->name);

    /* Set monochrome pixel format (MONO10 or MONO01) */
    ret = display_set_pixel_format (display_dev, PIXEL_FORMAT_MONO10);
    if (ret != 0) {
        ret = display_set_pixel_format (display_dev, PIXEL_FORMAT_MONO01);
        if (ret != 0) {
            LOG_ERR ("Failed to set pixel format: %d", ret);
            return ret;
        }
    }

    /* Initialize Character Framebuffer (CFB) */
    ret = cfb_framebuffer_init (display_dev);
    if (ret != 0) {
        LOG_ERR ("CFB initialization failed: %d", ret);
        return ret;
    }

    cfb_framebuffer_clear (display_dev, true);
    display_blanking_off (display_dev);

    x_res = cfb_get_display_parameter (display_dev, CFB_DISPLAY_WIDTH);
    y_res = cfb_get_display_parameter (display_dev, CFB_DISPLAY_HEIGHT);
    LOG_INF ("Display initialized: %d x %d pixels", x_res, y_res);

    /* Discover registered fonts */
    int num_fonts = cfb_get_numof_fonts (display_dev);
    LOG_INF ("Found %d registered fonts:", num_fonts);

    for (int idx = 0; idx < num_fonts; idx++) {
        uint8_t fw, fh;
        if (cfb_get_font_size (display_dev, idx, &fw, &fh) == 0) {
            LOG_INF ("  Font [%d]: %dx%d pixels", idx, fw, fh);
            if (fw <= 8 && fh <= 10) {
                font_small_idx = idx;
            } else if (fw <= 12 && fh <= 16) {
                font_large_idx = idx;
            }
        }
    }

    /* Fallback if needed */
    if (font_small_idx < 0) {
        font_small_idx = 0;
    }
    if (font_large_idx < 0) {
        font_large_idx = 0;
    }

    LOG_INF ("Selected Small Font: %d, Large Font: %d", font_small_idx, font_large_idx);

    return 0;
}
