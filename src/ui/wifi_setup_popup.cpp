#include "src/ui/wifi_setup_popup.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "src/core/board_hal.h"
#include "src/core/config_manager.h"
#include "src/core/display_manager.h"
#include "src/core/i18n.h"
#include "src/devices/device.h"
#include "src/fonts/ui_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/ui_keyboard.h"

namespace {

const i18n::Strings& tr() {
  return i18n::strings(configManager.getConfig().language);
}

constexpr size_t kMaxScanResults = 24;
constexpr int32_t kRowHeight = 52;

struct ScanEntry {
  char ssid[33];
  int16_t rssi;
  bool open;
};

lv_obj_t* g_overlay = nullptr;         // dunkler Vollbild-Hintergrund (faengt Klicks ab)
lv_obj_t* g_card = nullptr;
lv_obj_t* g_title_label = nullptr;
lv_obj_t* g_list_view = nullptr;       // Ansicht 1: Scan-Status + Netzliste
lv_obj_t* g_list_container = nullptr;
lv_obj_t* g_scan_status_label = nullptr;
lv_obj_t* g_rescan_btn = nullptr;
lv_obj_t* g_entry_view = nullptr;      // Ansicht 2: SSID/Passwort + Tastatur
lv_obj_t* g_back_btn = nullptr;
lv_obj_t* g_connect_btn = nullptr;
lv_obj_t* g_ssid_ta = nullptr;
lv_obj_t* g_pass_ta = nullptr;
lv_obj_t* g_show_pass_cb = nullptr;
lv_obj_t* g_keyboard = nullptr;
lv_timer_t* g_scan_timer = nullptr;
ScanEntry g_results[kMaxScanResults];
size_t g_result_count = 0;
char g_selected_ssid[33] = {};
bool g_selected_open = false;
bool g_manual_mode = false;
bool g_restart_pending = false;

void populate_list();

void stop_scan_timer() {
  if (g_scan_timer) {
    lv_timer_del(g_scan_timer);
    g_scan_timer = nullptr;
  }
}

void close_popup() {
  stop_scan_timer();
  WiFi.scanDelete();
  if (g_overlay) {
    lv_obj_del(g_overlay);
  }
  g_overlay = nullptr;
  g_card = nullptr;
  g_title_label = nullptr;
  g_list_view = nullptr;
  g_list_container = nullptr;
  g_scan_status_label = nullptr;
  g_rescan_btn = nullptr;
  g_entry_view = nullptr;
  g_back_btn = nullptr;
  g_connect_btn = nullptr;
  g_ssid_ta = nullptr;
  g_pass_ta = nullptr;
  g_show_pass_cb = nullptr;
  g_keyboard = nullptr;
  g_result_count = 0;
  g_manual_mode = false;
  g_restart_pending = false;
}

void style_popup_button(lv_obj_t* btn, uint32_t color) {
  lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
  lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(btn, 14, 0);
}

lv_obj_t* create_popup_button(lv_obj_t* parent, const char* text, uint32_t color,
                              lv_event_cb_t cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_height(btn, 64);
  style_popup_button(btn, color);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &ui_font_20, 0);
  lv_obj_center(label);
  return btn;
}

int rssi_to_percent(int16_t rssi) {
  if (rssi >= -50) return 100;
  if (rssi <= -100) return 0;
  return 2 * (rssi + 100);
}

// ---- Ansicht wechseln ------------------------------------------------------

void show_list_view() {
  if (g_title_label) lv_label_set_text(g_title_label, tr().wifi_choose_btn);
  if (g_rescan_btn) lv_obj_clear_flag(g_rescan_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_back_btn) lv_obj_add_flag(g_back_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_connect_btn) lv_obj_add_flag(g_connect_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_list_view) lv_obj_clear_flag(g_list_view, LV_OBJ_FLAG_HIDDEN);
  if (g_entry_view) lv_obj_add_flag(g_entry_view, LV_OBJ_FLAG_HIDDEN);
}

