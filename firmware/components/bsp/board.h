#pragma once

#include <stddef.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Configure board-owned GPIOs in their safe startup states. */
esp_err_t board_init(void);

/** SK6812 data line; idles low. Header pin 7 on the dev kit. */
gpio_num_t board_led_data_gpio(void);

/** Number of SK6812 RGBW LEDs chained on the data line. */
size_t board_led_count(void);

#ifdef __cplusplus
}
#endif
