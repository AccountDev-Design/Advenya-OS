idf_build_get_property(build_components BUILD_COMPONENTS)
if("lvgl" IN_LIST build_components)
    set(ADV_LVGL_COMP lvgl)
else()
    set(ADV_LVGL_COMP lvgl__lvgl)
endif()

# Interfaz portable (tambien la compila el simulador de sim/) + la capa de
# plataforma de ESP-IDF (ui_platform_esp.c).
idf_component_register(
    SRCS "src/ui_core.c" "src/ui_theme.c" "src/ui_i18n.c" "src/ui_backdrop.c"
         "src/ui_boot.c" "src/ui_wizard.c" "src/ui_widgets.c" "src/ui_legal.c"
         "src/ui_scr_welcome.c" "src/ui_scr_language.c" "src/ui_scr_privacy.c"
         "src/ui_scr_personalize.c" "src/i18n/ui_i18n_gen.c" "src/legal/legal_docs.c"
         "src/ui_platform_esp.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES ${ADV_LVGL_COMP}
    PRIV_REQUIRES advenya_assets advenya_settings advenya_handoff advenya_lvgl_port esp_timer freertos heap log
)
