idf_component_register(
    SRCS "led_power.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES bsp esp_adc esp_driver_gpio log
)
