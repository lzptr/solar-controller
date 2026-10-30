#include "cli.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "lamp.h"

#define WATCH_PERIOD_MS 250
#define WATCH_MAX_S 120

#define STR_(x) #x
#define STR(x) STR_(x)

static const char *TAG = "cli";
static bool started;

static int usage(const char *name);

static bool parse_uint(const char *text, unsigned long max,
                       unsigned long *value) {
  if (text == NULL || !isdigit((unsigned char)text[0])) {
    return false;
  }
  char *end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (*end != '\0' || parsed > max) {
    return false;
  }
  *value = parsed;
  return true;
}

static bool parse_color(char **argv, led_ring_rgbw_t *color) {
  uint8_t v[4];
  for (size_t i = 0; i < 4; i++) {
    unsigned long parsed;
    if (!parse_uint(argv[i], 255, &parsed)) {
      printf("invalid channel value '%s' (0-255)\n", argv[i]);
      return false;
    }
    v[i] = (uint8_t)parsed;
  }
  *color = (led_ring_rgbw_t){v[0], v[1], v[2], v[3]};
  return true;
}

static void print_color(const char *label, led_ring_rgbw_t color) {
  printf("%sR=%3u G=%3u B=%3u W=%3u", label, color.r, color.g, color.b,
         color.w);
}

static void print_permille(const char *label, unsigned permille) {
  printf("%s%u.%u%%", label, permille / 10, permille % 10);
}

static void print_status(void) {
  lamp_status_t s;
  lamp_get_status(&s);
  printf("lamp    level %u/%u", s.level, s.level_count);
  print_permille(" (", s.brightness_permille);
  printf(")  %s  %s\n", s.mode == LAMP_MODE_WHITE ? "white" : "colour",
         s.manual ? "MANUAL (type 'scene' to leave)" : "scene");
  printf("power   EN %s%s  FLT %s  fault: %s\n",
         s.power_enabled ? "on" : "off", s.power_ramping ? " (ramping)" : "",
         s.flt_low ? "LOW" : "high", lamp_fault_name(s.fault));
  printf("current measured %d mA (ILM %d mV)  estimate %u mA  budget %u mA\n",
         s.measured_ma, s.measured_mv, (unsigned)s.estimated_ma,
         (unsigned)s.budget_ma);
  print_permille("limit   scale ", s.scale_permille);
  print_permille("  guard ", s.guard_permille);
  printf("  %u LEDs\n", (unsigned)s.led_count);
}

static int finish(esp_err_t error) {
  if (error != ESP_OK) {
    printf("failed: %s\n", esp_err_to_name(error));
    return 1;
  }
  print_status();
  return 0;
}

static void print_globe(size_t globe) {
  printf("%u %-8s ", (unsigned)globe, lamp_globe_name(globe));
  print_color("", lamp_globe_color(globe));
  printf("\n");
}

static int cmd_status(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  print_status();
  return 0;
}

static int cmd_bright(int argc, char **argv) {
  lamp_status_t s;
  lamp_get_status(&s);
  unsigned long level;
  if (argc != 2 || !parse_uint(argv[1], s.level_count, &level)) {
    return usage(argv[0]);
  }
  return finish(lamp_set_level((uint8_t)level));
}

static int cmd_mode(int argc, char **argv) {
  if (argc != 2) {
    return usage(argv[0]);
  }
  if (strcmp(argv[1], "colour") == 0 || strcmp(argv[1], "color") == 0) {
    return finish(lamp_set_mode(LAMP_MODE_COLOUR));
  }
  if (strcmp(argv[1], "white") == 0) {
    return finish(lamp_set_mode(LAMP_MODE_WHITE));
  }
  return usage(argv[0]);
}

static int cmd_globe(int argc, char **argv) {
  if (argc == 1) {
    for (size_t i = 0; i < LAMP_GLOBE_COUNT; i++) {
      print_globe(i);
    }
    return 0;
  }
  unsigned long globe;
  led_ring_rgbw_t color;
  if (argc != 6 || !parse_uint(argv[1], LAMP_GLOBE_COUNT - 1, &globe) ||
      !parse_color(&argv[2], &color)) {
    return usage(argv[0]);
  }
  const esp_err_t error = lamp_set_globe_color(globe, color);
  if (error == ESP_OK) {
    print_globe(globe);
  }
  return finish(error);
}

