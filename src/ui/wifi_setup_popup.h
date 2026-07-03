#ifndef WIFI_SETUP_POPUP_H
#define WIFI_SETUP_POPUP_H

// Vollbild-Overlay zum Auswaehlen eines WLANs direkt am Geraet:
// asynchroner Scan -> Netzliste -> Bildschirmtastatur fuers Passwort ->
// speichert wifi_ssid/wifi_pass (und setzt wie das Web-Portal die
// statische IP auf DHCP zurueck) -> Neustart.
void wifi_setup_popup_open();

#endif  // WIFI_SETUP_POPUP_H
