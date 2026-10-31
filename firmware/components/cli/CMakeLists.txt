idf_component_register(
    SRCS "cli.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES console esp_system esp_timer freertos lamp led_ring
)
