#!/usr/bin/env bash
# Genera las fuentes LVGL (Inter, OFL 1.1) con el rango Latin-1 completo, que
# cubre los cinco idiomas del asistente (es, en, pt-BR, fr, de): á é í ó ú ñ ü
# ç ã õ â ê ô à è ù ë ï î û ÿ ä ö ß ¡ ¿ « » ...
#
# Requisitos: Node.js y lv_font_conv 1.5.3 (npm i -g lv_font_conv@1.5.3)
# Uso: INTER_DIR=/ruta/a/Inter-4.1/extras/ttf tools/gen_fonts.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/components/advenya_assets/src/fonts"
INTER_DIR="${INTER_DIR:?define INTER_DIR con la carpeta extras/ttf de Inter 4.1}"
CONV="${LV_FONT_CONV:-lv_font_conv}"
RANGE="0x20-0x7E,0xA0-0xFF,0x152-0x153,0x2013-0x2014,0x2018-0x201E,0x2022,0x2026,0x20AC"
mkdir -p "$OUT"
gen() { # nombre peso tamano
  local name="$1" weight="$2" size="$3"
  "$CONV" --no-compress --no-prefilter --bpp 4 --size "$size" --format lvgl \
    --lv-include "lvgl.h" --lv-font-name "$name" \
    --font "$INTER_DIR/Inter-$weight.ttf" -r "$RANGE" -o "$OUT/$name.c"
  # rutas locales fuera del comentario generado (reproducible entre maquinas)
  sed -i -e "s#$INTER_DIR#Inter-4.1/extras/ttf#g" -e "s#$OUT/##g" "$OUT/$name.c"
  echo "  $name ($weight $size px)"
}
gen adv_font_r14 Regular 14
gen adv_font_r16 Regular 16
gen adv_font_m16 Medium 16
gen adv_font_m20 Medium 20
gen adv_font_sb18 SemiBold 18
gen adv_font_sb20 SemiBold 20
gen adv_font_sb22 SemiBold 22
gen adv_font_b28 Bold 28
gen adv_font_b36 Bold 36
