# advenya_setup · primera experiencia de ADVENYA OS

Animación del logotipo al encender y asistente de configuración inicial
(bienvenida, idioma, privacidad y personalización) para la placa
**GUITION JC4880P443C_I_W** (ESP32-P4, pantalla MIPI-DSI 480×800 ST7701,
táctil GT911), con **ESP-IDF 5.5.5** y **LVGL 9.2.2**.

Alcance deliberadamente limitado: no hay escritorio, launcher, apps,
notificaciones, widgets ni tienda. Al terminar, el asistente entrega su estado
a un punto de integración para el futuro núcleo (`docs/INTEGRACION_NUCLEO.md`).

## Estructura

```
advenya_setup/
├── CMakeLists.txt · sdkconfig.defaults · partitions.csv · dependencies.lock
├── main/                     arranque (main.c), versión de IDF y LVGL fijadas
├── components/
│   ├── advenya_board/        capa de hardware: ST7701/DSI, retroiluminación, GT911
│   ├── advenya_lvgl_port/    LVGL ↔ hardware, tarea, diagnóstico (FPS/memoria)
│   ├── advenya_settings/     estado persistente (núcleo portable + NVS)
│   ├── advenya_ui/           animación de arranque y asistente (portable)
│   ├── advenya_assets/       fuentes e imágenes generadas
│   └── advenya_handoff/      punto de integración con el núcleo
├── translations/             textos de los 5 idiomas (JSON, fuente única)
├── assets/                   referencias originales y PNG generados
├── legal/                    estructura para los documentos oficiales
├── tools/                    generadores (assets, fuentes, i18n) y comprobación HW
├── sim/                      simulador de host y pruebas automáticas
└── docs/                     hardware, arquitectura, diseño, validación, informe
```

## Compilar y flashear

Requisitos: ESP-IDF **v5.5.5** instalado para `esp32p4`
(`./install.sh esp32p4` y `. ./export.sh`) y acceso a GitHub (LVGL se descarga
por git con un commit fijo; ver `docs/DEPENDENCIAS.md`).

```bash
cd advenya_setup
idf.py set-target esp32p4        # solo la primera vez (genera sdkconfig desde sdkconfig.defaults)
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor     # en Windows: -p COM5
```

El puerto es el USB-UART o el USB-Serial/JTAG de la placa. Para salir del
monitor: `Ctrl+]`.

**Antes del primer flasheo** lee la sección «Revisión del chip» de
`docs/HARDWARE.md`: el proyecto está configurado para ESP32-P4 rev < 3.0 (la
de la placa de referencia). El primer log del arranque muestra la revisión.

Opciones útiles (`idf.py menuconfig`):

- *ADVENYA · Asistente de configuración* → borrar el estado en cada arranque
  (para repetir el asistente durante el desarrollo).
- *ADVENYA · Placa* → brillo inicial y orientación del GT911.
- *ADVENYA · Port de LVGL* → líneas de buffer, núcleo/prioridad de la tarea,
  diagnóstico periódico.

Diagnóstico en el monitor cada 5 s:
`adv_diag: fps=… render=media/max us flush_wait_max=… | int … | psram … | pila lvgl …`

## Pruebas simuladas (en el PC)

```bash
python3 sim/run_tests.py          # compila sim/ y ejecuta todos los escenarios
```

Usa el Python del sistema con numpy y Pillow (no el entorno virtual de
ESP-IDF, que no trae Pillow). Genera capturas en `sim/out/` y hojas en
`docs/capturas/`. Ver
`docs/VALIDACION.md` para qué cubre y qué no.

## Regenerar recursos

```bash
python3 tools/gen_assets.py                       # logotipo en capas, iconos, banderas, fondos
python3 tools/gen_i18n.py                         # tras editar translations/*.json
INTER_DIR=…/Inter-4.1/extras/ttf tools/gen_fonts.sh   # fuentes (lv_font_conv 1.5.3)
python3 tools/check_hw_vs_flex.py ../ruta/Flex-OS-Ecosistema   # datos HW vs referencia
```

## Documentación

- `docs/INFORME.md` — informe de entrega
- `docs/HARDWARE.md` — hardware identificado y adaptación a ESP-IDF 5.5.5
- `docs/ARQUITECTURA.md` — componentes, hilos, presentación, memoria, persistencia
- `docs/DISENO.md` — pantallas, animaciones y decisiones de interpretación
- `docs/INTEGRACION_NUCLEO.md` — cómo toma el control el futuro núcleo
- `docs/DEPENDENCIAS.md` — versiones fijadas
- `docs/VALIDACION.md` — compilación, pruebas simuladas y pruebas físicas pendientes
