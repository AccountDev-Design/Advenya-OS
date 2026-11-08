/*
 * ADVENYA OS · contenido de los documentos legales
 *
 * PENDIENTE: aqui se enlazaran los textos OFICIALES aprobados. No se ha
 * redactado ningun texto legal provisional a proposito.
 */
#include "legal_docs.h"

const adv_legal_doc_t adv_legal_docs[ADV_LEGAL_DOC_COUNT] = {
    [ADV_LEGAL_PRIVACY]     = {.title = TXT_PRIVACY_ITEM_TITLE, .version = NULL, .text = {NULL}},
    [ADV_LEGAL_PERMISSIONS] = {.title = TXT_PERMS_ITEM_TITLE,   .version = NULL, .text = {NULL}},
    [ADV_LEGAL_TERMS]       = {.title = TXT_TERMS_ITEM_TITLE,   .version = NULL, .text = {NULL}},
};
