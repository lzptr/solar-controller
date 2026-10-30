#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "led_ring.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Sun, Mercury ... Neptune, Pluto, in chain order from the controller. */
#define LAMP_GLOBE_COUNT 10
#define LAMP_LEDS_PER_GLOBE 7

#define LAMP_BUDGET_MIN_MA 50

typedef enum {
  LAMP_MODE_COLOUR, /**< Each globe in its own colour */
  LAMP_MODE_WHITE,  /**< Every globe in the Sun's built-in warm white */
  LAMP_MODE_OFF,    /**< LED power off; brightness is kept */
} lamp_mode_t;

typedef enum {
  LAMP_FAULT_NONE,
  LAMP_FAULT_OVERTEMP,    /**< eFuse FLT: thermal shutdown */
  LAMP_FAULT_OVERCURRENT, /**< Measured current at the eFuse limit */
} lamp_fault_t;

typedef struct {
  uint16_t brightness_permille; /**< 0 = off */
  lamp_mode_t mode;
  bool manual; /**< Debug frame from the CLI instead of the scene */

  bool power_enabled; /**< eFuse EN */
  bool power_ramping; /**< EN on, waiting before sending data */
  bool flt_low;       /**< eFuse FLT pin right now */
  lamp_fault_t fault; /**< Latched; power stays off until cleared */

  size_t led_count;
  uint32_t budget_ma;
  uint32_t estimated_ma;
  int measured_ma;
  int measured_mv;
  uint16_t scale_permille; /**< Limiter scale sent, guard included */
  uint16_t guard_permille; /**< Measured-current guard alone */
} lamp_status_t;

/**
 * Load the settings from NVS, start the LED, power and encoder drivers and
 * the lamp task. LED power comes back on if the saved brightness is above 0.
 * Every function below is thread-safe.
 */
esp_err_t lamp_start(void);

void lamp_get_status(lamp_status_t *status);
const char *lamp_fault_name(lamp_fault_t fault);
uint32_t lamp_budget_max_ma(void);

const char *lamp_globe_name(size_t globe);
led_ring_rgbw_t lamp_globe_color(size_t globe);
const char *lamp_mode_name(lamp_mode_t mode);

/** Stored and sent colour of one LED. */
esp_err_t lamp_led(size_t index, led_ring_rgbw_t *stored,
                   led_ring_rgbw_t *sent);

/*
 * Scene settings. Brightness and mode are saved to NVS a few seconds after
 * the last change; the rest only by lamp_save_config(). Brightness, mode and
 * colour changes leave manual mode.
 */

/** 0-1000 permille of full; 0 switches LED power off. */
esp_err_t lamp_set_brightness(uint16_t permille);
esp_err_t lamp_set_mode(lamp_mode_t mode);
esp_err_t lamp_set_globe_color(size_t globe, led_ring_rgbw_t color);
esp_err_t lamp_set_led_count(size_t count);
esp_err_t lamp_set_budget_ma(uint32_t budget_ma);

/** Save globe colours, LED count and budget. */
esp_err_t lamp_save_config(void);

/** Back to the built-in colours, LED count and budget; not saved. */
void lamp_restore_defaults(void);

/*
 * Debug frame. The first call switches to manual mode starting from the
 * current scene frame. Fill and pixel switch LED power on. The current limit
 * still applies.
 */
esp_err_t lamp_manual_fill(led_ring_rgbw_t color);
esp_err_t lamp_manual_pixel(size_t index, led_ring_rgbw_t color);

/** Drive LED power directly; switching on clears a latched fault. */
void lamp_manual_power(bool on);

/** Leave manual mode and show the scene again. */
void lamp_resume_scene(void);

#ifdef __cplusplus
}
#endif
