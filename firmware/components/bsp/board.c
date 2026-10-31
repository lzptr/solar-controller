#include "board.h"

#include <stdbool.h>

#include "esp_check.h"

static const char *TAG = "bsp";
static bool initialized;

esp_err_t board_init(void) {
  if (initialized) {
    return ESP_OK;
  }

  // Latch the outputs low before enabling the drivers so neither pin glitches
  // high. R407 already holds LED_PWR_EN low through reset.
  ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_GPIO_LED_PWR_EN, 0), TAG,
                      "LED power enable level");
  ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_GPIO_STATUS_LED, 0), TAG,
                      "status LED level");
  const gpio_config_t output_config = {
      .pin_bit_mask =
          (1ULL << BOARD_GPIO_LED_PWR_EN) | (1ULL << BOARD_GPIO_STATUS_LED),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&output_config), TAG, "configure outputs");

  // FLT has its own pull-up (R404).
  const gpio_config_t fault_config = {
      .pin_bit_mask = 1ULL << BOARD_GPIO_LED_FAULT_N,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&fault_config), TAG, "configure LED fault");

  // Hold the SK6812 data line low with the pull-down until the RMT driver
  // claims it. Configuring it as a GPIO output here would reserve the pin and
  // make the RMT driver report a conflict.
  const gpio_config_t led_data_config = {
      .pin_bit_mask = 1ULL << BOARD_GPIO_LED_DATA,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&led_data_config), TAG,
                      "pull LED data low");

  initialized = true;
  return ESP_OK;
}
