idf_component_register(
    SRCS "led_ring.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES bsp log espressif__led_strip
)
