/*
 * OLED Graphics Demo for 01Space ESP32-C3 0.42" OLED (SSD1306 72x40)
 *
 * Demonstrates Zephyr's Character Framebuffer (CFB) subsystem and graphics
 * primitives: lines, rectangles, circles, custom fonts, inversion, and
 * real-time animations.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "font6x8.h"

LOG_MODULE_REGISTER (oled_demo, LOG_LEVEL_INF);

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
static void demo_splash (const struct device* dev) {
    LOG_INF ("[Scene 1] Splash Screen");

    cfb_framebuffer_clear (dev, false);

    /* Outer frame border */
    struct cfb_position p1 = { 0, 0 };
    struct cfb_position p2 = { x_res - 1, y_res - 1 };
    cfb_draw_rect (dev, &p1, &p2);

    /* Banner text */
    if (font_large_idx >= 0) {
        cfb_framebuffer_set_font (dev, font_large_idx);
        cfb_draw_text (dev, "NIDIA", 6, 4);
    }

    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (dev, font_small_idx);
        cfb_draw_text (dev, "ESP32-C3 OLED", 4, 26);
    }

    cfb_framebuffer_finalize (dev);
    k_sleep (K_MSEC (2500));
}

/* Scene 2: Expanding Nested Boxes & Frames */
static void demo_boxes (const struct device* dev) {
    LOG_INF ("[Scene 2] Boxes and Frames");

    for (int step = 0; step < 8; step++) {
        cfb_framebuffer_clear (dev, false);

        int16_t margin         = step * 2;
        struct cfb_position tl = { margin, margin };
        struct cfb_position br = { x_res - 1 - margin, y_res - 1 - margin };
        cfb_draw_rect (dev, &tl, &br);

        /* Inner solid box */
        if (step > 2) {
            draw_filled_box (dev, (x_res / 2) - 6, (y_res / 2) - 4, 12, 8);
        }

        if (font_small_idx >= 0) {
            cfb_framebuffer_set_font (dev, font_small_idx);
            cfb_draw_text (dev, "FERNANDEZ", 12, 2);
        }

        cfb_framebuffer_finalize (dev);
        k_sleep (K_MSEC (200));
    }
    k_sleep (K_MSEC (1000));
}

/* Scene 3: Concentric Circles */
static void demo_circles (const struct device* dev) {
    LOG_INF ("[Scene 3] Concentric Circles");

    struct cfb_position center = { x_res / 2, y_res / 2 };

    for (int max_r = 4; max_r <= 18; max_r += 3) {
        cfb_framebuffer_clear (dev, false);

        for (int r = 3; r <= max_r; r += 4) {
            cfb_draw_circle (dev, &center, r);
        }

        if (font_small_idx >= 0) {
            cfb_framebuffer_set_font (dev, font_small_idx);
            cfb_draw_text (dev, "ROS", 16, 1);
        }

        cfb_framebuffer_finalize (dev);
        k_sleep (K_MSEC (250));
    }
    k_sleep (K_MSEC (1200));
}

/* Scene 4: Fan & Grid Lines */
static void demo_lines (const struct device* dev) {
    LOG_INF ("[Scene 4] Fan Lines");

    for (int i = 0; i <= x_res; i += 12) {
        cfb_framebuffer_clear (dev, false);

        struct cfb_position top    = { i, 0 };
        struct cfb_position bot    = { x_res - i, y_res - 1 };
        struct cfb_position origin = { 0, y_res - 1 };

        cfb_draw_line (dev, &origin, &top);
        cfb_draw_line (dev, &origin, &bot);

        struct cfb_position corner_r = { x_res - 1, 0 };
        cfb_draw_line (dev, &corner_r, &bot);

        if (font_small_idx >= 0) {
            cfb_framebuffer_set_font (dev, font_small_idx);
            cfb_draw_text (dev, "LA", 38, 2);
        }

        cfb_framebuffer_finalize (dev);
        k_sleep (K_MSEC (150));
    }
    k_sleep (K_MSEC (800));
}

