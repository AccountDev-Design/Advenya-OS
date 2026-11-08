# Integración con el futuro núcleo de ADVENYA OS

El asistente **no** crea escritorio ni pantalla final. Su único punto de
salida es `adv_handoff_dispatch()` (componente `advenya_handoff`), que se
llama **una vez por arranque**, desde la tarea de LVGL y con el bloqueo de
LVGL tomado:

| Caso | Motivo | Qué hay en pantalla al entregar |
|---|---|---|
| el usuario termina personalización | `ADV_HANDOFF_SETUP_JUST_COMPLETED` | el fondo del asistente (las pantallas ya están destruidas) |
| arranque con setup completo | `ADV_HANDOFF_SETUP_ALREADY_DONE` | el logotipo de arranque, fijo |

## Lo que recibe el núcleo (`adv_handoff_t`)

```c
typedef struct {
    adv_handoff_reason_t reason;
    adv_settings_t settings;     /* copia completa del estado persistido      */
    const char *language_code;   /* "es", "en", "pt-BR", "fr", "de"           */
    bool dark;                   /* tema efectivo                             */
    uint32_t accent_rgb;         /* 0xRRGGBB                                  */
    uint8_t wallpaper_index;     /* 0..4                                      */
    void *lv_display;            /* lv_display_t*                             */
    void *lv_indev;              /* lv_indev_t* (táctil)                      */
    void (*release_setup_ui)(void);
} adv_handoff_t;
```

`settings` incluye además `theme_mode` (claro/oscuro/automático),
`privacy_accepted`, `legal_version_accepted` y `personalization_deferred`.

## Cómo se conecta el núcleo

1. Crear un componente propio (p. ej. `components/advenya_core`) con
   `REQUIRES advenya_handoff`.
2. Registrar su punto de entrada **antes** de `adv_ui_start()` en `main.c`:

   ```c
   #include "advenya_handoff.h"
   void advenya_core_main(const adv_handoff_t *h);   /* del núcleo */
   ...
   adv_handoff_set_core(advenya_core_main);
   adv_lvgl_lock(0);
   adv_ui_start(adv_lvgl_display(), adv_lvgl_indev());
   adv_lvgl_unlock();
   ```

3. En `advenya_core_main()`: copiar lo que necesite de `h` (el puntero no
   sobrevive a la llamada), crear su primera pantalla con LVGL y, cuando esté
   dibujada, llamar a `h->release_setup_ui()`. Esa función funde y retira el
   logotipo o el fondo del asistente y libera ≈2,3 MB de PSRAM. Es
   idempotente.

Se usa registro explícito (no un símbolo débil) porque con bibliotecas
estáticas el enlazador puede quedarse con la versión débil sin avisar.

Sin núcleo registrado, la entrega por defecto solo la anota en el log y la
pantalla se queda como está.

## Reglas para el núcleo

- LVGL solo desde la tarea `lvgl` o con `adv_lvgl_lock()/adv_lvgl_unlock()`.
- El bus I²C (GPIO7/8) es compartido: usar `adv_board_i2c_bus()` desde la misma
  tarea que el táctil.
- Si sube `ADV_LEGAL_BUNDLE_VERSION`, comparar con
  `settings.legal_version_accepted` para pedir una nueva aceptación.
- Para volver a lanzar el asistente: `adv_settings_reset()` y reiniciar.
