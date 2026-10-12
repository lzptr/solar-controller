idf_component_register(
    SRCS "led_cli.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES console led_ring log
)
