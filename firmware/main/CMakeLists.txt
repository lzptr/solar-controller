idf_component_register(
    SRCS "app_main.c"
    PRIV_REQUIRES bsp led_ring led_cli log
    INCLUDE_DIRS ""
)