static int cmd_white(int argc, char **argv) {
  led_ring_rgbw_t color;
  if (argc == 1) {
    print_color("white ", lamp_white_color());
    printf("\n");
    return 0;
  }
  if (argc != 5 || !parse_color(&argv[1], &color)) {
    return usage(argv[0]);
  }
  return finish(lamp_set_white_color(color));
}

static int cmd_count(int argc, char **argv) {
  unsigned long count;
  if (argc != 2 || !parse_uint(argv[1], CONFIG_BSP_LED_MAX, &count) ||
      count == 0) {
    return usage(argv[0]);
  }
  return finish(lamp_set_led_count(count));
}

static int cmd_budget(int argc, char **argv) {
  unsigned long budget;
  if (argc != 2 || !parse_uint(argv[1], lamp_budget_max_ma(), &budget) ||
      budget < LAMP_BUDGET_MIN_MA) {
    printf("budget range %u-%u mA\n", LAMP_BUDGET_MIN_MA,
           (unsigned)lamp_budget_max_ma());
    return usage(argv[0]);
  }
  return finish(lamp_set_budget_ma(budget));
}

static int cmd_save(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  const esp_err_t error = lamp_save_config();
  if (error != ESP_OK) {
    printf("save failed: %s\n", esp_err_to_name(error));
    return 1;
  }
  printf("saved colours, white mix, LED count and budget\n");
  return 0;
}

static int cmd_defaults(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  lamp_restore_defaults();
  printf("built-in colours, LED count and budget restored; 'save' keeps "
         "them\n");
  return finish(ESP_OK);
}

static int cmd_watch(int argc, char **argv) {
  unsigned long seconds = 10;
  if (argc > 2 || (argc == 2 && !parse_uint(argv[1], WATCH_MAX_S, &seconds))) {
    return usage(argv[0]);
  }
  const int64_t start = esp_timer_get_time() / 1000;
  TickType_t wake = xTaskGetTickCount();
  for (unsigned long i = 0; i < seconds * 1000 / WATCH_PERIOD_MS; i++) {
    lamp_status_t s;
    lamp_get_status(&s);
    printf("%6lld ms  measured %4d mA  estimate %4u mA  ",
           (long long)(esp_timer_get_time() / 1000 - start), s.measured_ma,
           (unsigned)s.estimated_ma);
    print_permille("scale ", s.scale_permille);
    print_permille("  guard ", s.guard_permille);
    printf("  EN %s  fault: %s\n", s.power_enabled ? "on" : "off",
           lamp_fault_name(s.fault));
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(WATCH_PERIOD_MS));
  }
  return 0;
}

static int cmd_leds(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  led_ring_rgbw_t stored;
  led_ring_rgbw_t sent;
  for (size_t i = 0; lamp_led(i, &stored, &sent) == ESP_OK; i++) {
    printf("%2u %-8s ", (unsigned)i,
           lamp_globe_name(i / LAMP_LEDS_PER_GLOBE % LAMP_GLOBE_COUNT));
    print_color("", stored);
    print_color("  sent ", sent);
    printf("\n");
  }
  return 0;
}

static int cmd_rgbw(int argc, char **argv) {
  led_ring_rgbw_t color;
  if (argc != 5 || !parse_color(&argv[1], &color)) {
    return usage(argv[0]);
  }
  return finish(lamp_manual_fill(color));
}

static int cmd_pixel(int argc, char **argv) {
  lamp_status_t s;
  lamp_get_status(&s);
  unsigned long index;
  if (argc != 6) {
    return usage(argv[0]);
  }
  if (!parse_uint(argv[1], s.led_count - 1, &index)) {
    printf("invalid LED index '%s' (0-%u)\n", argv[1],
           (unsigned)s.led_count - 1);
    return usage(argv[0]);
  }
  led_ring_rgbw_t color;
  if (!parse_color(&argv[2], &color)) {
    return usage(argv[0]);
  }
  return finish(lamp_manual_pixel(index, color));
}

