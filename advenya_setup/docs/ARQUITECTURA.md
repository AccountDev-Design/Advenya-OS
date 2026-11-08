# Arquitectura de `advenya_setup`

```
main/main.c ── orden de arranque (NVS → hardware → LVGL → interfaz)
│
├─ components/advenya_board        CAPA DE HARDWARE (solo ESP-IDF, sin LVGL)
│    board_display_st7701.c        LDO + DSI + DBI + DPI + tabla DCS
│    board_backlight.c             LEDC 20 kHz
│    board_touch_gt911.c           GT911 + recuperación del bus I²C
│
├─ components/advenya_lvgl_port    LVGL ↔ hardware
│    advenya_lvgl_port.c           display parcial, flush DMA2D, indev, tarea, mutex
│    advenya_diag.c                FPS, tiempos de render, memoria, pila
│
├─ components/advenya_settings     ESTADO PERSISTENTE
│    settings_core.c               portable: validación, escritura diferida, sin duplicados
│    settings_nvs.c                backend NVS con tarea de escritura propia
│
├─ components/advenya_ui           INTERFAZ (portable: solo LVGL)
│    ui_core.c                     orquestación arranque → asistente / entrega
│    ui_boot.c                     animación del logotipo en capas
│    ui_wizard.c                   navegación Fluid Motion, gesto, etapas, final
│    ui_scr_*.c                    las cuatro pantallas
│    ui_theme.c / ui_i18n.c        estilos compartidos / textos centralizados
│    ui_backdrop.c                 fondo + velo, horneado y crossfade
│    ui_legal.c, legal/            panel de documentos y su registro
│    ui_platform_esp.c             PSRAM, tarea de trabajo, hora (solo placa)
│
├─ components/advenya_assets       recursos generados (fuentes, imágenes)
└─ components/advenya_handoff      punto de integración con el futuro núcleo
```

Dependencias en un solo sentido: `ui` no conoce el hardware (solo
`ui_platform.h`), `board` no conoce LVGL, `settings` no conoce ni LVGL ni
ESP-IDF en su núcleo. Gracias a eso `sim/` compila la interfaz real en el PC.

## Hilos

| Tarea | Núcleo | Prio | Qué hace |
|---|---|---|---|
| `lvgl` | 1 | 4 | `lv_timer_handler()`, render, animaciones, lectura del GT911 (cada 16 ms) |
| `adv_work` | 0 | 2 | horneado del fondo (≈380 k píxeles); devuelve el resultado con `lv_async_call` |
| `adv_nvs` | — | 2 | escrituras NVS (cola de longitud 1: siempre la última) |
| ISR DSI/DMA2D | — | — | `on_color_trans_done` → semáforo (IRAM, cache-safe) |

El I²C del táctil se usa siempre desde la tarea `lvgl` (regla de Flex para
el bus compartido). Ninguna animación usa esperas bloqueantes: todo son
`lv_anim` y `lv_timer`.

## Presentación

1. LVGL dibuja en uno de los dos buffers parciales (480×80 RGB565, 75 KB, RAM
   interna, alineados a 64 B).
2. `flush_cb` → `esp_lcd_panel_draw_bitmap()`: `esp_lcd` hace el write-back de
   caché y la DMA2D copia el bloque al framebuffer DPI (PSRAM).
3. LVGL dibuja ya en el otro buffer.
4. Antes de reutilizar el primero, `flush_wait_cb` duerme en el semáforo que
   libera la ISR (plazo 120 ms, se cuentan los vencimientos).
5. El puente DSI refresca el panel solo a ≈60 Hz desde el framebuffer.

Refresco de LVGL: 33 ms (objetivo 30 FPS estables). El panel no se apaga
durante escrituras de NVS gracias a `CONFIG_LCD_DSI_ISR_CACHE_SAFE`.

## Memoria

| Uso | Dónde | Tamaño |
|---|---|---|
| Buffers de dibujo | RAM interna | 2 × 75 KB |
| Framebuffer DPI (driver) | PSRAM | 750 KB |
| Fondo del asistente (3 buffers rotativos) | PSRAM | 3 × 750 KB, liberados con `release_setup_ui` |
| Capas del logotipo | PSRAM | 4 × 49 KB, liberadas al terminar el fundido |
| Fuentes, imágenes, fondos fuente | flash (rodata) | ≈2,5 MB |

Las pantallas del asistente se crean la primera vez que se visitan y se
conservan; se destruyen todas al terminar. Los estilos son compartidos: un
cambio de tema reescribe ≈40 estilos y no recrea objetos.

## Persistencia

Un blob de 16 bytes (`advenya`/`setup`) que NVS escribe de forma atómica.

| Evento | Escritura |
|---|---|
| llegar a una pantalla (etapa) | inmediata |
| aceptar privacidad / terminar | inmediata |
| cambiar idioma, tema, color o fondo | diferida 1,5 s sin cambios |
| contenido idéntico al guardado | ninguna |

Al arrancar se normaliza el estado (valores fuera de rango, etapa posterior a
privacidad sin aceptación, etc.) y se continúa en la etapa guardada con las
elecciones restauradas.
