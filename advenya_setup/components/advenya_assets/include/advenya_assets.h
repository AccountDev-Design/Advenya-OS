/*
 * ADVENYA OS · recursos graficos y tipograficos del asistente de configuracion
 *
 * Todo lo que declara este archivo se GENERA con:
 *   tools/gen_assets.py  -> src/generated/ (logotipo en capas, iconos, banderas,
 *                           fondos, miniaturas, velo)
 *   tools/gen_fonts.sh   -> src/fonts/     (Inter 4.1, OFL 1.1, Latin-1 completo)
 */
#pragma once

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "../src/generated/assets_gen.h"

LV_FONT_DECLARE(adv_font_r14);
LV_FONT_DECLARE(adv_font_r16);
LV_FONT_DECLARE(adv_font_m16);
LV_FONT_DECLARE(adv_font_m20);
LV_FONT_DECLARE(adv_font_sb18);
LV_FONT_DECLARE(adv_font_sb20);
LV_FONT_DECLARE(adv_font_sb22);
LV_FONT_DECLARE(adv_font_b28);
LV_FONT_DECLARE(adv_font_b36);

#ifdef __cplusplus
}
#endif
