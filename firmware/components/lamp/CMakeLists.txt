idf_component_register(
    SRCS "lamp.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common led_ring
    PRIV_REQUIRES bsp encoder esp_driver_gpio esp_timer freertos led_power log
                  nvs_flash
)
