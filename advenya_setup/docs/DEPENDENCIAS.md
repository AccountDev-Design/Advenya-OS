# Dependencias y versiones fijadas

| Dependencia | Versión | Cómo se fija |
|---|---|---|
| ESP-IDF | **v5.5.5** (`ff1bac0a`) | `main/idf_component.yml` → `idf: "==5.5.5"`; `dependencies.lock` |
| Toolchain | riscv32-esp-elf GCC 14.2.0 (esp-14.2.0_20260121) | la instala `install.sh` de IDF 5.5.5 |
| LVGL | **v9.2.2** (commit `7f07a129e8d77f4984fff8e623fd5be18ff42e74`) | `idf_component.yml` (fuente git, commit exacto) + `dependencies.lock` |
| Inter | 4.1 (OFL 1.1) | fuentes ya convertidas en `components/advenya_assets/src/fonts/`; licencia en `assets/fonts/` |

No se usan `esp_lvgl_port`, `esp_lcd_st7701` ni `esp_lcd_touch_gt911`: el
port de LVGL y los drivers (portados de Flex OS Ultra) son propios, para no
depender de versiones de terceros y mantener exactamente la secuencia
verificada en la placa.

## LVGL desde git o desde el registro

El manifiesto apunta al repositorio oficial de LVGL con un commit exacto, de
modo que la compilación solo necesita acceso a GitHub. Si se prefiere el
registro de Espressif, la línea equivalente es:

```yaml
  lvgl/lvgl: "==9.2.2"
```

(en `main/idf_component.yml` y en los `idf_component.yml` de
`advenya_assets`, `advenya_lvgl_port` y `advenya_ui`). Después:
`idf.py reconfigure`.

## Herramientas de generación (solo para regenerar recursos)

| Herramienta | Versión usada |
|---|---|
| Python | 3.9+ (probado con 3.13) |
| numpy / Pillow | 2.5.3 / 12.3.0 |
| Node.js + lv_font_conv | 22 / **1.5.3** |

## Simulador

CMake ≥ 3.16, GCC o Clang con AddressSanitizer/UBSan (probado con GCC 13.3),
LVGL 9.2.2 (se toma de `managed_components/` tras compilar el firmware o se
descarga con el mismo commit).
