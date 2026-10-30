#include "lamp.h"

#include <math.h>

#include "board.h"
#include "driver/gpio.h"
#include "encoder.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_power.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define TICK_MS 10
#define MEASURE_MS 100
// U402 ramps +5V at about 0.42 V/ms with C403 (100 nF): 5 V in 12 ms.
#define RAMP_MS 50
#define STATE_SAVE_DELAY_MS 3000
#define FAULT_BLINK_MS 100
#define OVERCURRENT_SAMPLES 3
#define GUARD_MIN 100
#define GUARD_DOWN 50
#define GUARD_UP 10

#define STATE_VERSION 2
#define CONFIG_VERSION 1

static const char *TAG = "lamp";

static const char *const GLOBE_NAMES[LAMP_GLOBE_COUNT] = {
    "Sun",     "Mercury", "Venus",  "Earth",   "Mars",
    "Jupiter", "Saturn",  "Uranus", "Neptune", "Pluto",
};

// Starting points for tuning on the real globes with 'globe'.
static const led_ring_rgbw_t DEFAULT_GLOBES[LAMP_GLOBE_COUNT] = {
    {255, 110, 0, 60},  // Sun
    {40, 30, 20, 120},  // Mercury
    {160, 110, 20, 60}, // Venus
    {0, 60, 200, 40},   // Earth
    {220, 50, 10, 10},  // Mars
    {200, 100, 30, 60}, // Jupiter
    {180, 130, 40, 80}, // Saturn
    {20, 160, 170, 30}, // Uranus
    {10, 40, 255, 10},  // Neptune
    {120, 70, 50, 60},  // Pluto
};
static const led_ring_rgbw_t DEFAULT_WHITE = {0, 0, 0, 255};

typedef enum { POWER_OFF, POWER_RAMPING, POWER_ON } power_state_t;

// NVS blobs; bump the version when the layout changes.
typedef struct {
  uint8_t version;
  uint8_t mode;
  uint16_t brightness; // permille of full
} saved_state_t;

typedef struct {
  uint8_t version;
  uint8_t led_count;
  uint16_t budget_ma;
  led_ring_rgbw_t white;
  led_ring_rgbw_t globes[LAMP_GLOBE_COUNT];
} saved_config_t;

static SemaphoreHandle_t lock;
static nvs_handle_t nvs;
static saved_state_t state;
static saved_config_t config;

static bool manual;
static bool manual_power;
static power_state_t power;
static int64_t ramp_done_ms;
static lamp_fault_t fault;
static uint16_t guard = 1000;
static int overcurrent_samples;
static led_power_sample_t measured;
static int64_t state_save_ms;
static uint16_t encoder_steps[CONFIG_LAMP_ENCODER_STEPS];

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static uint32_t budget_max_ma(void) { return CONFIG_LAMP_OVERCURRENT_MA - 200; }

// Encoder stops: equal ratios from the lowest step to full, so each detent
// looks about as big.
static void init_encoder_steps(void) {
  const float lowest = CONFIG_LAMP_MIN_BRIGHTNESS_PERMILLE / 1000.0f;
  for (int i = 0; i < CONFIG_LAMP_ENCODER_STEPS; i++) {
    const float exponent = (float)(CONFIG_LAMP_ENCODER_STEPS - 1 - i) /
                           (CONFIG_LAMP_ENCODER_STEPS - 1);
    encoder_steps[i] = (uint16_t)lroundf(1000.0f * powf(lowest, exponent));
  }
}

// Move to the next stop above or below, so a brightness set from the CLI
// between two stops snaps onto the curve with the first detent.
static uint16_t step_brightness(uint16_t permille, int steps) {
  for (; steps > 0; steps--) {
    int i = 0;
    while (i < CONFIG_LAMP_ENCODER_STEPS && encoder_steps[i] <= permille) {
      i++;
    }
    if (i == CONFIG_LAMP_ENCODER_STEPS) {
      break;
    }
    permille = encoder_steps[i];
  }
  for (; steps < 0; steps++) {
    int i = CONFIG_LAMP_ENCODER_STEPS - 1;
    while (i >= 0 && encoder_steps[i] >= permille) {
      i--;
    }
    permille = i >= 0 ? encoder_steps[i] : 0;
  }
  return permille;
}

