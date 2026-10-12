#include "led_ring.h"

#include <stdlib.h>

#include "board.h"
#include "esp_check.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "led_ring";

static led_strip_handle_t strip;
static led_ring_rgbw_t *pixels;
static size_t count;
static uint8_t brightness;
static bool on;

static uint8_t scale(uint8_t value, uint8_t percent) {
  return (uint8_t)((value * percent + 50U) / 100U);
}

static esp_err_t render(void) {
  for (size_t i = 0; i < count; i++) {
    const led_ring_rgbw_t out = led_ring_output(i);
    ESP_RETURN_ON_ERROR(
        led_strip_set_pixel_rgbw(strip, i, out.r, out.g, out.b, out.w), TAG,
        "set LED %u", (unsigned)i);
  }
  ESP_RETURN_ON_ERROR(led_strip_refresh(strip), TAG, "refresh LEDs");
  return ESP_OK;
}

esp_err_t led_ring_init(void) {
  if (strip != NULL) {
    return ESP_OK;
  }

  count = board_led_count();
  pixels = calloc(count, sizeof(*pixels));
  if (pixels == NULL) {
    return ESP_ERR_NO_MEM;
  }

  const led_strip_config_t strip_config = {
      .strip_gpio_num = board_led_data_gpio(),
      .max_leds = count,
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

  for (size_t i = 0; i < count; i++) {
    pixels[i] = (led_ring_rgbw_t){.w = CONFIG_LED_RING_BOOT_WHITE};
  }
  brightness = CONFIG_LED_RING_BOOT_BRIGHTNESS;
  on = true;
  return render();
}

size_t led_ring_count(void) { return count; }

esp_err_t led_ring_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  ESP_RETURN_ON_FALSE(strip != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  for (size_t i = 0; i < count; i++) {
    pixels[i].r = r;
    pixels[i].g = g;
    pixels[i].b = b;
  }
  return render();
}

esp_err_t led_ring_set_white(uint8_t w) {
  ESP_RETURN_ON_FALSE(strip != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  for (size_t i = 0; i < count; i++) {
    pixels[i].w = w;
  }
  return render();
}

esp_err_t led_ring_set_all(led_ring_rgbw_t color) {
  ESP_RETURN_ON_FALSE(strip != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  for (size_t i = 0; i < count; i++) {
    pixels[i] = color;
  }
  return render();
}

esp_err_t led_ring_set_pixel(size_t index, led_ring_rgbw_t color) {
  ESP_RETURN_ON_FALSE(strip != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  ESP_RETURN_ON_FALSE(index < count, ESP_ERR_INVALID_ARG, TAG,
                      "LED %u out of range", (unsigned)index);
  pixels[index] = color;
  return render();
}

esp_err_t led_ring_set_brightness(uint8_t percent) {
  ESP_RETURN_ON_FALSE(strip != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG,
                      "brightness %u%% out of range", percent);
  brightness = percent;
  return render();
}

esp_err_t led_ring_set_on(bool enable) {
  ESP_RETURN_ON_FALSE(strip != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  on = enable;
  return render();
}

led_ring_rgbw_t led_ring_pixel(size_t index) {
  return index < count ? pixels[index] : (led_ring_rgbw_t){0};
}

led_ring_rgbw_t led_ring_output(size_t index) {
  if (!on || index >= count) {
    return (led_ring_rgbw_t){0};
  }
  const led_ring_rgbw_t color = pixels[index];
  return (led_ring_rgbw_t){
      .r = scale(color.r, brightness),
      .g = scale(color.g, brightness),
      .b = scale(color.b, brightness),
      .w = scale(color.w, brightness),
  };
}

uint8_t led_ring_brightness(void) { return brightness; }

bool led_ring_is_on(void) { return on; }
