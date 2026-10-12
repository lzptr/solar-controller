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

  const gpio_config_t output_config = {
      .pin_bit_mask = 1ULL << CONFIG_BSP_LED_DATA_GPIO,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  ESP_RETURN_ON_ERROR(gpio_config(&output_config), TAG,
                      "configure LED data output");
  ESP_RETURN_ON_ERROR(gpio_set_level(board_led_data_gpio(), 0), TAG,
                      "idle LED data low");

  initialized = true;
  return ESP_OK;
}

gpio_num_t board_led_data_gpio(void) {
  return (gpio_num_t)CONFIG_BSP_LED_DATA_GPIO;
}

size_t board_led_count(void) { return CONFIG_BSP_LED_COUNT; }
