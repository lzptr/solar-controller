#include "board.h"

#include <stdbool.h>

#include "esp_check.h"
#include "sdkconfig.h"

static const char *TAG = "bsp";
static bool initialized;

esp_err_t board_init(void) {
  if (initialized) {
    return ESP_OK;
  }

  // Hold the SK6812 data line low with the pull-down until the RMT driver
  // claims it. Configuring it as a GPIO output here would reserve the pin and
  // make the RMT driver report a conflict.
  const gpio_config_t led_data_config = {
      .pin_bit_mask = 1ULL << CONFIG_BSP_LED_DATA_GPIO,
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

gpio_num_t board_led_data_gpio(void) {
  return (gpio_num_t)CONFIG_BSP_LED_DATA_GPIO;
}

size_t board_led_count(void) { return CONFIG_BSP_LED_COUNT; }
