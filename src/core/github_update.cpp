#include "src/core/github_update.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "src/core/firmware_version.h"
#include "src/devices/device.h"

namespace {

// "v0.3.1" / "0.3.1" -> {0,3,1}; fehlende Teile bleiben 0.
void parseVersion(const char* s, int out[3]) {
  out[0] = out[1] = out[2] = 0;
  if (!s) return;
  while (*s && !isdigit(static_cast<unsigned char>(*s))) ++s;
  for (int i = 0; i < 3 && *s; ++i) {
    out[i] = atoi(s);
    while (*s && isdigit(static_cast<unsigned char>(*s))) ++s;
    if (*s != '.') break;
    ++s;
  }
}

bool isNewerThanCurrent(const char* tag) {
  int latest[3];
  int current[3];
  parseVersion(tag, latest);
  parseVersion(FW_VERSION, current);
  for (int i = 0; i < 3; ++i) {
    if (latest[i] != current[i]) return latest[i] > current[i];
  }
  return false;
}

}  // namespace

namespace GithubUpdate {

CheckResult checkLatest() {
  CheckResult result;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Update] Check uebersprungen: kein WLAN");
    return result;
  }

  // GitHub- und CDN-Zertifikate rotieren regelmaessig; eine eingebrannte
  // CA-Liste waere beim ersten Wechsel tot. Fuer Firmware von der eigenen
  // Release-Seite ist der Verzicht auf die Pruefung der uebliche Kompromiss
  // (Update.end validiert das Image selbst via Magic-Byte + Groesse).
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  const String url = String(kRepoUrl) + "/releases/latest";
  if (!http.begin(client, url)) return result;
  // Redirect NICHT folgen: der Location-Header traegt bereits den Tag.
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const char* kCollect[] = {"Location"};
  http.collectHeaders(kCollect, 1);

  const int code = http.GET();
  const String location = http.header("Location");
  http.end();

  if (code < 300 || code >= 400 || !location.length()) {
    Serial.printf("[Update] Check fehlgeschlagen: HTTP %d\n", code);
    return result;
  }

  const int tag_pos = location.indexOf("/releases/tag/");
  if (tag_pos < 0) {
    // Repo ohne Releases leitet auf /releases um
    Serial.printf("[Update] Kein Release gefunden (%s)\n", location.c_str());
    return result;
  }
  const String tag = location.substring(tag_pos + strlen("/releases/tag/"));
  if (!tag.length() || tag.length() >= sizeof(result.latest_tag)) {
    Serial.printf("[Update] Unerwarteter Tag: '%s'\n", tag.c_str());
    return result;
  }

  snprintf(result.latest_tag, sizeof(result.latest_tag), "%s", tag.c_str());
  result.ok = true;
  result.update_available = isNewerThanCurrent(result.latest_tag);
  Serial.printf("[Update] Installiert %s, neuestes Release %s -> %s\n",
                FW_VERSION, result.latest_tag,
                result.update_available ? "Update verfuegbar" : "aktuell");
  return result;
}

bool install(const char* tag, ProgressFn progress, String& error_out) {
  error_out = "";
  if (!tag || !*tag) {
    error_out = "no tag";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    error_out = "no wifi";
    return false;
  }
  // Evtl. haengengebliebenen Web-OTA-Rest aufraeumen
  if (Update.isRunning()) Update.abort();

  const String url = String(kRepoUrl) + "/releases/download/" + tag +
                     "/esp32-p4-homeassistant-display-" + tag + "-" +
                     Device::profile().key + ".bin";
  Serial.printf("[Update] Lade %s\n", url.c_str());

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    error_out = "begin failed";
    return false;
  }
  // GitHub leitet Release-Downloads auf objects.githubusercontent.com um
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    error_out = String("HTTP ") + code;
    Serial.printf("[Update] Download fehlgeschlagen: %s (Asset-Name/Tag pruefen?)\n",
                  error_out.c_str());
    return false;
  }

  const int total = http.getSize();  // -1 = unbekannt (chunked)
  const size_t total_sz = (total > 0) ? static_cast<size_t>(total) : 0;
  if (!Update.begin(total_sz ? total_sz : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    error_out = Update.errorString();
    http.end();
    Serial.printf("[Update] Update.begin fehlgeschlagen: %s\n", error_out.c_str());
    return false;
  }
  Serial.printf("[Update] Update.begin OK, Groesse: %d\n", total);

  // Lesepuffer im PSRAM, um das knappe interne RAM (TLS!) nicht zusaetzlich
  // zu belasten; 8KB-Happen geben dem Progress-Callback einen steten Takt.
  constexpr size_t kChunk = 8192;
  uint8_t* buf = static_cast<uint8_t*>(
      heap_caps_malloc(kChunk, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buf) buf = static_cast<uint8_t*>(malloc(kChunk));
  if (!buf) {
    Update.abort();
    http.end();
    error_out = "alloc failed";
    return false;
  }

  auto* stream = http.getStreamPtr();
  size_t written_total = 0;
  uint32_t last_data_ms = millis();
  bool failed = false;

  while (http.connected() && (total < 0 || written_total < total_sz)) {
    const size_t avail = stream->available();
    if (avail) {
      const int r = stream->readBytes(buf, (avail > kChunk) ? kChunk : avail);
      if (r <= 0) continue;
      if (Update.write(buf, r) != static_cast<size_t>(r)) {
        error_out = Update.errorString();
        failed = true;
        break;
      }
      written_total += r;
      last_data_ms = millis();
      if (progress) progress(written_total, total_sz);
    } else {
      if (millis() - last_data_ms > 20000UL) {
        error_out = "timeout";
        failed = true;
        break;
      }
      if (progress) progress(written_total, total_sz);
      delay(10);
    }
  }
  free(buf);

  if (!failed && total_sz && written_total < total_sz) {
    error_out = "connection lost";
    failed = true;
  }

  if (failed) {
    Update.abort();
    http.end();
    Serial.printf("[Update] Install fehlgeschlagen: %s\n", error_out.c_str());
    return false;
  }
  http.end();

  if (!Update.end(true)) {
    error_out = Update.errorString();
    Serial.printf("[Update] Update.end fehlgeschlagen: %s\n", error_out.c_str());
    return false;
  }
  Serial.printf("[Update] %u Bytes installiert - bereit zum Neustart\n",
                static_cast<unsigned>(written_total));
  return true;
}

}  // namespace GithubUpdate
