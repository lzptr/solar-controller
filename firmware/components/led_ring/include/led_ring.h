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
 * Frame buffer for the SK6812 RGBW chain with a current limiter. Setters only
 * change the buffer; led_ring_show() sends it. Nothing here is thread-safe;
 * call it from one task.
 *
 * The limiter estimates the chain current from the channel values and scales
 * the whole frame down to fit the budget, then by the guard factor on top.
 */

/** Create the RMT driver on the BSP data line. All LEDs dark, output off. */
esp_err_t led_ring_init(void);

/** LEDs the frame buffer holds. */
size_t led_ring_max_count(void);

/** LEDs in use; the rest of the chain is sent dark. */
size_t led_ring_count(void);
esp_err_t led_ring_set_count(size_t count);

esp_err_t led_ring_set_pixel(size_t index, led_ring_rgbw_t color);
void led_ring_fill(led_ring_rgbw_t color);

/** With the output off, show() sends a dark frame and keeps the colours. */
void led_ring_set_output(bool on);

/** Current budget for the whole chain, including idle current. */
void led_ring_set_budget_ma(uint32_t budget_ma);

/** Extra scale on top of the estimate, 0-1000 permille. */
void led_ring_set_guard(uint16_t permille);

/** Send the frame. */
esp_err_t led_ring_show(void);

/** Stored colour of one LED; zero for an invalid index. */
led_ring_rgbw_t led_ring_pixel(size_t index);

/** Channel values show() sends to one LED, after the limiter. */
led_ring_rgbw_t led_ring_output(size_t index);

/** Scale show() applies, 0-1000 permille: the budget fit times the guard. */
uint16_t led_ring_scale(void);

/** Estimated chain current for what show() sends, idle current included. */
uint32_t led_ring_estimated_ma(void);

#ifdef __cplusplus
}
#endif
