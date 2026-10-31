#include "board.h"

#include "esp_log.h"

#include "cli.h"
#include "lamp.h"

void app_main(void) {
  ESP_ERROR_CHECK(board_init());
  ESP_ERROR_CHECK(lamp_start());
  ESP_LOGI("app", "Solar controller ready; type 'help' for commands");
  ESP_ERROR_CHECK(cli_start());
}
