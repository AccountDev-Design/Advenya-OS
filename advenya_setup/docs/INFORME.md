# Informe de entrega · ADVENYA OS · primera experiencia

## 1. Resumen

Proyecto nuevo `advenya_setup` (ESP-IDF 5.5.5 + LVGL 9.2.2) para la placa
GUITION JC4880P443C_I_W con: animación del logotipo al encender, bienvenida,
selección de idioma (5 idiomas), privacidad/permisos/condiciones con
aceptación explícita y personalización (tema, color, fondo), persistencia en
NVS con reanudación tras corte y un punto de integración para el núcleo.

| Comprobación | Resultado |
|---|---|
| Compilación ESP-IDF 5.5.5 (esp32p4) | **correcta**, 0 avisos en código propio, binario 2,9 MB (51 % libre) |
| Datos de hardware vs Flex OS Ultra | **idénticos** (tabla DCS, LDO, DSI, timings, pines) |
| Pruebas simuladas (host) | **62/62 correctas**, sin errores de ASan/UBSan, memoria estable |
| Pruebas en la placa | **pendientes** (no había placa en este entorno) |

## 2. Archivos y componentes creados

| Ruta | Contenido |
|---|---|
| `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `dependencies.lock` | proyecto, configuración, particiones (factory 6 MB), versiones bloqueadas |
| `main/` | `main.c` (orden de arranque), `idf_component.yml` (IDF ==5.5.5, LVGL commit fijo), `Kconfig.projbuild` |
| `components/advenya_board/` | `board_display_st7701.c`, `st7701_init_table.h`, `board_backlight.c`, `board_touch_gt911.c`, `advenya_board.h`, `advenya_board_pins.h`, `Kconfig` |
| `components/advenya_lvgl_port/` | `advenya_lvgl_port.c` (display, flush DMA2D, indev, tarea, mutex), `advenya_diag.c`, `Kconfig` |
| `components/advenya_settings/` | `settings_core.c` (portable), `settings_nvs.c` (NVS + tarea de escritura) |
| `components/advenya_ui/` | `ui_core.c`, `ui_boot.c`, `ui_wizard.c`, `ui_scr_welcome/language/privacy/personalize.c`, `ui_theme.c`, `ui_i18n.c`, `ui_backdrop.c`, `ui_legal.c`, `ui_widgets.c`, `ui_platform_esp.c`, `i18n/` (generado), `legal/` |
| `components/advenya_assets/` | 9 fuentes Inter (Latin-1), logotipo en 4 capas + mapas de revelado, lockups, 11 iconos, 5 banderas, 3 vistas de tema, 5 fondos, 5 miniaturas, velo, halo |
| `components/advenya_handoff/` | `adv_handoff_t`, registro del núcleo y entrega por defecto |
| `translations/` | `languages.json`, `es.json`, `en.json`, `pt-BR.json`, `fr.json`, `de.json` |
| `assets/` | `reference/` (las 6 imágenes originales), `generated/` (PNG de control), `fonts/` (licencia OFL) |
| `legal/README.md` | cómo incorporar los documentos oficiales |
| `tools/` | `gen_assets.py`, `gen_fonts.sh`, `gen_i18n.py`, `check_hw_vs_flex.py` |
| `sim/` | simulador de host (`sim_main.c`, `platform_sim.c`, `settings_file.c`, `lv_conf.h`, `CMakeLists.txt`, `run_tests.py`) |
| `docs/` | este informe, `HARDWARE.md`, `ARQUITECTURA.md`, `DISENO.md`, `INTEGRACION_NUCLEO.md`, `DEPENDENCIAS.md`, `VALIDACION.md`, `validacion_simulador.log`, `capturas/` |

## 3. Hardware identificado y archivos de Flex OS Ultra usados

Referencia: `Flex-OS-Ecosistema`, rama `claude/flexos-ota-system-blq9g9-2csacx`
(commit `5dbc949`), sin modificar. Archivos: `FlexOS_Ultra/FlexOS_Ultra_HAL.h`,
`FlexOS_Ultra/FlexOS_Ultra_Gfx.h` (solo el contrato de flush),
`FlexOS_Ultra/FlexOS_Ultra_Types.h`, `FlexOS_Ultra/FlexOS_Ultra.ino` (entorno)
y `docs/BUS-I2C-COMPARTIDO.md`. Detalle con números de línea en
`docs/HARDWARE.md`.

ESP32-P4 a 360 MHz · flash 16 MB QIO · PSRAM · ST7701 por MIPI-DSI (2 lanes,
500 Mbps, LDO canal 3 a 2,5 V) · DPI 34 MHz 480×800 RGB565, HSYNC 12/42/42,
VSYNC 2/8/166 · reset GPIO5 · retroiluminación GPIO23 PWM 20 kHz · GT911 I²C
400 kHz en GPIO7/8, reset GPIO3, 0x5D/0x14.

Pendiente de verificar: GPIO del INT del GT911 (no se usa, igual que Flex),
revisión exacta del chip (deducida < 3.0), capacidad de la PSRAM.

## 4. Adaptaciones a ESP-IDF 5.5.5

- API de LCD de 5.5: `in_color_format` en lugar de `pixel_format`; reloj
  del PHY DSI elegido por revisión (`PLLREF_CLK_SRC_DEFAULT_LEGACY` = PLL_F20M,
  el mismo de Flex en 5.4).
- Revisión de chip: IDF 5.5 compila por defecto solo para rev ≥ 3.0; se fija
  rev < 3.0 (la de la placa de referencia) y se documenta cómo cambiarlo.
- Flash a 40 MHz (80 MHz exige rev mínima v0.1 en 5.5; documentado).
- I²C con el driver nuevo `i2c_master`, plazos de 8 ms y recuperación del bus
  reimplementada con GPIO en drenador abierto.
- LEDC del IDF en lugar de `ledcAttach`.
- `CONFIG_LCD_DSI_ISR_CACHE_SAFE` (el panel no
  destella al escribir NVS) y callback en IRAM con contexto en RAM interna.
- LVGL con port propio: 2 buffers parciales en RAM interna alineados a 64 B,
  copia por DMA2D, espera con semáforo y plazo (sin bucles de espera activa).
- Gestión de errores con `esp_err_t` y liberación ordenada de recursos.

## 5. Versiones de dependencias

ESP-IDF **v5.5.5**, GCC riscv32-esp-elf **14.2.0**, LVGL **v9.2.2**
(`7f07a129`), Inter **4.1**, lv_font_conv **1.5.3** (solo generación). Ver
`docs/DEPENDENCIAS.md`.

## 6. Pantallas y animaciones implementadas

- **Arranque** (~3,0 s): A → 3 ondas → órbita → destello de la estrella →
  pausa → fundido de 300 ms; capas extraídas del logotipo original.
- **Bienvenida**, **Idioma**, **Privacidad**, **Personalización** según las
  referencias; documento legal en panel inferior.
- **Fluid Motion**: ±20 px, salida 260 ms ease-in, entrada 300 ms ease-out
  (410 ms en total), selección 180 ms, crossfade de fondos 320 ms, pulsación
  sutil 120 ms, navegación bloqueada durante transiciones, sin indicadores de
  progreso. Retroceso con gesto a la derecha.
- Decisiones de interpretación en `docs/DISENO.md` §«Decisiones».

## 7. Resultado de compilación

`idf.py build` → correcto. `advenya_setup.bin` 0x2EA510 B en partición de
0x600000 B (51 % libre). DIRAM 28,6 % usada. 0 avisos en código propio.

## 8. Pruebas realizadas y pendientes

- **Compilación**: hecha (ESP-IDF 5.5.5 real).
- **Simuladas**: hechas, 62/62 (navegación, 5 idiomas, 3 temas, 8 acentos,
  5 fondos, persistencia y reanudación tras corte, bloqueo de navegación
  duplicada, entrega al núcleo, estabilidad de memoria con 30 ciclos).
- **Físicas**: **ninguna**. Lista F1–F10 en `docs/VALIDACION.md`
  (inicialización real del panel y táctil, FPS en la placa, NVS real,
  memoria real).

## 9. Rama y commit

Rama `claude/advenya-first-boot-setup` en `AccountDev-Design/Advenya-OS`
(sin merge a `main`). El commit exacto figura en el historial de la rama
(`git log`), y se indica también en el mensaje de entrega.

## 10. Compilar y flashear

```bash
cd advenya_setup
idf.py set-target esp32p4
idf.py build
idf.py -p <PUERTO> flash monitor
```

Pruebas simuladas: `python3 sim/run_tests.py`. Más detalle en `README.md`.
