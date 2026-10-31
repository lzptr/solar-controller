#include "led_power.h"

#include "board.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"

// ILM is 0.56 V at U402's 2.04 A limit; 2.5 dB covers that with headroom.
#define ATTEN ADC_ATTEN_DB_2_5
#define SAMPLES 8

static const char *TAG = "led_power";

static adc_oneshot_unit_handle_t adc;
static adc_channel_t channel;
static adc_cali_handle_t cali;
static bool enabled;

esp_err_t led_power_init(void) {
  if (adc != NULL) {
    return ESP_OK;
  }

  adc_unit_t unit;
  ESP_RETURN_ON_ERROR(
      adc_oneshot_io_to_channel(BOARD_GPIO_LED_ISENSE, &unit, &channel), TAG,
      "ILM ADC channel");
  const adc_oneshot_unit_init_cfg_t unit_config = {.unit_id = unit};
  ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &adc), TAG,
                      "create ADC unit");
  const adc_oneshot_chan_cfg_t channel_config = {
      .atten = ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(adc, channel, &channel_config),
                      TAG, "configure ILM channel");

  const adc_cali_curve_fitting_config_t cali_config = {
      .unit_id = unit,
      .chan = channel,
      .atten = ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali) != ESP_OK) {
    cali = NULL;
    ESP_LOGW(TAG, "no ADC calibration; current readings are approximate");
  }
  return ESP_OK;
}

void led_power_enable(bool on) {
  gpio_set_level(BOARD_GPIO_LED_PWR_EN, on);
  enabled = on;
}

bool led_power_enabled(void) { return enabled; }

bool led_power_fault(void) {
  return gpio_get_level(BOARD_GPIO_LED_FAULT_N) == 0;
}

esp_err_t led_power_measure(led_power_sample_t *sample) {
  ESP_RETURN_ON_FALSE(adc != NULL, ESP_ERR_INVALID_STATE, TAG, "not ready");
  int sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    int raw;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(adc, channel, &raw), TAG, "read ILM");
    sum += raw;
  }
  const int raw = (sum + SAMPLES / 2) / SAMPLES;

  int mv;
  if (cali != NULL) {
    ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(cali, raw, &mv), TAG,
                        "calibrate ILM");
  } else {
    mv = raw * 1000 / 4095;
  }
  sample->mv = mv;
  sample->ma = mv * 1000 / BOARD_ILM_MV_PER_A;
  return ESP_OK;
}
