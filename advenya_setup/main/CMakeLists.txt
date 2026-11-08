idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES advenya_board advenya_lvgl_port advenya_settings advenya_ui advenya_handoff
                  esp_timer esp_system log
)
