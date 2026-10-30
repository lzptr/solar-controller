#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int mv; /**< ILM voltage after the R414 / C408 filter */
  int ma; /**< LED current derived from it */
} led_power_sample_t;

/**
 * Set up the ILM current measurement. The enable and fault pins are
 * configured by board_init(); LED power stays off. Not thread-safe; use from
 * one task.
 */
esp_err_t led_power_init(void);

/** Drive the TPS259531 EN pin. Re-enabling also clears a latched fault. */
void led_power_enable(bool on);

bool led_power_enabled(void);

/** True while FLT is low (thermal shutdown). */
bool led_power_fault(void);

/** Average a few ILM readings. */
esp_err_t led_power_measure(led_power_sample_t *sample);

#ifdef __cplusplus
}
#endif
