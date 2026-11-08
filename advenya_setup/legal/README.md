# Documentos legales de ADVENYA (pendientes)

El asistente tiene preparada la estructura para mostrar tres documentos:

| Id | Tarjeta | Texto |
|---|---|---|
| `ADV_LEGAL_PRIVACY` | Privacidad | **pendiente** |
| `ADV_LEGAL_PERMISSIONS` | Permisos | **pendiente** |
| `ADV_LEGAL_TERMS` | Condiciones de uso | **pendiente** |

No se ha redactado ningún texto legal, ni siquiera provisional. Mientras falte
un documento, su panel muestra el aviso traducido *«El documento oficial aún
no se ha incorporado a esta versión…»* y *«Estado: pendiente»*.

## Cómo incorporar los documentos oficiales

1. Guardar aquí los textos aprobados, uno por documento e idioma (UTF-8), p.
   ej. `legal/privacy/es.txt`, `legal/privacy/en.txt`, …
2. Enlazarlos en
   `components/advenya_ui/src/legal/legal_docs.c` (campo `text[ADV_LANG_*]`)
   y rellenar `version` (p. ej. `"1.0 · 2027-01-15"`). Para textos largos se
   pueden incrustar con `EMBED_TXTFILES` en el `CMakeLists.txt` del
   componente y apuntar `text[]` al símbolo `_binary_..._start`
   (terminado en NUL por `EMBED_TXTFILES`).
3. Subir `ADV_LEGAL_BUNDLE_VERSION` en `legal_docs.h`. El valor aceptado se
   guarda en `adv_settings_t.legal_version_accepted`; el núcleo puede pedir de
   nuevo la aceptación si no coincide.
4. Revisar que caben los caracteres: las fuentes incluyen Latin-1 completo,
   comillas tipográficas, guiones, «…» y «€».

El panel ya es desplazable, así que la longitud no es un problema.