/* Scene 5: Live Stats and Multi-line Text */
static void demo_text_stats (const struct device* dev) {
    LOG_INF ("[Scene 5] Text and System Stats");

    if (font_small_idx < 0) {
        return;
    }

    cfb_framebuffer_set_font (dev, font_small_idx);

    for (int count = 0; count < 10; count++) {
        cfb_framebuffer_clear (dev, false);

        uint32_t uptime_sec = (uint32_t)(k_uptime_get () / 1000);

        char buf[20];
        cfb_draw_text (dev, "ESP32-C3", 2, 1);
        cfb_draw_text (dev, "72x40 OLED", 2, 11);

        snprintf (buf, sizeof (buf), "Uptime: %us", uptime_sec);
        cfb_draw_text (dev, buf, 2, 21);

        snprintf (buf, sizeof (buf), "Tick: %d", count);
        cfb_draw_text (dev, buf, 2, 31);

        /* Right decorative vertical bar */
        struct cfb_position p1 = { x_res - 3, 0 };
        struct cfb_position p2 = { x_res - 3, y_res - 1 };
        cfb_draw_line (dev, &p1, &p2);

        cfb_framebuffer_finalize (dev);
        k_sleep (K_MSEC (300));
    }
}

/* Scene 6: Inversion & Split Screen */
static void demo_inversion (const struct device* dev) {
    LOG_INF ("[Scene 6] Invert Modes");

    cfb_framebuffer_clear (dev, false);

    if (font_small_idx >= 0) {
        cfb_framebuffer_set_font (dev, font_small_idx);
        cfb_draw_text (dev, "LA", 18, 4);
        cfb_draw_text (dev, "BESTIA", 24, 18);
    }

    /* Half-screen inverted block */
    draw_filled_box (dev, 0, 28, x_res, 12);
    cfb_framebuffer_finalize (dev);
    k_sleep (K_MSEC (1000));

    /* Invert entire display */
    cfb_framebuffer_invert (dev);
    cfb_framebuffer_finalize (dev);
    k_sleep (K_MSEC (800));

    /* Revert back */
    cfb_framebuffer_invert (dev);
    cfb_framebuffer_finalize (dev);
    k_sleep (K_MSEC (400));
}

/* Scene 7: Real-time Bouncing Box Screensaver */
static void demo_bouncing_box (const struct device* dev) {
    LOG_INF ("[Scene 7] Bouncing Box Screensaver");

    int16_t box_size = 8;
    int16_t x = 4, y = 4;
    int16_t dx = 2, dy = 1;

    for (int frame = 0; frame < 60; frame++) {
        cfb_framebuffer_clear (dev, false);

        /* Draw bouncing filled box */
        draw_filled_box (dev, x, y, box_size, box_size);

        /* Draw screen border */
        struct cfb_position tl = { 0, 0 };
        struct cfb_position br = { x_res - 1, y_res - 1 };
        cfb_draw_rect (dev, &tl, &br);

        /* Update position */
        x += dx;
        y += dy;

        if (x <= 1 || (x + box_size) >= (x_res - 1)) {
            dx = -dx;
        }
        if (y <= 1 || (y + box_size) >= (y_res - 1)) {
            dy = -dy;
        }

        cfb_framebuffer_finalize (dev);
        k_sleep (K_MSEC (40)); /* ~25 fps */
    }
}

int main (void) {
    int ret;

    LOG_INF ("Starting 01Space ESP32-C3 0.42\" OLED Graphics Demo...");

    if (!device_is_ready (display_dev)) {
        LOG_ERR ("Display device %s is not ready!", display_dev->name);
        return 0;
    }

    LOG_INF ("Display device %s is ready", display_dev->name);

    /* Set monochrome pixel format (MONO10 or MONO01) */
    ret = display_set_pixel_format (display_dev, PIXEL_FORMAT_MONO10);
    if (ret != 0) {
        ret = display_set_pixel_format (display_dev, PIXEL_FORMAT_MONO01);
        if (ret != 0) {
            LOG_ERR ("Failed to set pixel format: %d", ret);
            return 0;
        }
    }

    /* Initialize Character Framebuffer (CFB) */
    ret = cfb_framebuffer_init (display_dev);
    if (ret != 0) {
        LOG_ERR ("CFB initialization failed: %d", ret);
        return 0;
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

    /* Main Demo Loop */
    while (1) {
        demo_splash (display_dev);
        demo_boxes (display_dev);
        demo_circles (display_dev);
        demo_lines (display_dev);
        demo_inversion (display_dev);
        demo_bouncing_box (display_dev);
        demo_text_stats (display_dev);
    }

    return 0;
}
