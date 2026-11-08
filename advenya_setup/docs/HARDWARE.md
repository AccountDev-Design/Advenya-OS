# Hardware: GUITION JC4880P443C_I_W y su adaptación a ESP-IDF 5.5.5

## 1. Fuente de los datos

Todos los datos de hardware salen de **Flex OS Ultra**, repositorio
`AccountDev-Design/Flex-OS-Ecosistema`, rama `claude/flexos-ota-system-blq9g9-2csacx`
(commit `5dbc949`). La rama de referencia **no se ha modificado**.

| Archivo de Flex OS Ultra | Qué se tomó |
|---|---|
| `FlexOS_Ultra/FlexOS_Ultra_HAL.h:85` `ST7701_INIT[]` | tabla DCS del ST7701 (40 filas, copiada byte a byte) |
| `FlexOS_Ultra/FlexOS_Ultra_HAL.h:164` `flexPanelInit()` | LDO, bus DSI, canal DBI, panel DPI, timings, reset, SLPOUT/DISPON |
| `FlexOS_Ultra/FlexOS_Ultra_HAL.h:97-125` | motivo de `CONFIG_LCD_DSI_ISR_CACHE_SAFE` y callback de fin de copia en IRAM |
| `FlexOS_Ultra/FlexOS_Ultra_HAL.h:136` `setBacklight()` | PWM 20 kHz / 8 bits y mapeo 5..100 % → duty 25..255 |
| `FlexOS_Ultra/FlexOS_Ultra_HAL.h:284` `panelDisplayOff()` | DCS 0x28/0x29 (DISPOFF/DISPON) |
| `FlexOS_Ultra/FlexOS_Ultra_HAL.h:395-569` | GT911: detección 0x5D/0x14, resolución 0x8048, lectura 0x814E/0x8150, recuperación del bus I²C |
| `FlexOS_Ultra/FlexOS_Ultra_Gfx.h:121` `flxFlush()` | contrato de presentación: `draw_bitmap` + espera acotada del callback (120 ms) |
| `FlexOS_Ultra/FlexOS_Ultra_Gfx.h:259` `flxGfxInit()` | buffers alineados a 64 B (línea de caché de la PSRAM) |
| `FlexOS_Ultra/FlexOS_Ultra_Types.h:289-302` | resolución 480×800 y pines |
| `FlexOS_Ultra/FlexOS_Ultra_Types.h:336-352` | el INT del GT911 **no está confirmado** (no se usa) |
| `FlexOS_Ultra/FlexOS_Ultra.ino:30-33` | CPU 360 MHz, flash 16 MB QIO 80 MHz, PSRAM habilitada, core Arduino 3.2.1 |
| `docs/BUS-I2C-COMPARTIDO.md` | el bus GPIO7/8 es compartido (GT911, ES8311, IMU opcional) |

`tools/check_hw_vs_flex.py` comprueba de forma automática que tabla DCS,
LDO, bus DSI, timings, pines y resolución coinciden con Flex (resultado
actual: **todo coincide**).

## 2. Hardware identificado

| Elemento | Valor | Estado |
|---|---|---|
| SoC | ESP32-P4, 2 núcleos HP | verificado (Flex) |
| CPU | 360 MHz | verificado (Flex) |
| Flash | 16 MB, QIO | verificado (Flex) |
| PSRAM | habilitada (en encapsulado del P4) | verificado (Flex); capacidad exacta **no consta** |
| Pantalla | 480×800 nativo, retrato, RGB565 | verificado |
| Controlador | ST7701 por MIPI-DSI, 2 lanes @ 500 Mbps | verificado |
| PHY MIPI | LDO interno canal 3 a 2500 mV | verificado |
| Vídeo DPI | 34 MHz; HSYNC 12/42/42; VSYNC 2/8/166 (≈60,5 Hz) | verificado |
| Reset panel | GPIO5 (H 5 ms, L 10 ms, H 120 ms) | verificado |
| Retroiluminación | GPIO23, PWM 20 kHz 8 bits | verificado |
| Táctil | GT911 I²C 400 kHz, SDA GPIO7, SCL GPIO8, RST GPIO3, dir. 0x5D o 0x14 | verificado |
| INT del GT911 | — | **pendiente de verificación**: no se usa (sondeo, como Flex) |
| Revisión del chip | < 3.0 (deducida) | **pendiente de verificación en placa** (ver §4) |
| Audio ES8311, Wi-Fi C6, etc. | — | fuera de alcance; no se inicializan |

