#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
} led_ring_rgbw_t;

/**
 * Create the SK6812 RGBW driver on the BSP data line and show the configured
 * power-up state. The setters below are not thread-safe; call them from one
 * task.
 */
esp_err_t led_ring_init(void);

/** Number of LEDs on the ring. */
size_t led_ring_count(void);

/** Set RGB on every LED, keeping each LED's white level. */
esp_err_t led_ring_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/** Set the white level on every LED, keeping each LED's RGB. */
esp_err_t led_ring_set_white(uint8_t w);

/** Set all four channels on every LED. */
esp_err_t led_ring_set_all(led_ring_rgbw_t color);

/** Set all four channels on one LED. */
esp_err_t led_ring_set_pixel(size_t index, led_ring_rgbw_t color);

/** Set the global brightness, 0-100 %, applied linearly to every channel. */
esp_err_t led_ring_set_brightness(uint8_t percent);

/** Switch the output on with the stored colours, or off. */
esp_err_t led_ring_set_on(bool on);

/** Stored colour of one LED, before brightness; zero for an invalid index. */
led_ring_rgbw_t led_ring_pixel(size_t index);

/** Channel values currently sent to one LED, after brightness and on/off. */
led_ring_rgbw_t led_ring_output(size_t index);

uint8_t led_ring_brightness(void);
bool led_ring_is_on(void);

#ifdef __cplusplus
}
#endif
