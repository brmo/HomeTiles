#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(ui_font_16);
LV_FONT_DECLARE(ui_font_20);
LV_FONT_DECLARE(ui_font_20_semibold);
LV_FONT_DECLARE(ui_font_24);
LV_FONT_DECLARE(ui_font_28);
LV_FONT_DECLARE(ui_font_32);
LV_FONT_DECLARE(ui_font_40);
LV_FONT_DECLARE(ui_font_48);
// Schlanke Clock-Zeichensaetze: Ziffern, Datumstrenner und AM/PM. Dadurch
// sind grosse Uhrzeiten moeglich, ohne pro Groesse einen ~1-MB-Vollfont
// einzubetten.
LV_FONT_DECLARE(clock_font_56);
LV_FONT_DECLARE(clock_font_64);
LV_FONT_DECLARE(clock_font_72);
LV_FONT_DECLARE(clock_font_80);
LV_FONT_DECLARE(clock_font_96);
