#include "board.h"

#include "esp_log.h"

#include "led_cli.h"
#include "led_ring.h"

void app_main(void) {
  ESP_ERROR_CHECK(board_init());
  ESP_ERROR_CHECK(led_ring_init());
  ESP_LOGI("app", "SK6812 ring test ready; type 'help' for commands");
  ESP_ERROR_CHECK(led_cli_start());
}
