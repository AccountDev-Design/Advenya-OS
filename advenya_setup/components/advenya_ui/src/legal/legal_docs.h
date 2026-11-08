/*
 * ADVENYA OS · documentos legales del asistente
 *
 * ESTRUCTURA PREPARADA, SIN TEXTOS LEGALES. Los documentos oficiales
 * (Politica de privacidad, Permisos, Condiciones de uso) no se han
 * proporcionado y NO se inventan. Mientras `text[lang]` sea NULL la interfaz
 * muestra el aviso "documento pendiente" traducido.
 *
 * Como incorporarlos: ver legal/README.md en la raiz del proyecto.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../i18n/ui_i18n_gen.h"

/* Version del paquete legal que se registra al aceptar (0 = sin documentos
 * oficiales). Subirla obliga al nucleo a pedir una nueva aceptacion si lo
 * desea: el valor aceptado queda en adv_settings_t.legal_version_accepted. */
#define ADV_LEGAL_BUNDLE_VERSION 0

typedef enum {
    ADV_LEGAL_PRIVACY = 0,
    ADV_LEGAL_PERMISSIONS,
    ADV_LEGAL_TERMS,
    ADV_LEGAL_DOC_COUNT
} adv_legal_doc_id_t;

typedef struct {
    adv_txt_t title;                        /* titulo traducido de la tarjeta  */
    const char *version;                    /* p. ej. "1.0 (2027-01-15)" o NULL */
    const char *text[ADV_LANG_COUNT];       /* texto UTF-8 por idioma o NULL    */
} adv_legal_doc_t;

extern const adv_legal_doc_t adv_legal_docs[ADV_LEGAL_DOC_COUNT];
