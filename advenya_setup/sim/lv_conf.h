/*
 * lv_conf.h del SIMULADOR. Replica los valores de sdkconfig.defaults que
 * afectan a la interfaz (16 bits, 33 ms, mismas cachés y widgets).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 160
#define LV_USE_OS LV_OS_NONE
#define LV_DRAW_SW_SHADOW_CACHE_SIZE 16
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 8
#define LV_CACHE_DEF_SIZE 0
#define LV_IMAGE_HEADER_CACHE_DEF_CNT 16
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (48 * 1024)
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 1
#define LV_USE_ASSERT_OBJ 1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LABEL 1
#define LV_USE_IMAGE 1
#define LV_USE_SPAN 1
#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_BAR 0
#define LV_USE_BUTTON 1
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_BUILD_EXAMPLES 0

#endif
