# Validación

Se distinguen tres niveles. **Ninguna función se ha probado en la placa
física**: no había placa conectada a este entorno.

| Nivel | Qué demuestra | Estado |
|---|---|---|
| Compilación (ESP-IDF 5.5.5, esp32p4) | que el firmware completo compila y enlaza con las APIs reales de IDF 5.5.5 y cabe en la partición | **hecho, correcto** |
| Pruebas simuladas (host) | que la interfaz real (mismo código C) navega, traduce, aplica temas, persiste y no pierde memoria | **hecho, 62/62 correctas** |
| Pruebas físicas (placa) | panel, DMA2D, táctil, NVS real, FPS reales, PSRAM | **pendiente** |

## 1. Compilación

```
ESP-IDF v5.5.5 · riscv32-esp-elf-gcc 14.2.0 · target esp32p4
idf.py set-target esp32p4 && idf.py build   -> EXIT 0
advenya_setup.bin 0x2ea510 B (2,9 MB) · partición factory 6 MB · 51 % libre
DIRAM usada 164 798 B de 576 464 (28,6 %)
avisos en código propio: 0 (3 avisos de LVGL upstream por atributos IRAM)
```

Configuración efectiva comprobada en `sdkconfig`: `ESP32P4_SELECTS_REV_LESS_V3=y`,
CPU 360 MHz, `SPIRAM=y` 200 MHz HEX, `LCD_DSI_ISR_CACHE_SAFE=y`,
`DMA2D_ISR_IRAM_SAFE` desactivado (ver ARQUITECTURA.md), `LV_COLOR_DEPTH=16`, `LV_DEF_REFR_PERIOD=33`,
`FREERTOS_HZ=1000`.

Comprobación de hardware contra la referencia (`tools/check_hw_vs_flex.py`):
tabla DCS del ST7701 (40 filas) idéntica; LDO, bus DSI, timings DPI, pines y
resolución idénticos a Flex OS Ultra.

## 2. Pruebas simuladas

`python3 sim/run_tests.py` compila `sim/` (LVGL 9.2.2 en el PC, con
AddressSanitizer + UBSan) y ejecuta los escenarios; cada escenario es un
proceso distinto y terminar el proceso equivale a cortar la alimentación. Log
completo: `docs/validacion_simulador.log`. Capturas: `docs/capturas/`.

| Escenario | Verifica |
|---|---|
| `boot_fresh` | animación activa, se retira a ~3,3 s, llega a bienvenida, etapa persistida |
| `flow` | avance con transición; doble toque durante la transición ignorado; 5 idiomas aplicados al instante (también en pantallas ocultas); idioma escrito una sola vez tras 1,5 s; gesto atrás; idioma conservado; privacidad no avanza sin casilla; los 3 apartados abren su documento; el gesto cierra el documento; «Más tarde» vuelve sin registrar aceptación; aceptación explícita registrada; tema oscuro y automático; 8 acentos; 5 fondos con crossfade; al retroceder se conserva casilla/tema/color/fondo; persistencia diferida |
| `resume` | tras «corte» continúa en personalización con fondo, tema, color y aceptación recuperados; entrega al núcleo con idioma/tema/acento/fondo; pantallas destruidas; `release_setup_ui` libera ~2,3 MB |
| `boot_complete` | con setup completo entrega directa, logotipo fijo hasta `release_setup_ui`, ninguna pantalla creada |
| `powercut_lang` + `_resume` | corte tras elegir alemán: continúa en idioma con el alemán aplicado |
| `stress` | 30 ciclos completos (navegación, idiomas, documento, temas, acentos, fondos, gestos): heap +9 B (umbral ±4 KB) |
| `matrix` | 40 capturas: 4 pantallas × 5 idiomas × claro/oscuro (revisadas: sin recortes) |

Resultados destacados:

- **Memoria**: sin crecimiento tras 30 ciclos (2 425 009 → 2 425 018 B);
  sin errores de ASan/UBSan.
- **Transición**: 410 ms de animación; en el host, 88 fotogramas con
  render medio de 0,6 ms (CPU del PC; **no** es representativo del P4).
- **Escrituras de estado**: 12 en el recorrido completo del escenario `flow`.
- Fallos reales encontrados y corregidos gracias a estas pruebas: un gesto de
  retroceso que empezaba sobre una fila la seleccionaba al soltar; textos
  largos (alemán, francés) recortados en privacidad; etiqueta de botón que
  pisaba la flecha; desplazamiento a la izquierda indefinido en `ui_boot.c`
  (UBSan).

Limitaciones del simulador: no ejecuta el driver DSI/DMA2D, ni el GT911, ni
NVS; el fondo se hornea de forma síncrona; los tiempos son del PC.

## 3. Pruebas físicas pendientes (lista para la placa)

| # | Prueba | Cómo observarla |
|---|---|---|
| F1 | revisión del chip compatible con la compilación | log `ESP32-P4 rev vX.Y` (< 3.0) |
| F2 | panel inicializa sin destello blanco | retroiluminación tras `primer fotograma en el panel` |
| F3 | sin `underrun`/destellos al guardar en NVS | cambiar de pantalla repetidamente y mirar el log |
| F4 | GT911 detectado y coordenadas correctas | log `GT911 en 0x..`; tocar las esquinas de las filas |
| F5 | animación de arranque fluida | log `adv_diag` durante el arranque (`fps`, `render`) |
| F6 | transiciones a ≥ 30 FPS | `adv_diag` al navegar: `fps≈30`, `render` < 33 ms |
| F7 | crossfade de fondos sin bloqueo | tiempo de `adv_work` y `fps` al cambiar fondo |
| F8 | persistencia real | cortar la alimentación en cada pantalla y comprobar la reanudación |
| F9 | memoria | `adv_diag`: `int`/`psram` mínimos estables tras varios recorridos |
| F10 | recuperación del bus I²C | (opcional) perturbar SDA/SCL y ver `bus I2C recuperado` |

Si F6 no llega a 30 FPS en transiciones, palancas previstas: bajar
`CONFIG_ADV_LVGL_BUF_LINES`/subirlo según RAM, quitar la sombra del botón
(`ui_theme.c`), o pasar a 2 framebuffers DPI en modo directo.