void show_entry_view(bool manual, const char* ssid, bool open_network) {
  g_manual_mode = manual;
  g_selected_open = open_network;
  if (ssid) {
    strncpy(g_selected_ssid, ssid, sizeof(g_selected_ssid) - 1);
    g_selected_ssid[sizeof(g_selected_ssid) - 1] = '\0';
  } else {
    g_selected_ssid[0] = '\0';
  }

  if (g_title_label) {
    static char buf[80];
    if (manual) {
      lv_label_set_text(g_title_label, tr().wifi_manual_entry);
    } else if (open_network) {
      snprintf(buf, sizeof(buf), "%s (%s)", g_selected_ssid, tr().wifi_open_network);
      lv_label_set_text(g_title_label, buf);
    } else {
      snprintf(buf, sizeof(buf), tr().wifi_password_for_fmt, g_selected_ssid);
      lv_label_set_text(g_title_label, buf);
    }
  }
  if (g_rescan_btn) lv_obj_add_flag(g_rescan_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_back_btn) lv_obj_clear_flag(g_back_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_connect_btn) lv_obj_clear_flag(g_connect_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_ssid_ta) {
    lv_textarea_set_text(g_ssid_ta, "");
    if (manual) {
      lv_obj_clear_flag(g_ssid_ta, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_ssid_ta, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (g_pass_ta) {
    lv_textarea_set_text(g_pass_ta, "");
    lv_textarea_set_password_mode(g_pass_ta, true);
    // Offenes Netz: kein Passwortfeld, nur bestaetigen
    if (!manual && open_network) {
      lv_obj_add_flag(g_pass_ta, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(g_pass_ta, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (g_show_pass_cb) {
    lv_obj_remove_state(g_show_pass_cb, LV_STATE_CHECKED);
    if (!manual && open_network) {
      lv_obj_add_flag(g_show_pass_cb, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(g_show_pass_cb, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (g_keyboard) {
    if (!manual && open_network) {
      lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
      ui_keyboard_set_target(g_keyboard, manual ? g_ssid_ta : g_pass_ta,
                             manual ? g_pass_ta : g_ssid_ta);
    }
  }

  if (g_list_view) lv_obj_add_flag(g_list_view, LV_OBJ_FLAG_HIDDEN);
  if (g_entry_view) lv_obj_clear_flag(g_entry_view, LV_OBJ_FLAG_HIDDEN);
}

// ---- Scan ------------------------------------------------------------------

void show_scanning_state() {
  if (g_list_container) lv_obj_clean(g_list_container);
  if (g_scan_status_label) {
    lv_label_set_text(g_scan_status_label, tr().wifi_scan_searching);
    lv_obj_clear_flag(g_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  }
  if (g_rescan_btn) lv_obj_add_flag(g_rescan_btn, LV_OBJ_FLAG_HIDDEN);
}

void show_scan_finished_state() {
  if (g_rescan_btn) lv_obj_clear_flag(g_rescan_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_scan_status_label) {
    if (g_result_count == 0) {
      lv_label_set_text(g_scan_status_label, tr().wifi_scan_none);
      lv_obj_clear_flag(g_scan_status_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_scan_status_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void on_scan_timer(lv_timer_t*) {
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  stop_scan_timer();

  g_result_count = 0;
  for (int16_t i = 0; i < n && g_result_count < kMaxScanResults; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    const int16_t rssi = static_cast<int16_t>(WiFi.RSSI(i));
    const bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

    // Duplikate (Mesh/Repeater senden dieselbe SSID mehrfach): staerksten behalten
    bool duplicate = false;
    for (size_t k = 0; k < g_result_count; ++k) {
      if (strcmp(g_results[k].ssid, ssid.c_str()) == 0) {
        duplicate = true;
        if (rssi > g_results[k].rssi) {
          g_results[k].rssi = rssi;
          g_results[k].open = open;
        }
        break;
      }
    }
    if (duplicate) continue;

    ScanEntry& entry = g_results[g_result_count++];
    strncpy(entry.ssid, ssid.c_str(), sizeof(entry.ssid) - 1);
    entry.ssid[sizeof(entry.ssid) - 1] = '\0';
    entry.rssi = rssi;
    entry.open = open;
  }
  WiFi.scanDelete();

  // Nach Signalstaerke sortieren
  for (size_t i = 1; i < g_result_count; ++i) {
    ScanEntry key = g_results[i];
    size_t j = i;
    while (j > 0 && g_results[j - 1].rssi < key.rssi) {
      g_results[j] = g_results[j - 1];
      --j;
    }
    g_results[j] = key;
  }

  populate_list();
  show_scan_finished_state();
}

void start_scan() {
  stop_scan_timer();
  g_result_count = 0;
  show_scanning_state();
  WiFi.scanDelete();
  const int16_t r = WiFi.scanNetworks(/*async=*/true);
  if (r == WIFI_SCAN_FAILED) {
    show_scan_finished_state();
    return;
  }
  g_scan_timer = lv_timer_create(on_scan_timer, 500, nullptr);
}

// ---- Verbinden / Speichern -------------------------------------------------

void on_restart_timer(lv_timer_t* t) {
  lv_timer_del(t);
  displayManager.setInputEnabled(false);
  BoardHAL::prepareForRestart();
  delay(150);
  ESP.restart();
}

void do_connect() {
  if (g_restart_pending) return;

  const char* ssid = g_manual_mode && g_ssid_ta ? lv_textarea_get_text(g_ssid_ta)
                                                : g_selected_ssid;
  const char* pass = g_pass_ta ? lv_textarea_get_text(g_pass_ta) : "";
  if (!ssid || !ssid[0]) return;
  // Gesichertes Netz ohne Passwort: nicht speichern (wuerde nach dem
  // Neustart nur in einer toten Verbindung enden)
  if (!g_manual_mode && !g_selected_open && (!pass || !pass[0])) return;

  DeviceConfig cfg = configManager.getConfig();
  strncpy(cfg.wifi_ssid, ssid, CONFIG_WIFI_SSID_MAX - 1);
  cfg.wifi_ssid[CONFIG_WIFI_SSID_MAX - 1] = '\0';
  strncpy(cfg.wifi_pass, pass ? pass : "", CONFIG_WIFI_PASS_MAX - 1);
  cfg.wifi_pass[CONFIG_WIFI_PASS_MAX - 1] = '\0';
  // Wie das Web-Portal: Netzwechsel setzt auf DHCP zurueck, damit eine
  // alte statische IP das Geraet im neuen Netz nicht aussperrt.
  cfg.wifi_static_ip[0] = '\0';
  cfg.wifi_gateway[0] = '\0';
  cfg.wifi_subnet[0] = '\0';
  cfg.wifi_dns[0] = '\0';

  if (!configManager.save(cfg)) {
    if (g_title_label) lv_label_set_text(g_title_label, tr().wifi_save_failed);
    return;
  }

  Serial.printf("[WifiSetup] Neue WLAN-Konfiguration gespeichert (SSID '%s'), Neustart...\n",
                cfg.wifi_ssid);
  g_restart_pending = true;
  if (g_title_label) lv_label_set_text(g_title_label, tr().wifi_saved_restarting);
  if (g_keyboard) lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
  if (g_ssid_ta) lv_obj_add_flag(g_ssid_ta, LV_OBJ_FLAG_HIDDEN);
  if (g_pass_ta) lv_obj_add_flag(g_pass_ta, LV_OBJ_FLAG_HIDDEN);
  if (g_show_pass_cb) lv_obj_add_flag(g_show_pass_cb, LV_OBJ_FLAG_HIDDEN);
  if (g_back_btn) lv_obj_add_flag(g_back_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_connect_btn) lv_obj_add_flag(g_connect_btn, LV_OBJ_FLAG_HIDDEN);
  lv_timer_t* t = lv_timer_create(on_restart_timer, 1200, nullptr);
  lv_timer_set_repeat_count(t, 1);
}

// ---- Event-Handler ---------------------------------------------------------

void on_close_clicked(lv_event_t*) { close_popup(); }
void on_rescan_clicked(lv_event_t*) { start_scan(); }
void on_back_clicked(lv_event_t*) {
  if (g_restart_pending) return;
  show_list_view();
}
void on_connect_clicked(lv_event_t*) { do_connect(); }
void on_manual_clicked(lv_event_t*) { show_entry_view(true, nullptr, false); }

void on_network_clicked(lv_event_t* e) {
  const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (index >= g_result_count) return;
  show_entry_view(false, g_results[index].ssid, g_results[index].open);
}

void on_textarea_focused(lv_event_t* e) {
  lv_obj_t* ta = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!g_keyboard || !ta) return;
  ui_keyboard_set_target(g_keyboard, ta, ta == g_ssid_ta ? g_pass_ta : g_ssid_ta);
}

void on_keyboard_event(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    do_connect();
  } else if (code == LV_EVENT_CANCEL) {
    if (!g_restart_pending) show_list_view();
  }
}

void on_show_pass_toggled(lv_event_t*) {
  if (!g_pass_ta || !g_show_pass_cb) return;
  const bool show = lv_obj_has_state(g_show_pass_cb, LV_STATE_CHECKED);
  lv_textarea_set_password_mode(g_pass_ta, !show);
}

// ---- Aufbau ----------------------------------------------------------------

void populate_list() {
  if (!g_list_container) return;
  lv_obj_clean(g_list_container);

  static char right_buf[24];
  for (size_t i = 0; i < g_result_count; ++i) {
    lv_obj_t* row = lv_button_create(g_list_container);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, kRowHeight);
    style_popup_button(row, 0x3A3A3A);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(row, on_network_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

    lv_obj_t* name = lv_label_create(row);
    lv_label_set_text(name, g_results[i].ssid);
    lv_obj_set_style_text_font(name, &ui_font_20, 0);
    lv_obj_set_flex_grow(name, 1);
    lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);

    if (g_results[i].open) {
      snprintf(right_buf, sizeof(right_buf), "%d%%  (%s)",
               rssi_to_percent(g_results[i].rssi), tr().wifi_open_network);
    } else {
      snprintf(right_buf, sizeof(right_buf), "%d%%", rssi_to_percent(g_results[i].rssi));
    }
    lv_obj_t* right = lv_label_create(row);
    lv_label_set_text(right, right_buf);
    lv_obj_set_style_text_font(right, &ui_font_20, 0);
    lv_obj_set_style_text_color(right, lv_color_hex(0xA0A0A0), 0);
  }

  lv_obj_t* manual = lv_button_create(g_list_container);
  lv_obj_set_width(manual, LV_PCT(100));
  lv_obj_set_height(manual, kRowHeight);
  style_popup_button(manual, 0x2F4A63);
  lv_obj_set_style_radius(manual, 10, 0);
  lv_obj_add_event_cb(manual, on_manual_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* manual_label = lv_label_create(manual);
  lv_label_set_text(manual_label, tr().wifi_manual_entry);
  lv_obj_set_style_text_font(manual_label, &ui_font_20, 0);
  lv_obj_center(manual_label);
}

lv_obj_t* create_textarea(lv_obj_t* parent, const char* placeholder, bool password) {
  lv_obj_t* ta = lv_textarea_create(parent);
  lv_obj_set_width(ta, LV_PCT(100));
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_max_length(ta, password ? CONFIG_WIFI_PASS_MAX - 1
                                          : CONFIG_WIFI_SSID_MAX - 1);
  lv_obj_set_style_text_font(ta, &ui_font_20, 0);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x1E1E1E), 0);
  lv_obj_set_style_text_color(ta, lv_color_white(), 0);
  lv_obj_set_style_radius(ta, 10, 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_border_opa(ta, LV_OPA_COVER, 0);
  // Aktives Feld: blauer Rahmen + sichtbarer weisser Cursor. Ohne explizite
  // Fokus-Styles zeigt das dunkle Theme keinen erkennbaren Cursor.
  lv_obj_set_style_border_color(ta, lv_color_hex(0x378ADD), LV_STATE_FOCUSED);
  lv_obj_set_style_border_width(ta, 2, LV_STATE_FOCUSED);
  lv_obj_set_style_border_color(ta, lv_color_white(),
                                LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_width(ta, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_LEFT,
                               LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(ta, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_add_event_cb(ta, on_textarea_focused, LV_EVENT_FOCUSED, nullptr);
  return ta;
}

}  // namespace

void wifi_setup_popup_open() {
  if (g_overlay) return;  // laeuft bereits

  // Opak und als Kind des aktiven Screens (nicht lv_layer_top): LVGLs
  // Top-Objekt-Optimierung (lv_refr_get_top_obj) greift nur innerhalb des
  // Screens. Nur so ueberspringt jeder Popup-Redraw das Rendern+Blenden des
  // kompletten Tabs darunter — sonst sichtbar langsamer Aufbau (v.a. Tab5).
  g_overlay = lv_obj_create(lv_screen_active());
  lv_obj_set_size(g_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(g_overlay, 0, 0);
  lv_obj_set_style_bg_color(g_overlay, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(g_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(g_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(g_overlay, 0, 0);
  // Rand exakt wie die aeusseren Tiles zum Screenrand (grid_pad des Geraets):
  // die Karte fuellt fast den ganzen Screen, ihre runden Ecken bleiben aber
  // gegen den Overlay-Hintergrund sichtbar.
  lv_obj_set_style_pad_all(g_overlay, Device::kGridPad, 0);
  lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_CLICKABLE);  // Klicks nicht durchlassen
  lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_FLOATING);   // von Layout/Scroll ausnehmen

  g_card = lv_obj_create(g_overlay);
  lv_obj_set_size(g_card, LV_PCT(100), LV_PCT(100));
  lv_obj_center(g_card);
  lv_obj_set_style_bg_color(g_card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(g_card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(g_card, 22, 0);
  // Kinder (v.a. die eckigen Tasten) sauber an der Rundung abschneiden
  lv_obj_set_style_clip_corner(g_card, true, 0);
  lv_obj_set_style_pad_all(g_card, 10, 0);
  lv_obj_clear_flag(g_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(g_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(g_card, 8, 0);

  // Kopfzeile: Titel + Neu-suchen + X - hoeher, aehnlich dem Kopf der
  // anderen Popups (dort kHeaderHeight = 96).
  lv_obj_t* header = lv_obj_create(g_card);
  lv_obj_set_width(header, LV_PCT(100));
  lv_obj_set_height(header, 88);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(header, 10, 0);

  g_title_label = lv_label_create(header);
  lv_label_set_text(g_title_label, tr().wifi_choose_btn);
  lv_label_set_long_mode(g_title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(g_title_label, &ui_font_28, 0);
  lv_obj_set_style_text_color(g_title_label, lv_color_white(), 0);
  lv_obj_set_flex_grow(g_title_label, 1);

  g_rescan_btn = create_popup_button(header, tr().wifi_scan_retry, 0x1976D2,
                                     on_rescan_clicked);
  // Zurueck/Verbinden leben in der Kopfzeile (nur im Eingabe-View sichtbar),
  // damit unten die volle Hoehe fuer die Tastatur bleibt.
  g_back_btn = create_popup_button(header, tr().wifi_back_btn, 0x555555,
                                   on_back_clicked);
  lv_obj_add_flag(g_back_btn, LV_OBJ_FLAG_HIDDEN);
  g_connect_btn = create_popup_button(header, tr().wifi_connect_btn, 0x2E7D32,
                                      on_connect_clicked);
  lv_obj_add_flag(g_connect_btn, LV_OBJ_FLAG_HIDDEN);

  // X-Button im Stil der uebrigen Popups: transparent, MDI-Icon, Press-Feedback
  lv_obj_t* close_btn = lv_button_create(header);
  lv_obj_set_size(close_btn, 64, 64);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(close_btn, 14, 0);
  lv_obj_set_style_pad_all(close_btn, 0, 0);
  lv_obj_set_ext_click_area(close_btn, 8);
  lv_obj_add_event_cb(close_btn, on_close_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* close_label = lv_label_create(close_btn);
  lv_obj_set_style_text_font(close_label, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);

  // Ansicht 1: Scan-Status + Liste
  g_list_view = lv_obj_create(g_card);
  lv_obj_set_width(g_list_view, LV_PCT(100));
  lv_obj_set_flex_grow(g_list_view, 1);
  lv_obj_set_style_bg_opa(g_list_view, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(g_list_view, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(g_list_view, 0, 0);
  lv_obj_clear_flag(g_list_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(g_list_view, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(g_list_view, 8, 0);

  g_scan_status_label = lv_label_create(g_list_view);
  lv_label_set_text(g_scan_status_label, tr().wifi_scan_searching);
  lv_obj_set_style_text_font(g_scan_status_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(g_scan_status_label, lv_color_hex(0xC8C8C8), 0);

  g_list_container = lv_obj_create(g_list_view);
  lv_obj_set_width(g_list_container, LV_PCT(100));
  lv_obj_set_flex_grow(g_list_container, 1);
  lv_obj_set_style_bg_opa(g_list_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(g_list_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(g_list_container, 0, 0);
  lv_obj_set_flex_flow(g_list_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(g_list_container, 8, 0);

  // Ansicht 2: Eingabe (SSID optional, Passwort) + Tastatur
  g_entry_view = lv_obj_create(g_card);
  lv_obj_set_width(g_entry_view, LV_PCT(100));
  lv_obj_set_flex_grow(g_entry_view, 1);
  lv_obj_set_style_bg_opa(g_entry_view, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(g_entry_view, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(g_entry_view, 0, 0);
  lv_obj_clear_flag(g_entry_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(g_entry_view, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(g_entry_view, 8, 0);
  lv_obj_add_flag(g_entry_view, LV_OBJ_FLAG_HIDDEN);

  g_ssid_ta = create_textarea(g_entry_view, tr().ssid_label, false);
  g_pass_ta = create_textarea(g_entry_view, tr().wifi_password_label, true);

  g_show_pass_cb = lv_checkbox_create(g_entry_view);
  lv_checkbox_set_text(g_show_pass_cb, tr().wifi_show_password);
  lv_obj_set_style_text_font(g_show_pass_cb, &ui_font_20, 0);
  lv_obj_set_style_text_color(g_show_pass_cb, lv_color_hex(0xC8C8C8), 0);
  lv_obj_add_event_cb(g_show_pass_cb, on_show_pass_toggled, LV_EVENT_VALUE_CHANGED,
                      nullptr);

  // Unsichtbarer Spacer schluckt den Rest der Hoehe (z.B. wenn das SSID-Feld
  // ausgeblendet ist), damit die Tastatur trotzdem unten verankert bleibt
  // statt in der Mitte zu schweben.
  lv_obj_t* keyboard_spacer = lv_obj_create(g_entry_view);
  lv_obj_set_width(keyboard_spacer, LV_PCT(100));
  lv_obj_set_flex_grow(keyboard_spacer, 1);
  lv_obj_set_style_bg_opa(keyboard_spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(keyboard_spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(keyboard_spacer, 0, 0);
  lv_obj_clear_flag(keyboard_spacer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(keyboard_spacer, LV_OBJ_FLAG_CLICKABLE);

  g_keyboard = ui_keyboard_create(g_entry_view);
  lv_obj_set_width(g_keyboard, LV_PCT(100));
  // Feste (kleinere) Hoehe statt flex_grow: die Tastatur soll immer gleich
  // gross sein, egal ob (manuell) SSID+Passwort oder (Auswahl) nur Passwort
  // sichtbar ist - und damit auch Platz fuer weitere Felder auf kuenftigen
  // Eingabe-Seiten (MQTT, statische IP, ...) frei bleibt. Der Spacer oben
  // verankert sie unten buendig mit demselben Abstand wie links/rechts
  // (card-pad), statt dass sie mittig schwebt.
  lv_obj_set_height(g_keyboard, LV_PCT(46));
  lv_keyboard_set_textarea(g_keyboard, g_pass_ta);
  lv_obj_add_event_cb(g_keyboard, on_keyboard_event, LV_EVENT_ALL, nullptr);

  show_list_view();
  start_scan();
}