static int cmd_power(int argc, char **argv) {
  if (argc != 2) {
    return usage(argv[0]);
  }
  if (strcmp(argv[1], "on") == 0) {
    lamp_manual_power(true);
  } else if (strcmp(argv[1], "off") == 0) {
    lamp_manual_power(false);
  } else {
    return usage(argv[0]);
  }
  return finish(ESP_OK);
}

static int cmd_scene(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  lamp_resume_scene();
  return finish(ESP_OK);
}

static int cmd_reboot(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  esp_restart();
  return 0;
}

typedef struct {
  const char *name;
  const char *hint;
  const char *help;
  esp_console_cmd_func_t func;
} command_t;

static const command_t COMMANDS[] = {
    {"status", NULL, "Show level, mode, LED power, fault and current",
     cmd_status},
    {"bright", "<level>",
     "Set the brightness level, 0 = off (eFuse off); saved after 3 s",
     cmd_bright},
    {"mode", "colour|white", "Planet colours or the white mix; saved after 3 s",
     cmd_mode},
    {"globe", "[<0-9> <r> <g> <b> <w>]",
     "List the globe colours, or set one (0 = Sun ... 9 = Pluto); 'save' "
     "keeps it",
     cmd_globe},
    {"white", "[<r> <g> <b> <w>]",
     "Show or set the white-mode mix; 'save' keeps it", cmd_white},
    {"count", "<1-" STR(CONFIG_BSP_LED_MAX) ">",
     "LEDs on the chain, 7 per globe; 'save' keeps it", cmd_count},
    {"budget", "<mA>",
     "LED current budget incl. idle current; 'save' keeps it", cmd_budget},
    {"save", NULL, "Save colours, white mix, LED count and budget to flash",
     cmd_save},
    {"defaults", NULL,
     "Restore built-in colours, LED count and budget (not saved)",
     cmd_defaults},
    {"watch", "[<seconds>]", "Print the LED current every 250 ms (default 10 s)",
     cmd_watch},
    {"leds", NULL, "List every LED's stored and sent values", cmd_leds},
    {"rgbw", "<r> <g> <b> <w>",
     "Debug: set all LEDs raw (manual mode, power on)", cmd_rgbw},
    {"pixel", "<index> <r> <g> <b> <w>",
     "Debug: set one LED raw (manual mode, power on)", cmd_pixel},
    {"power", "on|off",
     "Debug: drive LED power directly (manual mode); 'on' clears a fault",
     cmd_power},
    {"scene", NULL, "Leave manual mode and show the scene again", cmd_scene},
    {"reboot", NULL, "Restart the controller", cmd_reboot},
};

static int usage(const char *name) {
  for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
    if (strcmp(COMMANDS[i].name, name) == 0) {
      printf("usage: %s %s\n", name,
             COMMANDS[i].hint != NULL ? COMMANDS[i].hint : "");
      break;
    }
  }
  return 1;
}

esp_err_t cli_start(void) {
  if (started) {
    return ESP_ERR_INVALID_STATE;
  }

  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.prompt = "lamp>";
  repl_config.max_cmdline_length = 64;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  const esp_console_dev_usb_serial_jtag_config_t device =
      ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(
      esp_console_new_repl_usb_serial_jtag(&device, &repl_config, &repl), TAG,
      "create USB Serial/JTAG console");
#elif CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
  const esp_console_dev_uart_config_t device =
      ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&device, &repl_config, &repl),
                      TAG, "create UART console");
#else
#error "Select a USB Serial/JTAG or UART console for the CLI"
#endif

  ESP_RETURN_ON_ERROR(esp_console_register_help_command(), TAG,
                      "register help");
  for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
    const esp_console_cmd_t command = {
        .command = COMMANDS[i].name,
        .help = COMMANDS[i].help,
        .hint = COMMANDS[i].hint,
        .func = COMMANDS[i].func,
    };
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&command), TAG,
                        "register %s", COMMANDS[i].name);
  }

  ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "start console");
  started = true;
  return ESP_OK;
}
