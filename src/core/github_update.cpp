#include "src/core/github_update.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <lwip/sockets.h>
#include <stdlib.h>
#include <string.h>

#include "src/core/firmware_metadata.h"
#include "src/core/firmware_version.h"
#include "src/devices/device.h"

namespace {

constexpr size_t kInstallReadChunk = 2048;
constexpr size_t kInstallStageBytes = 512 * 1024;
constexpr size_t kInstallWriteSliceBytes = 16 * 1024;
constexpr size_t kSocketRxBufferBytes = 4 * 1024;
constexpr size_t kMaxHttpLineLen = 4096;
constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kReadTimeoutMs = 20000;
constexpr uint32_t kReadPaceMs = 0;

struct ParsedHttpsUrl {
  String host;
  String path;
  uint16_t port = 443;
};

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

bool parseHttpsUrl(const String& url, ParsedHttpsUrl& out) {
  constexpr const char* kPrefix = "https://";
  if (!url.startsWith(kPrefix)) return false;

  const int host_start = strlen(kPrefix);
  int path_start = url.indexOf('/', host_start);
  if (path_start < 0) path_start = url.length();

  String host_port = url.substring(host_start, path_start);
  if (!host_port.length()) return false;

  out.port = 443;
  const int colon = host_port.lastIndexOf(':');
  if (colon > 0) {
    out.host = host_port.substring(0, colon);
    const int parsed_port = atoi(host_port.substring(colon + 1).c_str());
    if (parsed_port <= 0 || parsed_port > 65535) return false;
    out.port = static_cast<uint16_t>(parsed_port);
  } else {
    out.host = host_port;
  }

  out.path = (path_start < url.length()) ? url.substring(path_start) : "/";
  return out.host.length() && out.path.length();
}

bool readHttpLine(WiFiClientSecure& client, String& line, uint32_t timeout_ms) {
  line = "";
  const uint32_t start_ms = millis();
  while (millis() - start_ms < timeout_ms) {
    while (client.available() > 0) {
      const char c = static_cast<char>(client.read());
      if (c == '\r') continue;
      if (c == '\n') return true;
      if (line.length() < kMaxHttpLineLen) line += c;
    }
    if (!client.connected() && !client.available()) return line.length() > 0;
    delay(1);
  }
  return false;
}

bool parseStatusCode(const String& status_line, int& code) {
  const int first_space = status_line.indexOf(' ');
  if (first_space < 0 || first_space + 4 > status_line.length()) return false;
  code = atoi(status_line.substring(first_space + 1, first_space + 4).c_str());
  return code > 0;
}

size_t parseSizeT(const String& value) {
  return static_cast<size_t>(strtoul(value.c_str(), nullptr, 10));
}

bool parseContentRangeTotal(const String& value, size_t& total) {
  const int slash = value.lastIndexOf('/');
  if (slash < 0 || slash + 1 >= value.length()) return false;
  const String total_part = value.substring(slash + 1);
  if (total_part == "*") return false;
  total = parseSizeT(total_part);
  return total > 0;
}

String resolveRedirectUrl(const String& current_url,
                          const ParsedHttpsUrl& current,
                          const String& location) {
  if (location.startsWith("https://")) return location;
  if (location.startsWith("//")) return String("https:") + location;
  if (location.startsWith("/")) {
    return String("https://") + current.host + location;
  }

  const int slash = current_url.lastIndexOf('/');
  if (slash > strlen("https://")) {
    return current_url.substring(0, slash + 1) + location;
  }
  return String("https://") + current.host + "/" + location;
}

String releaseDownloadUrl(const char* tag, const String& asset_name) {
  return String(GithubUpdate::kRepoUrl) + "/releases/download/" + tag + "/" + asset_name;
}

String legacyDeviceSlug() {
  String slug = Device::profile().key;
  slug.replace('_', '-');
  return slug;
}

void logCheckNetworkState(const char* label, const String& url) {
  const uint32_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const uint32_t dma_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t dma_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

  Serial.printf("[Update/Diag] %s url=%s\n",
                label ? label : "?",
                url.c_str());
  Serial.printf("[Update/Diag] wifi=%d ip=%s gw=%s dns=%s rssi=%ld\n",
                static_cast<int>(WiFi.status()),
                WiFi.localIP().toString().c_str(),
                WiFi.gatewayIP().toString().c_str(),
                WiFi.dnsIP(0).toString().c_str(),
                static_cast<long>(WiFi.RSSI()));
  Serial.printf("[Update/Diag] mem int=%u KB largest=%u KB dma=%u KB dma_largest=%u KB psram=%u KB\n",
                static_cast<unsigned>(int_free / 1024),
                static_cast<unsigned>(int_largest / 1024),
                static_cast<unsigned>(dma_free / 1024),
                static_cast<unsigned>(dma_largest / 1024),
                static_cast<unsigned>(ESP.getFreePsram() / 1024));
}

void logCheckFailureDetails(int code, WiFiClientSecure& client, const String& url) {
  char tls_error[96] = {};
  const int tls_code = client.lastError(tls_error, sizeof(tls_error));

  IPAddress github_ip;
  const int dns_ok = WiFi.hostByName("github.com", github_ip) ? 1 : 0;

  if (code < 0) {
    Serial.printf("[Update/Diag] http=%d (%s) tls=%d (%s) fd=%d\n",
                  code,
                  HTTPClient::errorToString(code).c_str(),
                  tls_code,
                  tls_error,
                  client.fd());
  } else {
    Serial.printf("[Update/Diag] http=%d tls=%d (%s) fd=%d\n",
                  code,
                  tls_code,
                  tls_error,
                  client.fd());
  }
  Serial.printf("[Update/Diag] dns github.com: ok=%d ip=%s | url=%s\n",
                dns_ok,
                github_ip.toString().c_str(),
                url.c_str());
}

typedef bool (*RangeDataFn)(const uint8_t* data, size_t len, void* ctx);

bool fetchHttpRange(const String& start_url, size_t from, size_t to,
                    uint8_t* buf, size_t buf_len, RangeDataFn on_data,
                    void* ctx, size_t& content_total, String& error_out) {
  if (!buf || !buf_len || !on_data || to < from) {
    error_out = "invalid range request";
    return false;
  }

  String url = start_url;
  for (int redirect = 0; redirect < 5; ++redirect) {
    ParsedHttpsUrl parsed;
    if (!parseHttpsUrl(url, parsed)) {
      error_out = "bad https url";
      return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);
    if (!client.connect(parsed.host.c_str(), parsed.port, kConnectTimeoutMs)) {
      error_out = String("connect failed: ") + parsed.host;
      return false;
    }

    int rcvbuf = static_cast<int>(kSocketRxBufferBytes);
    if (client.fd() >= 0) {
      client.setSocketOption(SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    }

    client.printf("GET %s HTTP/1.1\r\n", parsed.path.c_str());
    client.printf("Host: %s\r\n", parsed.host.c_str());
    client.print("User-Agent: hometiles\r\n");
    client.print("Accept: application/octet-stream\r\n");
    client.print("Accept-Encoding: identity\r\n");
    client.printf("Range: bytes=%u-%u\r\n",
                  static_cast<unsigned>(from), static_cast<unsigned>(to));
    client.print("Connection: close\r\n\r\n");

    String line;
    if (!readHttpLine(client, line, kReadTimeoutMs)) {
      client.stop();
      error_out = "no http status";
      return false;
    }

    int status_code = 0;
    if (!parseStatusCode(line, status_code)) {
      client.stop();
      error_out = "bad http status";
      return false;
    }

    String location;
    size_t content_length = 0;
    bool has_content_length = false;
    size_t range_total = 0;

    bool header_complete = false;
    while (readHttpLine(client, line, kReadTimeoutMs)) {
      if (!line.length()) {
        header_complete = true;
        break;
      }
      const int colon = line.indexOf(':');
      if (colon <= 0) continue;
      String name = line.substring(0, colon);
      name.toLowerCase();
      String value = line.substring(colon + 1);
      value.trim();
      if (name == "location") {
        location = value;
      } else if (name == "content-length") {
        content_length = parseSizeT(value);
        has_content_length = content_length > 0;
      } else if (name == "content-range") {
        parseContentRangeTotal(value, range_total);
      }
    }
    if (!header_complete) {
      client.stop();
      error_out = "header timeout";
      return false;
    }

    if (status_code >= 300 && status_code < 400 && location.length()) {
      client.stop();
      url = resolveRedirectUrl(url, parsed, location);
      continue;
    }

    if (status_code != 206) {
      client.stop();
      error_out = String("HTTP ") + status_code;
      return false;
    }
    if (!range_total) {
      client.stop();
      error_out = "missing content-range";
      return false;
    }
    content_total = range_total;

    const size_t expected = to - from + 1;
    if (has_content_length && content_length != expected) {
      client.stop();
      error_out = "range length mismatch";
      return false;
    }

    size_t received = 0;
    uint32_t last_data_ms = millis();
    while (received < expected) {
      const int avail = client.available();
      if (avail > 0) {
        size_t want = expected - received;
        if (want > buf_len) want = buf_len;
        if (want > static_cast<size_t>(avail)) want = static_cast<size_t>(avail);

        const int r = client.read(buf, want);
        if (r <= 0) {
          delay(1);
          continue;
        }
        if (!on_data(buf, static_cast<size_t>(r), ctx)) {
          client.stop();
          if (!error_out.length()) error_out = "write failed";
          return false;
        }
        received += static_cast<size_t>(r);
        last_data_ms = millis();
        if (kReadPaceMs > 0) {
          delay(kReadPaceMs);
        } else {
          yield();
        }
      } else {
        if (!client.connected() || millis() - last_data_ms > kReadTimeoutMs) {
          client.stop();
          error_out = client.connected() ? "timeout" : "connection lost";
          return false;
        }
        delay(10);
      }
    }

    client.stop();
    return true;
  }

  error_out = "too many redirects";
  return false;
}

struct HeadBufferCtx {
  uint8_t* data = nullptr;
  size_t capacity = 0;
  size_t len = 0;
};

bool storeHeadBytes(const uint8_t* data, size_t len, void* raw_ctx) {
  auto* ctx = static_cast<HeadBufferCtx*>(raw_ctx);
  if (!ctx || !ctx->data || ctx->len + len > ctx->capacity) return false;
  memcpy(ctx->data + ctx->len, data, len);
  ctx->len += len;
  return true;
}

bool storeBufferBytes(const uint8_t* data, size_t len, void* raw_ctx) {
  auto* ctx = static_cast<HeadBufferCtx*>(raw_ctx);
  if (!ctx || !ctx->data || ctx->len + len > ctx->capacity) return false;
  memcpy(ctx->data + ctx->len, data, len);
  ctx->len += len;
  return true;
}

struct UpdateWriteCtx {
  size_t written = 0;
  size_t total = 0;
  size_t next_progress_log = 512 * 1024;
  uint32_t last_progress_ms = 0;
  GithubUpdate::ProgressFn progress = nullptr;
  String* error = nullptr;
};

void reportInstallProgress(UpdateWriteCtx& ctx, bool force) {
  if (!ctx.progress) return;
  const uint32_t now_ms = millis();
  if (force || now_ms - ctx.last_progress_ms >= 500 ||
      (ctx.total && ctx.written >= ctx.total)) {
    ctx.last_progress_ms = now_ms;
    ctx.progress(ctx.written, ctx.total);
  }
}

bool writeUpdateBytes(const uint8_t* data, size_t len, void* raw_ctx) {
  auto* ctx = static_cast<UpdateWriteCtx*>(raw_ctx);
  if (!ctx || !data) return false;
  // In Scheiben schreiben statt der ganzen Stage am Stueck: waehrend
  // Update.write blockiert der Loop-Task sekundenlang im Flash-Erase/Write
  // und der esp-hosted-SDIO-Treiber kann eingehende WLAN-Frames nicht mehr
  // ablegen -> assert pkt_rxbuff (sdio_drv.c:928) mitten im Install. Die
  // Pausen zwischen den Scheiben lassen ihn die RX-Queue leeren; mit den
  // alten 32KB-Stages war das Fenster kurz genug, mit 512KB nicht mehr.
  size_t offset = 0;
  while (offset < len) {
    size_t slice = len - offset;
    if (slice > kInstallWriteSliceBytes) slice = kInstallWriteSliceBytes;
    const size_t bytes_written =
        Update.write(const_cast<uint8_t*>(data + offset), slice);
    if (bytes_written != slice) {
      if (ctx->error) *ctx->error = Update.errorString();
      return false;
    }
    ctx->written += bytes_written;
    offset += slice;
    if (offset < len) delay(2);
  }
  if (ctx->written >= ctx->next_progress_log ||
      (ctx->total && ctx->written >= ctx->total)) {
    Serial.printf("[Update] Install progress: %u / %u bytes\n",
                  static_cast<unsigned>(ctx->written),
                  static_cast<unsigned>(ctx->total));
    ctx->next_progress_log += 512 * 1024;
  }
  reportInstallProgress(*ctx, false);
  return true;
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

  // Redirects NICHT pauschal folgen: der Location-Header traegt den Tag.
  // Nach einem Repo-Rename liefert GitHub aber ERST eine Umleitung auf den
  // neuen Repo-Pfad (.../releases/latest) - dieser Kette manuell folgen,
  // bis eine Tag-URL auftaucht, sonst verlieren ausgelieferte Geraete beim
  // Umbenennen des Repos dauerhaft ihre Update-Suche.
  String url = String(kRepoUrl) + "/releases/latest";
  String tag;
  logCheckNetworkState("check-start", url);
  for (int hop = 0; hop < 4; ++hop) {
    client.stop();
    client.setTimeout(8000);
    client.setHandshakeTimeout(12);

    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    http.setReuse(false);
    if (!http.begin(client, url)) {
      Serial.printf("[Update] Check fehlgeschlagen: HTTP begin (%s)\n", url.c_str());
      logCheckFailureDetails(0, client, url);
      client.stop();
      return result;
    }
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Connection", "close");
    const char* kCollect[] = {"Location"};
    http.collectHeaders(kCollect, 1);

    const int code = http.GET();
    const String location = http.header("Location");
    http.end();
    client.stop();
    delay(10);

    if (code < 300 || code >= 400 || !location.length()) {
      if (code < 0) {
        Serial.printf("[Update] Check fehlgeschlagen: HTTP %d (%s)\n",
                      code,
                      HTTPClient::errorToString(code).c_str());
      } else {
        Serial.printf("[Update] Check fehlgeschlagen: HTTP %d\n", code);
      }
      logCheckFailureDetails(code, client, url);
      return result;
    }

    const int tag_pos = location.indexOf("/releases/tag/");
    if (tag_pos >= 0) {
      tag = location.substring(tag_pos + strlen("/releases/tag/"));
      break;
    }
    if (location.indexOf("/releases/latest") < 0) {
      // Weder Tag- noch latest-Pfad: Repo ohne Releases (leitet auf
      // /releases um) oder etwas Unerwartetes.
      Serial.printf("[Update] Kein Release gefunden (%s)\n", location.c_str());
      return result;
    }
    url = location;  // Rename-Redirect: neue Repo-URL erneut anfragen
  }

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

  String url = releaseDownloadUrl(
      tag, String("hometiles_") + tag + "_" + Device::profile().key + ".bin");
  Serial.printf("[Update] Lade %s\n", url.c_str());

  // Netzwerk-Lesepuffer und OTA-Staging liegen im PSRAM. Der GitHub/CDN-
  // Download wird absichtlich in Range-Requests zerlegt und erst in PSRAM
  // gepuffert. Update.write() laeuft danach ohne offene HTTPS-Verbindung, damit
  // Flash-Writes SDIO-WLAN nicht mit RX-Puffern blockieren koennen.
  uint8_t* net_buf = static_cast<uint8_t*>(
      heap_caps_malloc(kInstallReadChunk, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!net_buf) net_buf = static_cast<uint8_t*>(malloc(kInstallReadChunk));
  uint8_t* stage_buf = static_cast<uint8_t*>(
      heap_caps_malloc(kInstallStageBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!stage_buf) stage_buf = static_cast<uint8_t*>(malloc(kInstallStageBytes));
  if (!net_buf || !stage_buf) {
    if (net_buf) free(net_buf);
    if (stage_buf) free(stage_buf);
    error_out = "alloc failed";
    return false;
  }
  Serial.printf("[Update] Staged Range-Download: %uB TCP RX, %uB Lesechunk, %uKB Stage\n",
                static_cast<unsigned>(kSocketRxBufferBytes),
                static_cast<unsigned>(kInstallReadChunk),
                static_cast<unsigned>(kInstallStageBytes / 1024));

  bool failed = false;
  bool update_started = false;
  size_t total_sz = 0;

  uint8_t image_head[firmware_meta::kDeviceDescriptorImageBytes] = {0};
  HeadBufferCtx head_ctx{image_head, sizeof(image_head), 0};
  if (!fetchHttpRange(url, 0, sizeof(image_head) - 1, net_buf, kInstallReadChunk,
                      storeHeadBytes, &head_ctx, total_sz, error_out)) {
    const String first_error = error_out;
    const String legacy_url = releaseDownloadUrl(
        tag,
        String("esp32-p4-homeassistant-display-") + tag + "-" +
            legacyDeviceSlug() + "-update.bin");
    memset(image_head, 0, sizeof(image_head));
    head_ctx.len = 0;
    total_sz = 0;
    error_out = "";
    Serial.printf("[Update] Asset nicht gefunden/lesbar (%s), versuche %s\n",
                  first_error.c_str(), legacy_url.c_str());
    if (!fetchHttpRange(legacy_url, 0, sizeof(image_head) - 1, net_buf,
                        kInstallReadChunk, storeHeadBytes, &head_ctx, total_sz,
                        error_out)) {
      error_out = first_error + "; fallback: " + error_out;
      failed = true;
    } else {
      url = legacy_url;
    }
  }
  if (!failed) {
    delay(20);
  }

  if (!failed) {
    if (total_sz < firmware_meta::kDeviceDescriptorImageBytes) {
      error_out = "image too small";
      failed = true;
    }
  }

  if (!failed) {
    firmware_meta::DeviceDescriptor incoming_desc{};
    if (!firmware_meta::parseDeviceDescriptorFromImage(
            image_head, head_ctx.len, incoming_desc)) {
      error_out = "firmware metadata missing or invalid";
      failed = true;
    } else if (!firmware_meta::matchesCurrentDeviceKey(incoming_desc.device_key)) {
      error_out = String("device mismatch: got ") +
                  incoming_desc.display_name + ", expected " +
                  firmware_meta::expectedDeviceDisplayName();
      failed = true;
    } else if (strcmp(incoming_desc.project_key,
                      firmware_meta::currentProjectKey()) != 0) {
      error_out = String("project mismatch: got ") +
                  incoming_desc.project_key + ", expected " +
                  firmware_meta::currentProjectKey();
      failed = true;
    }
  }

  if (!failed) {
    if (!Update.begin(total_sz, U_FLASH)) {
      error_out = Update.errorString();
      failed = true;
      Serial.printf("[Update] Update.begin fehlgeschlagen: %s\n",
                    error_out.c_str());
    } else {
      update_started = true;
      Serial.printf("[Update] Update.begin OK, Groesse: %u\n",
                    static_cast<unsigned>(total_sz));
      UpdateWriteCtx write_ctx;
      write_ctx.written = 0;
      write_ctx.total = total_sz;
      write_ctx.progress = progress;
      write_ctx.error = &error_out;
      if (!writeUpdateBytes(image_head, head_ctx.len, &write_ctx)) {
        if (!error_out.length()) error_out = Update.errorString();
        failed = true;
      } else {
        reportInstallProgress(write_ctx, true);
      }

      while (!failed && write_ctx.written < total_sz) {
        const size_t range_start = write_ctx.written;
        size_t range_end = range_start + kInstallStageBytes - 1;
        if (range_end >= total_sz || range_end < range_start) {
          range_end = total_sz - 1;
        }
        const size_t expected_len = range_end - range_start + 1;

        size_t range_total = 0;
        HeadBufferCtx stage_ctx{stage_buf, kInstallStageBytes, 0};
        if (!fetchHttpRange(url, range_start, range_end, net_buf, kInstallReadChunk,
                            storeBufferBytes, &stage_ctx, range_total,
                            error_out)) {
          failed = true;
          break;
        }
        if (range_total != total_sz) {
          error_out = "image size changed";
          failed = true;
          break;
        }
        if (stage_ctx.len != expected_len) {
          error_out = "staged range incomplete";
          failed = true;
          break;
        }
        delay(20);
        if (!writeUpdateBytes(stage_buf, stage_ctx.len, &write_ctx)) {
          if (!error_out.length()) error_out = Update.errorString();
          failed = true;
          break;
        }
      }
    }
  }

  free(stage_buf);
  free(net_buf);

  if (failed) {
    if (update_started) Update.abort();
    Serial.printf("[Update] Install fehlgeschlagen: %s\n", error_out.c_str());
    return false;
  }

  if (!Update.end(true)) {
    error_out = Update.errorString();
    Serial.printf("[Update] Update.end fehlgeschlagen: %s\n", error_out.c_str());
    return false;
  }
  Serial.printf("[Update] %u Bytes installiert - bereit zum Neustart\n",
                static_cast<unsigned>(total_sz));
  return true;
}

}  // namespace GithubUpdate
