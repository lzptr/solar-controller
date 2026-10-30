#include "encoder.h"

#include "board.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_check.h"
#include "sdkconfig.h"

#define COUNT_LIMIT 1000
#define GLITCH_NS 10000
// Polls at 10 ms: 3 equal samples ride out the EC11's 3 ms chatter.
#define DEBOUNCE_POLLS 3
#define LONG_PRESS_POLLS 80

static const char *TAG = "encoder";

static pcnt_unit_handle_t unit;
static int last_count;
static int pending_counts;
static bool button_down;
static int button_stable_polls;
static int button_held_polls;

esp_err_t encoder_init(void) {
  if (unit != NULL) {
    return ESP_OK;
  }

  const pcnt_unit_config_t unit_config = {
      .low_limit = -COUNT_LIMIT,
      .high_limit = COUNT_LIMIT,
      .flags.accum_count = 1,
  };
  ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_config, &unit), TAG, "create PCNT");
  const pcnt_glitch_filter_config_t filter = {.max_glitch_ns = GLITCH_NS};
  ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(unit, &filter), TAG,
                      "glitch filter");

  // Full quadrature decoding: every edge on A and B counts.
  const pcnt_chan_config_t a_config = {
      .edge_gpio_num = BOARD_GPIO_ENC_A,
      .level_gpio_num = BOARD_GPIO_ENC_B,
  };
  const pcnt_chan_config_t b_config = {
      .edge_gpio_num = BOARD_GPIO_ENC_B,
      .level_gpio_num = BOARD_GPIO_ENC_A,
  };
  pcnt_channel_handle_t a;
  pcnt_channel_handle_t b;
  ESP_RETURN_ON_ERROR(pcnt_new_channel(unit, &a_config, &a), TAG, "channel A");
  ESP_RETURN_ON_ERROR(pcnt_new_channel(unit, &b_config, &b), TAG, "channel B");
  ESP_RETURN_ON_ERROR(
      pcnt_channel_set_edge_action(a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE),
      TAG, "channel A edges");
  ESP_RETURN_ON_ERROR(
      pcnt_channel_set_level_action(a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
      TAG, "channel A levels");
  ESP_RETURN_ON_ERROR(
      pcnt_channel_set_edge_action(b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                   PCNT_CHANNEL_EDGE_ACTION_DECREASE),
      TAG, "channel B edges");
  ESP_RETURN_ON_ERROR(
      pcnt_channel_set_level_action(b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
      TAG, "channel B levels");

  // Watch points at the limits let accum_count carry overflows.
  ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(unit, -COUNT_LIMIT), TAG,
                      "low watch point");
  ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(unit, COUNT_LIMIT), TAG,
                      "high watch point");
  ESP_RETURN_ON_ERROR(pcnt_unit_enable(unit), TAG, "enable PCNT");
  ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(unit), TAG, "clear PCNT");
  ESP_RETURN_ON_ERROR(pcnt_unit_start(unit), TAG, "start PCNT");

  // R413 pulls the button up.
  const gpio_config_t button_config = {
      .pin_bit_mask = 1ULL << BOARD_GPIO_ENC_SW_N,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&button_config), TAG, "configure button");
  button_down = gpio_get_level(BOARD_GPIO_ENC_SW_N) == 0;
  return ESP_OK;
}

static int take_steps(void) {
  int count = last_count;
  if (pcnt_unit_get_count(unit, &count) != ESP_OK) {
    return 0;
  }
  pending_counts += count - last_count;
  last_count = count;

  // Keep the remainder so a detent split across polls still counts once.
  const int steps = pending_counts / CONFIG_ENCODER_COUNTS_PER_DETENT;
  pending_counts -= steps * CONFIG_ENCODER_COUNTS_PER_DETENT;
#if CONFIG_ENCODER_REVERSE
  return -steps;
#else
  return steps;
#endif
}

static bool take_short_press(void) {
  const bool level_down = gpio_get_level(BOARD_GPIO_ENC_SW_N) == 0;
  if (level_down == button_down) {
    button_stable_polls = 0;
  } else if (++button_stable_polls >= DEBOUNCE_POLLS) {
    button_down = level_down;
    button_stable_polls = 0;
    if (!button_down) {
      // Long presses are reserved for later use.
      return button_held_polls < LONG_PRESS_POLLS;
    }
    button_held_polls = 0;
  }
  if (button_down) {
    button_held_polls++;
  }
  return false;
}

encoder_event_t encoder_poll(void) {
  if (unit == NULL) {
    return (encoder_event_t){0};
  }
  return (encoder_event_t){
      .steps = take_steps(),
      .pressed = take_short_press(),
  };
}
