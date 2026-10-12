#include "led_cli.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_console.h"
#include "sdkconfig.h"

#include "led_ring.h"

static const char *TAG = "led_cli";
static bool started;

static int usage(const char *name);

static bool parse_u8(const char *text, unsigned max, uint8_t *value) {
  if (text == NULL || !isdigit((unsigned char)text[0])) {
    return false;
  }
  char *end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (*end != '\0' || parsed > max) {
    return false;
  }
  *value = (uint8_t)parsed;
  return true;
}

static bool parse_channels(char **argv, size_t n, uint8_t *values) {
  for (size_t i = 0; i < n; i++) {
    if (!parse_u8(argv[i], 255, &values[i])) {
      printf("invalid channel value '%s' (0-255)\n", argv[i]);
      return false;
    }
  }
  return true;
}

static bool same_color(led_ring_rgbw_t a, led_ring_rgbw_t b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.w == b.w;
}

static void print_color(const char *label, led_ring_rgbw_t color) {
  printf("%s R=%u G=%u B=%u W=%u", label, color.r, color.g, color.b, color.w);
}

static void print_status(void) {
  const size_t count = led_ring_count();
  const char *state = led_ring_is_on() ? "on" : "off";
  bool uniform = true;
  for (size_t i = 1; i < count; i++) {
    uniform = uniform && same_color(led_ring_pixel(i), led_ring_pixel(0));
  }

  if (uniform) {
    print_color("colour", led_ring_pixel(0));
    printf("  bright=%u%%  %s\n", led_ring_brightness(), state);
    print_color("sent  ", led_ring_output(0));
    printf("\n");
    return;
  }

  printf("bright=%u%%  %s\n", led_ring_brightness(), state);
  for (size_t i = 0; i < count; i++) {
    printf("led %u  ", (unsigned)i);
    print_color("colour", led_ring_pixel(i));
    print_color("  sent", led_ring_output(i));
    printf("\n");
  }
}

static int finish(esp_err_t error) {
  if (error != ESP_OK) {
    printf("LED update failed: %s\n", esp_err_to_name(error));
    return 1;
  }
  print_status();
  return 0;
}

static int cmd_color(int argc, char **argv) {
  uint8_t v[3];
  if (argc != 4 || !parse_channels(&argv[1], 3, v)) {
    return usage(argv[0]);
  }
  return finish(led_ring_set_rgb(v[0], v[1], v[2]));
}

static int cmd_white(int argc, char **argv) {
  uint8_t w;
  if (argc != 2 || !parse_channels(&argv[1], 1, &w)) {
    return usage(argv[0]);
  }
  return finish(led_ring_set_white(w));
}

static int cmd_rgbw(int argc, char **argv) {
  uint8_t v[4];
  if (argc != 5 || !parse_channels(&argv[1], 4, v)) {
    return usage(argv[0]);
  }
  return finish(led_ring_set_all((led_ring_rgbw_t){v[0], v[1], v[2], v[3]}));
}

static int cmd_pixel(int argc, char **argv) {
  if (argc != 6) {
    return usage(argv[0]);
  }
  uint8_t index;
  if (!parse_u8(argv[1], led_ring_count() - 1, &index)) {
    printf("invalid LED index '%s' (0-%u)\n", argv[1],
           (unsigned)led_ring_count() - 1);
    return usage(argv[0]);
  }
  uint8_t v[4];
  if (!parse_channels(&argv[2], 4, v)) {
    return usage(argv[0]);
  }
  return finish(
      led_ring_set_pixel(index, (led_ring_rgbw_t){v[0], v[1], v[2], v[3]}));
}

static int cmd_bright(int argc, char **argv) {
  uint8_t percent;
  if (argc != 2 || !parse_u8(argv[1], 100, &percent)) {
    return usage(argv[0]);
  }
  return finish(led_ring_set_brightness(percent));
}

static int cmd_on(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  return finish(led_ring_set_on(true));
}

static int cmd_off(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  return finish(led_ring_set_on(false));
}

static int cmd_status(int argc, char **argv) {
  if (argc != 1) {
    return usage(argv[0]);
  }
  print_status();
  return 0;
}

typedef struct {
  const char *name;
  const char *hint;
  const char *help;
  esp_console_cmd_func_t func;
} command_t;

static const command_t COMMANDS[] = {
    {"color", "<r> <g> <b>",
     "Set RGB on all LEDs (0-255 each); keeps white", cmd_color},
    {"white", "<w>", "Set white on all LEDs (0-255); keeps RGB", cmd_white},
    {"rgbw", "<r> <g> <b> <w>",
     "Set all four channels on all LEDs (0-255 each)", cmd_rgbw},
    {"pixel", "<index> <r> <g> <b> <w>",
     "Set all four channels on one LED (index from 0)", cmd_pixel},
    {"bright", "<0-100>", "Set global brightness in percent", cmd_bright},
    {"on", NULL, "Switch the LEDs on with the stored colours", cmd_on},
    {"off", NULL, "Switch the LEDs off; colours are kept", cmd_off},
    {"status", NULL, "Print stored colours, brightness and the values sent",
     cmd_status},
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

esp_err_t led_cli_start(void) {
  if (started) {
    return ESP_ERR_INVALID_STATE;
  }

  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.prompt = "led>";
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
#error "Select a USB Serial/JTAG or UART console for the LED CLI"
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
