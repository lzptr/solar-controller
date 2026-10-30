#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Solar controller rev 0.1 (hardware/solar_controller), U301
 * ESP32-C6-MINI-1U-H4. Net names are the schematic's.
 */

/** MCU_LDATA: SK6812 data through U403, which is powered from the switched
 *  +5V, so data only reaches J401 while LED power is on. */
#define BOARD_GPIO_LED_DATA GPIO_NUM_7

/** LED_PWR_EN: U402 (TPS259531) EN/UVLO via R406 / R407; high = LED power on,
 *  pulled low at reset. */
#define BOARD_GPIO_LED_PWR_EN GPIO_NUM_5

/** LED_FAULT_N: U402 FLT, open drain with R404 pull-up. Low only after a
 *  thermal shutdown; overcurrent is current-limited without FLT. */
#define BOARD_GPIO_LED_FAULT_N GPIO_NUM_1

/** LED_ISENSE: U402 ILM across R403 (1 kOhm), filtered by R414 / C408. */
#define BOARD_GPIO_LED_ISENSE GPIO_NUM_0

/** ILM voltage per amp of LED current: G_IMON 276 uA/A x R403 1 kOhm. U402's
 *  current limit with R403 is 2000 / 1000 + 0.04 = 2.04 A. */
#define BOARD_ILM_MV_PER_A 276

/** Encoder SW402: A and B with 2k2 pull-ups, push button active low with a
 *  10k pull-up. No hardware debounce. */
#define BOARD_GPIO_ENC_A GPIO_NUM_3
#define BOARD_GPIO_ENC_B GPIO_NUM_2
#define BOARD_GPIO_ENC_SW_N GPIO_NUM_6

/** LED_ACT: status LED D301, high = on. */
#define BOARD_GPIO_STATUS_LED GPIO_NUM_19

/** SK6812 RGBW LEDs the data line is sized for. */
#define BOARD_LED_MAX CONFIG_BSP_LED_MAX

/**
 * Put the board-owned GPIOs in their safe startup states: LED power off,
 * status LED off, LED data held low until the RMT driver claims it.
 */
esp_err_t board_init(void);

#ifdef __cplusplus
}
#endif
