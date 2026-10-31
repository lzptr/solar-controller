#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register the lamp and debug commands and start the interactive console on
 * the configured ESP-IDF console (USB Serial/JTAG by default, or UART0). The
 * console runs in its own task; the commands go through the thread-safe lamp
 * API.
 */
esp_err_t cli_start(void);

#ifdef __cplusplus
}
#endif