## 3. Qué es de hardware y qué es de Flex OS Ultra

Se reutiliza **solo** la capa de hardware: encendido del panel, presentación
de píxeles y lectura del táctil (incluida su recuperación del bus). Quedan
fuera, por ser propios de Flex OS Ultra: motor gráfico (`FlexOS_Ultra_Gfx.h`
salvo el contrato de flush), compositor, fuentes, iconos, escritorio, apps,
OTA, red, LittleFS, suspensión/apagado, teclado multitáctil
(`gtPollMulti`), gesto de 2 dedos y la detección de módulos I²C.

## 4. Adaptaciones a ESP-IDF 5.5.5

| Flex (Arduino core 3.2.1 / IDF 5.4) | ADVENYA (ESP-IDF 5.5.5) |
|---|---|
| `pinMode/digitalWrite/delay` | `gpio_config/gpio_set_level/vTaskDelay` |
| `pixel_format = RGB565` (obsoleto en 5.5) | `in_color_format = LCD_COLOR_FMT_RGB565` |
| `phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT` | `MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT_LEGACY` (PLL_F20M, el mismo reloj) para rev < 3; `..._DEFAULT` (XTAL) si se compila para rev ≥ 3 |
| `ledcAttach/ledcWrite` | driver `ledc` (timer 0, canal 0) |
| `Wire` con plazo de 8 ms | `i2c_master_bus`/`i2c_master_dev`, plazo de 8 ms por transacción |
| recuperación con `pinMode`/`digitalWrite` | GPIO en drenador abierto, 9 pulsos de SCL, STOP manual, se vuelve a crear el bus |
| framebuffer propio + `flxFlush()` | LVGL en modo parcial (2 buffers en RAM interna) + `draw_bitmap` por DMA2D + `flush_wait_cb` con semáforo y plazo de 120 ms |
| `CONFIG_LCD_DSI_ISR_IRAM_SAFE` (nombre en 5.4) | `CONFIG_LCD_DSI_ISR_CACHE_SAFE` + `CONFIG_DMA2D_ISR_IRAM_SAFE` |
| fallos con `Serial.println` | `esp_err_t`, `ESP_GOTO_ON_ERROR` y liberación en orden inverso (`adv_board_display_deinit`) |

### Revisión del chip (decisión documentada)

IDF 5.5 compila por defecto **solo** para ESP32-P4 rev ≥ 3.0, y las revisiones
< 3.0 y ≥ 3.0 no son compatibles entre sí. Flex OS Ultra funciona en esta
placa con el core Arduino 3.2.1 (IDF 5.4), que únicamente soporta rev < 3.0;
por tanto la placa de referencia lleva un chip rev 0.x/1.x y el proyecto fija
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` (que además limita la CPU a 360 MHz, el
mismo valor que Flex). El firmware imprime la revisión real al arrancar
(`ESP32-P4 rev vX.Y`). Si fuese ≥ 3.0: `idf.py menuconfig` → *Component
config → Hardware Settings → Chip revision* y desmarcar la opción.

### Velocidad de la flash

Flex usa 80 MHz. En IDF 5.5 los 80 MHz solo se ofrecen con revisión mínima
≥ v0.1; para no excluir chips v0.0 queda en 40 MHz (por defecto). Afecta solo
a la lectura de código/datos desde flash (los fondos se leen una vez por
cambio). Con chip ≥ v0.1: `CONFIG_ESP32P4_REV_MIN_1=y` y
`CONFIG_ESPTOOLPY_FLASHFREQ_80M=y`.

### PSRAM

`CONFIG_SPIRAM=y` con la velocidad por defecto de IDF 5.5 para el P4
(200 MHz, modo HEX). No es un valor de Flex (Arduino no lo expone en el
sketch); si la placa mostrase errores de PSRAM, bajar a 80 MHz.

## 5. Pendiente de verificación física

Nada de lo anterior se ha probado en la placa (ver `VALIDACION.md`).
Puntos concretos a observar en el primer arranque:

1. Log `ESP32-P4 rev vX.Y` (revisión del chip).
2. `panel ST7701 480x800 por MIPI-DSI ... listo` y ausencia de
   `underrun` del driver DSI.
3. `GT911 en 0x5D/0x14, ID '911', configurado 480x800` (si la resolución
   configurada difiere se escala; si los ejes salen cambiados o espejados,
   `menuconfig → ADVENYA · Placa → GT911`).
4. Que la retroiluminación se enciende **después** del primer fotograma.
