idf_component_register(
    SRCS "src/settings_core.c" "src/settings_nvs.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash esp_timer freertos log
)
