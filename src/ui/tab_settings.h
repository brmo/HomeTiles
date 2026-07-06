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

// Update-ueber-GitHub (System-Popup): Klicks setzen nur Callbacks ab; der
// Sketch fuehrt Check/Install auf dem Loop-Task aus (Pending-Flag-Muster,
// nie TLS/Netzwerk direkt im LVGL-Event-Callback).
typedef void (*fw_check_callback_t)();
typedef void (*fw_install_callback_t)(const char* tag);
void settings_set_fw_check_callback(fw_check_callback_t cb);
void settings_set_fw_install_callback(fw_install_callback_t cb);
typedef void (*system_reboot_callback_t)();
void settings_set_system_reboot_callback(system_reboot_callback_t cb);
// Rueckmeldungen vom Loop-Task; tolerieren ein inzwischen geschlossenes Popup.
void settings_fw_check_result(bool ok, const char* latest_tag, bool update_available);
void settings_fw_install_progress(size_t written, size_t total);
void settings_fw_install_done();
void settings_fw_install_failed(const char* error);

// Update WiFi-Status (aufgerufen von main loop)
void settings_update_wifi_status(bool connected, const char* ssid, const char* ip);
void settings_update_wifi_status_ap(const char* ssid, const char* password);
void settings_update_ap_mode(bool running);
void settings_refresh_language();
void settings_sync_display_rotation(bool rotated);

// Update Power-Status (aufgerufen von main loop)
void settings_update_power_status();

#endif // TAB_SETTINGS_H
