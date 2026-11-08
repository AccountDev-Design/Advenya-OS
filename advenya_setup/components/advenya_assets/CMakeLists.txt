idf_build_get_property(build_components BUILD_COMPONENTS)
if("lvgl" IN_LIST build_components)
    set(ADV_LVGL_COMP lvgl)
else()
    set(ADV_LVGL_COMP lvgl__lvgl)
endif()

file(GLOB ADV_ASSET_SRCS "src/generated/*.c" "src/fonts/*.c")
idf_component_register(
    SRCS ${ADV_ASSET_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES ${ADV_LVGL_COMP}
)
