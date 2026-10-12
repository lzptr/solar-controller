#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register the LED ring commands and start the interactive console on the
 * configured ESP-IDF console (USB Serial/JTAG by default, or UART0).
 */
esp_err_t led_cli_start(void);

#ifdef __cplusplus
}
#endif
