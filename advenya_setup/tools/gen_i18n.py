#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Genera la tabla de traducciones centralizada del asistente a partir de
`translations/*.json`.

  translations/languages.json  -> orden de idiomas, nombres nativos y claves
  translations/<codigo>.json   -> textos de cada idioma (las mismas claves)

Salida: components/advenya_ui/src/i18n/ui_i18n_gen.h / ui_i18n_gen.c

Falla (codigo 1) si a un idioma le falta una clave, si sobra alguna o si hay un
texto vacio, para que nunca llegue a la placa una pantalla a medio traducir.
Uso: python3 tools/gen_i18n.py
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TR = os.path.join(ROOT, "translations")
OUT = os.path.join(ROOT, "components", "advenya_ui", "src", "i18n")


def c_str(s):
    out = []
    for ch in s.encode("utf-8"):
        if ch == 0x22:
            out.append('\\"')
        elif ch == 0x5C:
            out.append("\\\\")
        elif 0x20 <= ch < 0x7F:
            out.append(chr(ch))
        else:
            out.append("\\x%02x\" \"" % ch)
    return '"' + "".join(out) + '"'


def main():
    meta = json.load(open(os.path.join(TR, "languages.json"), encoding="utf-8"))
    order, keys, names = meta["order"], meta["keys"], meta["native_names"]
    tables = {}
    errors = []
    for code in order:
        d = json.load(open(os.path.join(TR, code + ".json"), encoding="utf-8"))
        missing = [k for k in keys if k not in d]
        extra = [k for k in d if k not in keys]
        empty = [k for k in keys if k in d and not d[k].strip()]
        if missing or extra or empty:
            errors.append("%s: faltan=%s sobran=%s vacios=%s" % (code, missing, extra, empty))
        tables[code] = d
    if errors:
        print("\n".join(errors), file=sys.stderr)
        sys.exit(1)
    os.makedirs(OUT, exist_ok=True)
    ident = lambda c: c.upper().replace("-", "_")
    with open(os.path.join(OUT, "ui_i18n_gen.h"), "w", encoding="utf-8") as f:
        f.write("/* ARCHIVO GENERADO por tools/gen_i18n.py desde translations/ - NO EDITAR. */\n#pragma once\n\n")
        f.write("typedef enum {\n")
        for c in order:
            f.write("    ADV_LANG_%s,\n" % ident(c))
        f.write("    ADV_LANG_COUNT\n} adv_lang_t;\n\n")
        f.write("typedef enum {\n")
        for k in keys:
            f.write("    TXT_%s,\n" % k.upper())
        f.write("    TXT_COUNT\n} adv_txt_t;\n\n")
        f.write("extern const char *const adv_i18n_table[ADV_LANG_COUNT][TXT_COUNT];\n")
        f.write("extern const char *const adv_i18n_lang_code[ADV_LANG_COUNT];\n")
        f.write("extern const char *const adv_i18n_lang_native[ADV_LANG_COUNT];\n")
    with open(os.path.join(OUT, "ui_i18n_gen.c"), "w", encoding="utf-8") as f:
        f.write("/* ARCHIVO GENERADO por tools/gen_i18n.py desde translations/ - NO EDITAR. */\n")
        f.write("#include \"ui_i18n_gen.h\"\n\n")
        f.write("const char *const adv_i18n_lang_code[ADV_LANG_COUNT] = {%s};\n" %
                ", ".join(c_str(c) for c in order))
        f.write("const char *const adv_i18n_lang_native[ADV_LANG_COUNT] = {%s};\n\n" %
                ", ".join(c_str(names[c]) for c in order))
        f.write("const char *const adv_i18n_table[ADV_LANG_COUNT][TXT_COUNT] = {\n")
        for c in order:
            f.write("    [ADV_LANG_%s] = {\n" % ident(c))
            for k in keys:
                f.write("        [TXT_%s] = %s,\n" % (k.upper(), c_str(tables[c][k])))
            f.write("    },\n")
        f.write("};\n")
    print("i18n: %d idiomas x %d textos" % (len(order), len(keys)))


if __name__ == "__main__":
    main()
