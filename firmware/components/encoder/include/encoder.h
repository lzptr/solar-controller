#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int steps;    /**< Detents turned since the last poll, clockwise positive */
  bool pressed; /**< A short press was released since the last poll */
} encoder_event_t;

/** Start quadrature decoding on the PCNT and set up the push button. */
esp_err_t encoder_init(void);

/**
 * Collect turns and debounce the button. Call every 10 ms from one task; the
 * debounce timing assumes that rate.
 */
encoder_event_t encoder_poll(void);

#ifdef __cplusplus
}
#endif