static uint8_t scale_channel(uint8_t value, uint16_t permille) {
  return (uint8_t)(((uint32_t)value * permille + 500U) / 1000U);
}

static led_ring_rgbw_t scale_color(led_ring_rgbw_t color, uint16_t permille) {
  return (led_ring_rgbw_t){
      .r = scale_channel(color.r, permille),
      .g = scale_channel(color.g, permille),
      .b = scale_channel(color.b, permille),
      .w = scale_channel(color.w, permille),
  };
}

static void build_scene(void) {
  const uint16_t permille = state.brightness;
  for (size_t i = 0; i < led_ring_count(); i++) {
    const led_ring_rgbw_t base =
        state.mode == LAMP_MODE_WHITE
            ? config.white
            : config.globes[(i / LAMP_LEDS_PER_GLOBE) % LAMP_GLOBE_COUNT];
    led_ring_set_pixel(i, scale_color(base, permille));
  }
}

static void show(void) {
  const esp_err_t error = led_ring_show();
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "send LEDs: %s", esp_err_to_name(error));
  }
}

// Bring LED power and the frame in line with the current state. Call with
// the lock held after any change.
static void update(void) {
  const bool want_power =
      fault == LAMP_FAULT_NONE &&
      (manual ? manual_power : state.brightness > 0);

  if (!want_power && power != POWER_OFF) {
    // Blank the chain while it still has power, then cut it.
    led_ring_set_output(false);
    show();
    led_power_enable(false);
    power = POWER_OFF;
  } else if (want_power && power == POWER_OFF) {
    led_power_enable(true);
    power = POWER_RAMPING;
    ramp_done_ms = now_ms() + RAMP_MS;
    guard = 1000;
    overcurrent_samples = 0;
  }

  if (!manual) {
    build_scene();
  }
  led_ring_set_guard(guard);
  led_ring_set_output(power == POWER_ON);
  if (power == POWER_ON) {
    show();
  }
}

static void trip(lamp_fault_t reason) {
  fault = reason;
  ESP_LOGE(TAG, "LED power off: %s (measured %d mA); turn or press the "
                "encoder, or 'power on', to retry",
           lamp_fault_name(reason), measured.ma);
  update();
}

static bool clear_fault(void) {
  if (fault == LAMP_FAULT_NONE) {
    return false;
  }
  ESP_LOGW(TAG, "fault cleared: %s", lamp_fault_name(fault));
  fault = LAMP_FAULT_NONE;
  return true;
}

static void mark_state_dirty(void) {
  state_save_ms = now_ms() + STATE_SAVE_DELAY_MS;
}

static void save_state(void) {
  state_save_ms = 0;
  esp_err_t error = nvs_set_blob(nvs, "state", &state, sizeof(state));
  if (error == ESP_OK) {
    error = nvs_commit(nvs);
  }
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "save state: %s", esp_err_to_name(error));
  }
}

static void handle_input(encoder_event_t event) {
  clear_fault();
  manual = false;

  if (event.steps != 0) {
    const uint16_t brightness = step_brightness(state.brightness, event.steps);
    if (brightness != state.brightness) {
      state.brightness = brightness;
      mark_state_dirty();
    }
    ESP_LOGI(TAG, "encoder %+d -> %u.%u%%", event.steps,
             state.brightness / 10, state.brightness % 10);
  }
  if (event.pressed) {
    state.mode =
        state.mode == LAMP_MODE_COLOUR ? LAMP_MODE_WHITE : LAMP_MODE_COLOUR;
    mark_state_dirty();
    ESP_LOGI(TAG, "press -> %s",
             state.mode == LAMP_MODE_WHITE ? "white" : "colour");
  }
  update();
}

