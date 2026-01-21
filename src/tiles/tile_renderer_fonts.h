#pragma once

#include <lvgl.h>
#include "src/fonts/ui_fonts.h"

#define FONT_TITLE (&ui_font_24)

#if defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  #define FONT_VALUE (&lv_font_montserrat_40)
#elif defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  #define FONT_VALUE (&lv_font_montserrat_48)
#else
  #define FONT_VALUE (LV_FONT_DEFAULT)
#endif

#define FONT_UNIT (&ui_font_24)
