#ifndef TAB_SETTINGS_H
#define TAB_SETTINGS_H

#include <lvgl.h>

// Callback-Typ für Hotspot-Button
typedef void (*hotspot_callback_t)(bool enable);

void build_settings_tab(lv_obj_t *tab, hotspot_callback_t hotspot_cb = nullptr);

// Vom Sketch registriert: stößt im Hauptloop einen WLAN-Reconnect mit den
// aktuell gespeicherten Zugangsdaten an (statt das Gerät neu zu starten).
typedef void (*wifi_reconnect_callback_t)();
void settings_set_wifi_reconnect_callback(wifi_reconnect_callback_t cb);

// Update WiFi-Status (aufgerufen von main loop)
void settings_update_wifi_status(bool connected, const char* ssid, const char* ip);
void settings_update_wifi_status_ap(const char* ssid, const char* password);
void settings_update_ap_mode(bool running);
void settings_refresh_language();
void settings_sync_display_rotation(bool rotated);

// Update Power-Status (aufgerufen von main loop)
void settings_update_power_status();

#endif // TAB_SETTINGS_H