static void measure(void) {
  if (led_power_measure(&measured) != ESP_OK) {
    return;
  }
  if (power != POWER_ON) {
    overcurrent_samples = 0;
    return;
  }

  // U402 only current-limits on overload; FLT follows much later, if at all.
  if (measured.ma >= CONFIG_LAMP_OVERCURRENT_MA) {
    if (++overcurrent_samples >= OVERCURRENT_SAMPLES) {
      trip(LAMP_FAULT_OVERCURRENT);
    }
    return;
  }
  overcurrent_samples = 0;

  // Back the frame off when the estimate undershoots the real current and
  // recover slowly once it is under budget again.
  const int budget = (int)config.budget_ma;
  uint16_t next = guard;
  if (measured.ma > budget * 11 / 10) {
    next = guard > GUARD_MIN + GUARD_DOWN ? guard - GUARD_DOWN : GUARD_MIN;
  } else if (measured.ma < budget * 95 / 100 && guard < 1000) {
    next = guard + GUARD_UP > 1000 ? 1000 : guard + GUARD_UP;
  }
  if (next != guard) {
    guard = next;
    led_ring_set_guard(guard);
    show();
  }
}

static void update_status_led(int64_t now) {
  bool on = power != POWER_OFF;
  if (fault != LAMP_FAULT_NONE) {
    on = (now / FAULT_BLINK_MS) % 2 == 0;
  }
  gpio_set_level(BOARD_GPIO_STATUS_LED, on);
}

static void tick(int64_t now) {
  const encoder_event_t event = encoder_poll();
  if (event.steps != 0 || event.pressed) {
    handle_input(event);
  }
  if (power == POWER_RAMPING && now >= ramp_done_ms) {
    power = POWER_ON;
    update();
  }
  if (power == POWER_ON && led_power_fault()) {
    trip(LAMP_FAULT_OVERTEMP);
  }
  static int64_t next_measure_ms;
  if (now >= next_measure_ms) {
    next_measure_ms = now + MEASURE_MS;
    measure();
  }
  if (state_save_ms != 0 && now >= state_save_ms) {
    save_state();
  }
  update_status_led(now);
}

static void lamp_task(void *arg) {
  (void)arg;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    xSemaphoreTake(lock, portMAX_DELAY);
    tick(now_ms());
    xSemaphoreGive(lock);
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(TICK_MS));
  }
}

static void default_config(void) {
  config = (saved_config_t){
      .version = CONFIG_VERSION,
      .led_count = (uint8_t)led_ring_max_count(),
      .budget_ma = CONFIG_LAMP_DEFAULT_BUDGET_MA < budget_max_ma()
                       ? CONFIG_LAMP_DEFAULT_BUDGET_MA
                       : budget_max_ma(),
      .white = DEFAULT_WHITE,
  };
  for (size_t i = 0; i < LAMP_GLOBE_COUNT; i++) {
    config.globes[i] = DEFAULT_GLOBES[i];
  }
}

static bool load_blob(const char *key, void *blob, size_t size) {
  size_t stored = size;
  return nvs_get_blob(nvs, key, blob, &stored) == ESP_OK && stored == size;
}

static void load_settings(void) {
  if (!load_blob("config", &config, sizeof(config)) ||
      config.version != CONFIG_VERSION || config.led_count < 1 ||
      config.led_count > led_ring_max_count() ||
      config.budget_ma < LAMP_BUDGET_MIN_MA ||
      config.budget_ma > budget_max_ma()) {
    default_config();
  }
  if (!load_blob("state", &state, sizeof(state)) ||
      state.version != STATE_VERSION || state.brightness > 1000 ||
      state.mode > LAMP_MODE_WHITE) {
    state = (saved_state_t){.version = STATE_VERSION};
  }
}

