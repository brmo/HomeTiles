#include "src/ui/boot_splash.h"

#include <lvgl.h>

#include "src/core/firmware_version.h"
#include "src/devices/device.h"
#include "src/fonts/ui_fonts.h"
#include "src/ui/hometiles_logo.h"

namespace BootSplash {
namespace {

lv_obj_t* g_overlay = nullptr;
lv_obj_t* g_bar = nullptr;
lv_obj_t* g_status_label = nullptr;

// Gleiche Kachel-Bild-Logik wie create_hometiles_logo_mark() in
// tab_settings.cpp (dort static/file-lokal, daher hier dupliziert statt
// exportiert -- ist nur der lv_image_create()+scale-Aufruf, keine Logik).
lv_obj_t* create_logo(lv_obj_t* parent, int32_t size) {
  lv_obj_t* img = lv_image_create(parent);
  lv_obj_set_style_pad_all(img, 0, 0);
  lv_obj_set_style_border_width(img, 0, 0);
  lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
  lv_image_set_src(img, &hometiles_logo_dsc);
  lv_image_set_antialias(img, true);
  const uint32_t zoom = static_cast<uint32_t>(
      (static_cast<int64_t>(size) * 256) / hometiles_logo_dsc.header.w);
  lv_image_set_scale(img, zoom);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
  return img;
}

}  // namespace

void show() {
  if (g_overlay) return;

  g_overlay = lv_obj_create(lv_screen_active());
  lv_obj_set_size(g_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(g_overlay, 0, 0);
  lv_obj_set_style_bg_color(g_overlay, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(g_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(g_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_overlay, 0, 0);
  lv_obj_set_style_radius(g_overlay, 0, 0);
  lv_obj_set_style_pad_all(g_overlay, 0, 0);
  lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(g_overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(g_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(g_overlay, 24, 0);

  // Marke: Icon links, rechts daneben Titel+Version -- identischer Aufbau
  // wie im System-Popup.
  lv_obj_t* brand = lv_obj_create(g_overlay);
  lv_obj_set_style_bg_opa(brand, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(brand, 0, 0);
  lv_obj_set_style_pad_all(brand, 0, 0);
  lv_obj_clear_flag(brand, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(brand, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brand, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(brand, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(brand, 18, 0);
  create_logo(brand, 100);

  lv_obj_t* brand_text = lv_obj_create(brand);
  lv_obj_set_style_bg_opa(brand_text, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(brand_text, 0, 0);
  lv_obj_set_style_pad_all(brand_text, 0, 0);
  lv_obj_clear_flag(brand_text, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(brand_text, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brand_text, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(brand_text, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(brand_text, 2, 0);

  lv_obj_t* title = lv_label_create(brand_text);
  lv_label_set_text(title, "HomeTiles");
  lv_obj_set_style_text_font(title, &ui_font_40, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  lv_obj_t* version_caption = lv_label_create(brand_text);
  lv_label_set_text(version_caption, FW_VERSION);
  lv_obj_set_style_text_font(version_caption, &ui_font_24, 0);
  lv_obj_set_style_text_color(version_caption, lv_color_hex(0xA8A8A8), 0);

  lv_obj_t* device_label = lv_label_create(g_overlay);
  lv_label_set_text(device_label, Device::displayName());
  lv_obj_set_style_text_font(device_label, &ui_font_24, 0);
  lv_obj_set_style_text_color(device_label, lv_color_hex(0xA8A8A8), 0);

  // Breite Content-Spalte fuers Untergeschoss (Status+Balken): gleiche
  // max_width/Seitenabstand wie settings_popup_content im System-Popup,
  // damit der Balken exakt so breit wie die dortigen Buttons wird -- auf
  // dem 720px breiten B4 genauso wie auf den 1280px-Geraeten.
  lv_obj_t* bottom = lv_obj_create(g_overlay);
  lv_obj_set_style_bg_opa(bottom, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bottom, 0, 0);
  // lv_obj_create()'s Standardgroesse ist ein festes 100x50-dp-Kaestchen,
  // keine Content-Groesse -- ohne explizite Hoehe wuerde das hier Status-
  // label+Balken abschneiden statt sie zu umschliessen.
  lv_obj_set_width(bottom, LV_PCT(100));
  lv_obj_set_height(bottom, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(bottom, 660, 0);
  lv_obj_set_style_pad_hor(bottom, 20, 0);
  lv_obj_set_style_pad_row(bottom, 12, 0);
  lv_obj_clear_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  g_status_label = lv_label_create(bottom);
  lv_label_set_text(g_status_label, "");
  lv_obj_set_width(g_status_label, LV_PCT(100));
  lv_label_set_long_mode(g_status_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(g_status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(g_status_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xC8C8C8), 0);

  g_bar = lv_bar_create(bottom);
  lv_obj_set_size(g_bar, LV_PCT(100), 18);
  lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_bar, 9, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x2E7D32), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(g_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(g_bar, 9, LV_PART_INDICATOR);
  lv_bar_set_range(g_bar, 0, 100);
  lv_bar_set_value(g_bar, 0, LV_ANIM_OFF);
}

void setProgress(uint8_t percent, const char* status_text) {
  if (!g_overlay) return;
  if (percent > 100) percent = 100;
  if (g_bar) lv_bar_set_value(g_bar, percent, LV_ANIM_OFF);
  if (status_text && g_status_label) lv_label_set_text(g_status_label, status_text);
  // Tabs/Popups werden waehrend des Boots auf denselben aktiven Screen
  // gebaut -- als spaeter hinzugefuegte Geschwister wuerden sie sonst ueber
  // dem Overlay landen (LVGL zeichnet Kinder in Erzeugungsreihenfolge).
  lv_obj_move_foreground(g_overlay);
}

void hide() {
  if (!g_overlay) return;
  lv_obj_del(g_overlay);
  g_overlay = nullptr;
  g_bar = nullptr;
  g_status_label = nullptr;
}

}  // namespace BootSplash
