#include "led_ring.h"

#include <stdlib.h>

#include "board.h"
#include "esp_check.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "led_ring";

static led_strip_handle_t strip;
static led_ring_rgbw_t *pixels;
static size_t max_count;
static size_t count;
static bool output;
static uint32_t budget_ma = UINT32_MAX;
static uint16_t guard = 1000;

static uint8_t scale_channel(uint8_t value, uint16_t permille) {
  // Round down so the estimate never undershoots what is sent.
  return (uint8_t)((uint32_t)value * permille / 1000U);
}

static led_ring_rgbw_t scale_color(led_ring_rgbw_t color, uint16_t permille) {
  return (led_ring_rgbw_t){
      .r = scale_channel(color.r, permille),
      .g = scale_channel(color.g, permille),
      .b = scale_channel(color.b, permille),
      .w = scale_channel(color.w, permille),
  };
}

static uint64_t idle_ua(void) {
  return (uint64_t)count * CONFIG_LED_RING_IDLE_UA;
}

// Current of the colour channels at the given scale, idle current excluded.
static uint64_t active_ua(uint16_t permille) {
  uint64_t sum = 0;
  for (size_t i = 0; i < count; i++) {
    sum += pixels[i].r + pixels[i].g + pixels[i].b + pixels[i].w;
  }
  return sum * CONFIG_LED_RING_CHANNEL_FULL_UA * permille / (255U * 1000U);
}

esp_err_t led_ring_init(void) {
  if (strip != NULL) {
    return ESP_OK;
  }

  max_count = BOARD_LED_MAX;
  count = max_count;
  pixels = calloc(max_count, sizeof(*pixels));
  if (pixels == NULL) {
    return ESP_ERR_NO_MEM;
  }

  const led_strip_config_t strip_config = {
      .strip_gpio_num = BOARD_GPIO_LED_DATA,
      .max_leds = max_count,
      .led_model = LED_MODEL_SK6812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,
  };
  const led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
  };
  const esp_err_t error =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
  if (error != ESP_OK) {
    free(pixels);
    pixels = NULL;
    strip = NULL;
    ESP_LOGE(TAG, "create SK6812 RMT device: %s", esp_err_to_name(error));
    return error;
  }
  return ESP_OK;
}

size_t led_ring_max_count(void) { return max_count; }

size_t led_ring_count(void) { return count; }

esp_err_t led_ring_set_count(size_t new_count) {
  ESP_RETURN_ON_FALSE(new_count >= 1 && new_count <= max_count,
                      ESP_ERR_INVALID_ARG, TAG, "LED count %u out of range",
                      (unsigned)new_count);
  count = new_count;
  return ESP_OK;
}

esp_err_t led_ring_set_pixel(size_t index, led_ring_rgbw_t color) {
  ESP_RETURN_ON_FALSE(index < count, ESP_ERR_INVALID_ARG, TAG,
                      "LED %u out of range", (unsigned)index);
  pixels[index] = color;
  return ESP_OK;
}

void led_ring_fill(led_ring_rgbw_t color) {
  for (size_t i = 0; i < count; i++) {
    pixels[i] = color;
  }
}

void led_ring_set_output(bool on) { output = on; }

void led_ring_set_budget_ma(uint32_t new_budget_ma) {
  budget_ma = new_budget_ma;
}

void led_ring_set_guard(uint16_t permille) {
  guard = permille > 1000 ? 1000 : permille;
}

uint16_t led_ring_scale(void) {
  if (!output) {
    return 0;
  }
  const uint64_t full = active_ua(1000);
  const uint64_t budget = (uint64_t)budget_ma * 1000U;
  const uint64_t allowed = budget > idle_ua() ? budget - idle_ua() : 0;
  if (full <= allowed) {
    return guard;
  }
  const uint16_t fit = (uint16_t)(allowed * 1000U / full);
  return fit < guard ? fit : guard;
}

esp_err_t led_ring_show(void) {
  ESP_RETURN_ON_FALSE(strip != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  const uint16_t scale = led_ring_scale();
  for (size_t i = 0; i < max_count; i++) {
    const led_ring_rgbw_t out =
        i < count ? scale_color(pixels[i], scale) : (led_ring_rgbw_t){0};
    ESP_RETURN_ON_ERROR(
        led_strip_set_pixel_rgbw(strip, i, out.r, out.g, out.b, out.w), TAG,
        "set LED %u", (unsigned)i);
  }
  ESP_RETURN_ON_ERROR(led_strip_refresh(strip), TAG, "refresh LEDs");
  return ESP_OK;
}

led_ring_rgbw_t led_ring_pixel(size_t index) {
  return index < count ? pixels[index] : (led_ring_rgbw_t){0};
}

led_ring_rgbw_t led_ring_output(size_t index) {
  if (index >= count) {
    return (led_ring_rgbw_t){0};
  }
  return scale_color(pixels[index], led_ring_scale());
}

uint32_t led_ring_estimated_ma(void) {
  return (uint32_t)((idle_ua() + active_ua(led_ring_scale()) + 500U) / 1000U);
}
