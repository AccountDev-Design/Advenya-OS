idf_component_register(
    SRCS "src/board_display_st7701.c"
         "src/board_backlight.c"
         "src/board_touch_gt911.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_hw_support
    PRIV_REQUIRES esp_timer log
)
