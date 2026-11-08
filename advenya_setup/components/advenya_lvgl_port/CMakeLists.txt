idf_build_get_property(build_components BUILD_COMPONENTS)
if("lvgl" IN_LIST build_components)
    set(ADV_LVGL_COMP lvgl)
else()
    set(ADV_LVGL_COMP lvgl__lvgl)
endif()

idf_component_register(
    SRCS "src/advenya_lvgl_port.c" "src/advenya_diag.c"
    INCLUDE_DIRS "include"
    REQUIRES advenya_board ${ADV_LVGL_COMP} esp_lcd
    PRIV_REQUIRES esp_timer freertos log heap
)