static esp_err_t open_nvs(void) {
  esp_err_t error = nvs_flash_init();
  if (error == ESP_ERR_NVS_NO_FREE_PAGES ||
      error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase NVS");
    error = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(error, TAG, "init NVS");
  return nvs_open("lamp", NVS_READWRITE, &nvs);
}

esp_err_t lamp_start(void) {
  ESP_RETURN_ON_FALSE(lock == NULL, ESP_ERR_INVALID_STATE, TAG,
                      "already started");
  ESP_RETURN_ON_ERROR(led_ring_init(), TAG, "LED ring");
  ESP_RETURN_ON_ERROR(led_power_init(), TAG, "LED power");
  ESP_RETURN_ON_ERROR(encoder_init(), TAG, "encoder");
  ESP_RETURN_ON_ERROR(open_nvs(), TAG, "open NVS");
  load_settings();
  init_encoder_steps();

  ESP_RETURN_ON_ERROR(led_ring_set_count(config.led_count), TAG, "LED count");
  led_ring_set_budget_ma(config.budget_ma);

  lock = xSemaphoreCreateMutex();
  ESP_RETURN_ON_FALSE(lock != NULL, ESP_ERR_NO_MEM, TAG, "create lock");
  update();
  ESP_LOGI(TAG, "brightness %u.%u%%, %s, %u LEDs, budget %u mA",
           state.brightness / 10, state.brightness % 10,
           state.mode == LAMP_MODE_WHITE ? "white" : "colour",
           config.led_count, config.budget_ma);

  ESP_RETURN_ON_FALSE(xTaskCreate(lamp_task, "lamp", 4096, NULL, 5, NULL) ==
                          pdPASS,
                      ESP_ERR_NO_MEM, TAG, "start lamp task");
  return ESP_OK;
}

void lamp_get_status(lamp_status_t *status) {
  xSemaphoreTake(lock, portMAX_DELAY);
  *status = (lamp_status_t){
      .brightness_permille = state.brightness,
      .mode = state.mode,
      .manual = manual,
      .power_enabled = led_power_enabled(),
      .power_ramping = power == POWER_RAMPING,
      .flt_low = led_power_fault(),
      .fault = fault,
      .led_count = led_ring_count(),
      .budget_ma = config.budget_ma,
      .estimated_ma = power == POWER_OFF ? 0 : led_ring_estimated_ma(),
      .measured_ma = measured.ma,
      .measured_mv = measured.mv,
      .scale_permille = led_ring_scale(),
      .guard_permille = guard,
  };
  xSemaphoreGive(lock);
}

const char *lamp_fault_name(lamp_fault_t reason) {
  switch (reason) {
  case LAMP_FAULT_NONE:
    return "none";
  case LAMP_FAULT_OVERTEMP:
    return "eFuse thermal shutdown (FLT)";
  case LAMP_FAULT_OVERCURRENT:
    return "overcurrent";
  }
  return "?";
}

uint32_t lamp_budget_max_ma(void) { return budget_max_ma(); }

const char *lamp_globe_name(size_t globe) {
  return globe < LAMP_GLOBE_COUNT ? GLOBE_NAMES[globe] : "?";
}

led_ring_rgbw_t lamp_globe_color(size_t globe) {
  xSemaphoreTake(lock, portMAX_DELAY);
  const led_ring_rgbw_t color =
      globe < LAMP_GLOBE_COUNT ? config.globes[globe] : (led_ring_rgbw_t){0};
  xSemaphoreGive(lock);
  return color;
}

led_ring_rgbw_t lamp_white_color(void) {
  xSemaphoreTake(lock, portMAX_DELAY);
  const led_ring_rgbw_t color = config.white;
  xSemaphoreGive(lock);
  return color;
}

esp_err_t lamp_led(size_t index, led_ring_rgbw_t *stored,
                   led_ring_rgbw_t *sent) {
  xSemaphoreTake(lock, portMAX_DELAY);
  const bool valid = index < led_ring_count();
  if (valid) {
    *stored = led_ring_pixel(index);
    *sent = led_ring_output(index);
  }
  xSemaphoreGive(lock);
  return valid ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t lamp_set_brightness(uint16_t permille) {
  ESP_RETURN_ON_FALSE(permille <= 1000, ESP_ERR_INVALID_ARG, TAG,
                      "brightness %u out of range", permille);
  xSemaphoreTake(lock, portMAX_DELAY);
  manual = false;
  state.brightness = permille;
  mark_state_dirty();
  update();
  xSemaphoreGive(lock);
  return ESP_OK;
}

esp_err_t lamp_set_mode(lamp_mode_t mode) {
  ESP_RETURN_ON_FALSE(mode == LAMP_MODE_COLOUR || mode == LAMP_MODE_WHITE,
                      ESP_ERR_INVALID_ARG, TAG, "invalid mode");
  xSemaphoreTake(lock, portMAX_DELAY);
  manual = false;
  state.mode = mode;
  mark_state_dirty();
  update();
  xSemaphoreGive(lock);
  return ESP_OK;
}

esp_err_t lamp_set_globe_color(size_t globe, led_ring_rgbw_t color) {
  ESP_RETURN_ON_FALSE(globe < LAMP_GLOBE_COUNT, ESP_ERR_INVALID_ARG, TAG,
                      "globe %u out of range", (unsigned)globe);
  xSemaphoreTake(lock, portMAX_DELAY);
  manual = false;
  config.globes[globe] = color;
  update();
  xSemaphoreGive(lock);
  return ESP_OK;
}

esp_err_t lamp_set_white_color(led_ring_rgbw_t color) {
  xSemaphoreTake(lock, portMAX_DELAY);
  manual = false;
  config.white = color;
  update();
  xSemaphoreGive(lock);
  return ESP_OK;
}

esp_err_t lamp_set_led_count(size_t count) {
  xSemaphoreTake(lock, portMAX_DELAY);
  const esp_err_t error = led_ring_set_count(count);
  if (error == ESP_OK) {
    config.led_count = (uint8_t)count;
    update();
  }
  xSemaphoreGive(lock);
  return error;
}

esp_err_t lamp_set_budget_ma(uint32_t budget_ma) {
  ESP_RETURN_ON_FALSE(budget_ma >= LAMP_BUDGET_MIN_MA &&
                          budget_ma <= budget_max_ma(),
                      ESP_ERR_INVALID_ARG, TAG, "budget %u mA out of range",
                      (unsigned)budget_ma);
  xSemaphoreTake(lock, portMAX_DELAY);
  config.budget_ma = (uint16_t)budget_ma;
  led_ring_set_budget_ma(budget_ma);
  update();
  xSemaphoreGive(lock);
  return ESP_OK;
}

esp_err_t lamp_save_config(void) {
  xSemaphoreTake(lock, portMAX_DELAY);
  esp_err_t error = nvs_set_blob(nvs, "config", &config, sizeof(config));
  if (error == ESP_OK) {
    error = nvs_commit(nvs);
  }
  xSemaphoreGive(lock);
  return error;
}

void lamp_restore_defaults(void) {
  xSemaphoreTake(lock, portMAX_DELAY);
  default_config();
  led_ring_set_count(config.led_count);
  led_ring_set_budget_ma(config.budget_ma);
  update();
  xSemaphoreGive(lock);
}

static void enter_manual(void) {
  if (!manual) {
    manual = true;
    manual_power = power != POWER_OFF;
  }
}

esp_err_t lamp_manual_fill(led_ring_rgbw_t color) {
  xSemaphoreTake(lock, portMAX_DELAY);
  enter_manual();
  manual_power = true;
  led_ring_fill(color);
  update();
  xSemaphoreGive(lock);
  return ESP_OK;
}

esp_err_t lamp_manual_pixel(size_t index, led_ring_rgbw_t color) {
  xSemaphoreTake(lock, portMAX_DELAY);
  enter_manual();
  const esp_err_t error = led_ring_set_pixel(index, color);
  if (error == ESP_OK) {
    manual_power = true;
    update();
  }
  xSemaphoreGive(lock);
  return error;
}

void lamp_manual_power(bool on) {
  xSemaphoreTake(lock, portMAX_DELAY);
  enter_manual();
  if (on) {
    clear_fault();
  }
  manual_power = on;
  update();
  xSemaphoreGive(lock);
}

void lamp_resume_scene(void) {
  xSemaphoreTake(lock, portMAX_DELAY);
  manual = false;
  update();
  xSemaphoreGive(lock);
}
