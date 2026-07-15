#include "src/network/network_manager.h"
#include "src/core/config_manager.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/network/ha_bridge_config.h"
#include "src/web/web_admin.h"
#include "src/ui/ui_manager.h"
#include "src/ui/tab_settings.h"
#include "src/devices/device.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <ESPmDNS.h>

// Globale Instanz
Tab5NetworkManager networkManager;

static constexpr uint16_t kMqttBufferOta = 1024;
static constexpr uint16_t kMqttBufferNormal = 16 * 1024;
// Sobald Media-Tiles konfiguriert sind, muss der "normale" Puffer die
// Bridge-Media-States mit eingebettetem 240px-Cover fassen (~14 KB JPEG ->
// ~19 KB Base64+JSON). Mit 16 KB verwirft PubSubClient diese Pakete komplett;
// Cover kamen dann nur zufaellig waehrend eines 32-KB-Large-Fensters durch.
static constexpr uint16_t kMqttBufferMedia = 24 * 1024;
static constexpr uint16_t kMqttBufferLarge = 32 * 1024;
static constexpr uint32_t kMqttPostConnectQuietMs = 3000;
static constexpr uint8_t kMqttOutboundDrainNormal = 12;
static constexpr uint8_t kMqttOutboundDrainStorm = 1;
static constexpr size_t kMqttMinDmaLargestBeforeTx = 8 * 1024;
// Der esp-hosted-RX-Pfad fordert pro Frame rund 4,5 KB zusammenhaengenden
// internen DMA-Speicher an und assertet bei nullptr. 16 KB lassen selbst nach
// einer Frame-Allokation noch deutlich Reserve. 24 KB waren mit dem dauerhaft
// benoetigten 24-KB-Media-Puffer auf dem 8-Zoll-P4 jedoch unerreichbar
// (gemessen: stabil 18 KB) und blockierten dadurch die gesamte Queue. Die
// zusaetzliche 50-ms-Abstandsregel unten verhindert weiterhin RX-Bursts.
static constexpr size_t kMqttMinDmaLargestBeforeControl = 16 * 1024;
// Subscribe/Unsubscribe kann sofort ein retained Paket ausloesen. Auf dem P4
// bekommt der SDIO-RX-Task zwischen diesen Kontrollpaketen Zeit, das Paket bis
// in die MQTT-Inbound-Queue weiterzureichen und seinen DMA-Puffer freizugeben.
static constexpr uint32_t kMqttSdioControlQuietMs = 50;

// Waehrend dieses Fensters direkt nach dem Connect bleibt der MQTT-Empfangs-
// puffer klein (16 KB). Der PubSubClient-Puffer liegt im internen RAM; ihn
// mitten im retained-Message-Sturm auf 32 KB zu vergroessern nimmt dem
// C6/SDIO-WLAN genau im RX-Peak das DMA-RAM weg -> Freeze beim Start.
static constexpr uint32_t kMqttStormWindowMs = 8000;

// ---------------------------------------------------------------------------
// Outbound-Command-Queue (Single-Owner MQTT)
//
// Gegenstueck zur Inbound-Queue in mqtt_handlers.cpp: jeder Task darf
// enqueuen, NUR der Worker-Task nimmt heraus und fasst mqtt_client an.
// Ein Allokations-Block pro Kommando, [MqttOutboundCmd][topic\0][payload],
// PSRAM bevorzugt -- 1:1 das Muster von mqttAllocInbound().
// ---------------------------------------------------------------------------
enum class MqttCmdKind : uint8_t { PUBLISH, SUBSCRIBE, UNSUBSCRIBE };

struct MqttOutboundCmd {
  MqttCmdKind kind;
  bool retain;
  size_t payload_len;
  char* topic;       // -> in dieselbe Allokation
  uint8_t* payload;  // -> in dieselbe Allokation (leer bei SUBSCRIBE/UNSUBSCRIBE)
};

// 64 reichte rechnerisch fuer einen mqttReloadDynamicSlots()-Burst; der
// Post-Connect-Burst (kRoutes + Discovery + Settings + Snapshot + Reload,
// zusammen ~90 Kommandos) kann aber auflaufen, wenn der Worker gerade in
// einem grossen readPacket() steckt -- deshalb 128.
static constexpr size_t kMqttOutboundQueueDepth = 128;
static QueueHandle_t g_mqtt_outbound_queue = nullptr;
static uint32_t g_mqtt_outbound_dropped = 0;
static uint32_t g_mqtt_last_tx_guard_log_ms = 0;
static uint32_t g_mqtt_sdio_control_quiet_until = 0;

static MqttOutboundCmd* mqttAllocOutbound(MqttCmdKind kind,
                                          const char* topic,
                                          const uint8_t* payload,
                                          size_t payload_len,
                                          bool retain) {
  if (!topic || !*topic) return nullptr;
  const size_t topic_len = strlen(topic);
  const size_t total = sizeof(MqttOutboundCmd) + topic_len + 1 + payload_len;
  uint8_t* block = static_cast<uint8_t*>(heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!block) block = static_cast<uint8_t*>(heap_caps_malloc(total, MALLOC_CAP_8BIT));
  if (!block) return nullptr;
  MqttOutboundCmd* cmd = reinterpret_cast<MqttOutboundCmd*>(block);
  cmd->kind = kind;
  cmd->retain = retain;
  cmd->payload_len = payload_len;
  cmd->topic = reinterpret_cast<char*>(block + sizeof(MqttOutboundCmd));
  cmd->payload = reinterpret_cast<uint8_t*>(cmd->topic + topic_len + 1);
  memcpy(cmd->topic, topic, topic_len);
  cmd->topic[topic_len] = '\0';
  if (payload_len) memcpy(cmd->payload, payload, payload_len);
  return cmd;
}

// Nie blockieren, nie inline verarbeiten (das wuerde mqtt_client vom
// falschen Task beruehren) -- bei voller Queue/Alloc-Fehler verwerfen+loggen.
static bool enqueueOutboundCmd(MqttCmdKind kind,
                               const char* topic,
                               const uint8_t* payload,
                               size_t payload_len,
                               bool retain,
                               bool priority = false) {
  if (!g_mqtt_outbound_queue) return false;
  MqttOutboundCmd* cmd = mqttAllocOutbound(kind, topic, payload, payload_len, retain);
  if (!cmd) {
    Serial.println("[MQTT] Outbound-Alloc fehlgeschlagen -> Kommando verworfen");
    return false;
  }
  const BaseType_t queued = priority
                                ? xQueueSendToFront(g_mqtt_outbound_queue,
                                                    &cmd, 0)
                                : xQueueSend(g_mqtt_outbound_queue, &cmd, 0);
  if (queued != pdTRUE) {
    heap_caps_free(cmd);
    ++g_mqtt_outbound_dropped;
    Serial.printf("[MQTT] Outbound-Queue voll -> verworfen (#%u)\n",
                  static_cast<unsigned>(g_mqtt_outbound_dropped));
    return false;
  }
  return true;
}

static void purgeOutboundQueue() {
  if (!g_mqtt_outbound_queue) return;
  MqttOutboundCmd* cmd = nullptr;
  while (xQueueReceive(g_mqtt_outbound_queue, &cmd, 0) == pdTRUE) {
    if (cmd) heap_caps_free(cmd);
  }
}

// Volle 48-Bit-MAC statt nur der unteren 16 Bit: zwei Geraete aus aehnlicher
// Fertigungscharge koennen in den unteren 16 Bit kollidieren (bei diesem
// Nutzer beobachtet -- zwei Panels meldeten dieselbe device_id und HA hat
// eines der beiden Zeroconf-Discovery-Events stillschweigend als "schon
// konfiguriert" verworfen). Kein "tab5_lvgl_"-Praefix mehr, nur die reine
// MAC als Hex-String.
void buildDeviceId(char* buffer, size_t len) {
  if (!buffer || !len) return;
  uint64_t mac = ESP.getEfuseMac();
  snprintf(buffer, len, "%012llX", (unsigned long long)(mac & 0xFFFFFFFFFFFFULL));
}

static void logMdnsHeap(const char* tag) {
  const uint32_t dma_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  Serial.printf("[mDNS] %s | DMA free=%u KB | DMA largest=%u KB\n",
                tag, dma_free / 1024, dma_largest / 1024);
}

static void logNetworkHeap(const char* tag) {
  const uint32_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const uint32_t dma_free =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t dma_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  Serial.printf("[Network/Mem] %s | Int free=%u KB | Int largest=%u KB | "
                "DMA free=%u KB | DMA largest=%u KB | PSRAM free=%u KB\n",
                tag ? tag : "?",
                static_cast<unsigned>(int_free / 1024),
                static_cast<unsigned>(int_largest / 1024),
                static_cast<unsigned>(dma_free / 1024),
                static_cast<unsigned>(dma_largest / 1024),
                static_cast<unsigned>(psram_free / 1024));
}

static bool parseConfiguredIp(const char* value, IPAddress& out) {
  if (!value || !value[0]) return false;
  String text = value;
  text.trim();
  if (!text.length()) return false;
  return out.fromString(text);
}

static void applyWifiAddressing(const DeviceConfig& cfg) {
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;

  const bool has_ip = parseConfiguredIp(cfg.wifi_static_ip, ip);
  const bool has_gateway = parseConfiguredIp(cfg.wifi_gateway, gateway);
  const bool has_subnet = parseConfiguredIp(cfg.wifi_subnet, subnet);
  const bool has_dns = parseConfiguredIp(cfg.wifi_dns, dns);

  if (has_ip || has_gateway || has_subnet || has_dns) {
    if (has_ip && has_gateway && has_subnet) {
      if (!has_dns) {
        dns = gateway;
      }
      if (WiFi.config(ip, gateway, subnet, dns)) {
        Serial.printf("WiFi: Static IP %s / GW %s / MASK %s / DNS %s\n",
                      ip.toString().c_str(),
                      gateway.toString().c_str(),
                      subnet.toString().c_str(),
                      dns.toString().c_str());
      } else {
        Serial.println("WiFi: Static IP configuration failed, fallback to DHCP");
        WiFi.config(IPAddress(), IPAddress(), IPAddress());
      }
    } else {
      Serial.println("WiFi: Incomplete static IP configuration, fallback to DHCP");
      WiFi.config(IPAddress(), IPAddress(), IPAddress());
    }
  } else {
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
  }
}

// ========== Initialisierung ==========
void Tab5NetworkManager::init() {
  Serial.println("🌐 Initialisiere Network Manager...");

  if (!configManager.isConfigured()) {
    Serial.println("⚠️ Keine Netzwerk-Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  // WiFi-Setup
  WiFi.mode(WIFI_STA);
  logNetworkHeap("after-WiFi.mode");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  wifi_retry_at = 0;  // Sofortiger Verbindungsversuch

  // Bridge-/Request-Topics EINMALIG hier bauen: sie haengen nur von der
  // (laufzeit-konstanten) Efuse-MAC ab. Frueher baute connectMqtt() sie bei
  // jedem Reconnect neu -- sobald connectMqtt() auf dem Worker laeuft,
  // waehrend der Loop-Task getBridgeApplyTopic() etc. liest, waere jedes
  // String-Reassignment ein echtes Race. init() laeuft vor dem Worker-Start.
  char did[24];
  buildDeviceId(did, sizeof(did));
  String base = "tab5_lvgl/config/";
  base += did;
  bridge_apply_topic_ = base + "/bridge/apply";
  bridge_request_topic_ = base + "/bridge/request";
  history_request_topic_ = base + "/history/request";
  history_response_topic_ = base + "/history/response";
  weather_request_topic_ = base + "/weather/request";
  energy_request_topic_ = base + "/energy/request";
  energy_response_topic_ = base + "/energy/response";
  bridge_icons_topic_ = base + "/bridge/icons";

  mqtt_enabled = configManager.hasMqttConfig();
  if (mqtt_enabled) {
    // MQTT-Setup (vor Worker-Start, daher direkter Client-Zugriff okay)
    mqtt_client.setClient(net_client);
    mqtt_client.setServer(cfg.mqtt_host, cfg.mqtt_port);
    setMqttBufferSize(mqttNormalBufferSize(), "init");
    mqtt_client.setCallback(mqttCallback);
  } else {
    Serial.println("MQTT: keine Konfiguration vorhanden - ueberspringe Verbindung");
  }

  Serial.println("✓ Network Manager initialisiert");
}

// ========== WiFi verbinden ==========
void Tab5NetworkManager::connectWifi() {
  wifi_retry_at = millis() + 5000UL;  // Retry in 5s

  // Jeder Verbindungsaufbau (manuell, neue Zugangsdaten, AP-Ende) hebt ein
  // vorheriges manuelles Trennen wieder auf.
  if (wifi_manual_disconnect) {
    wifi_manual_disconnect = false;
    WiFi.setAutoReconnect(true);
  }

  if (!configManager.isConfigured()) {
    Serial.println("WiFi: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();
  if (cfg.wifi_ssid && cfg.wifi_ssid[0]) {
    Serial.printf("WiFi: Verbinde mit %s\n", cfg.wifi_ssid);
    applyWifiAddressing(cfg);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
  }
}

// ========== WiFi manuell trennen (WLAN-Popup "Trennen") ==========
void Tab5NetworkManager::disconnectWifiManual() {
  wifi_manual_disconnect = true;
  if (isMqttConnected()) disconnectMqtt();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  Serial.println("WiFi: manuell getrennt (kein Auto-Reconnect bis Verbinden/Neustart)");
}

// ========== MQTT verbinden (worker-only) ==========
void Tab5NetworkManager::connectMqtt() {
  if (!mqtt_enabled) return;
  mqtt_retry_at = millis() + 3000UL;  // Retry in 3s

  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt_large_until == 0 && mqtt_client.getBufferSize() < mqttNormalBufferSize()) {
    setMqttBufferSize(mqttNormalBufferSize(), "connect");
  }

  if (!configManager.isConfigured()) {
    Serial.println("MQTT: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  char client_id[CONFIG_MQTT_CLIENT_ID_MAX];
  if (cfg.mqtt_client_id[0]) {
    snprintf(client_id, sizeof(client_id), "%s", cfg.mqtt_client_id);
  } else {
    const unsigned long long mac = static_cast<unsigned long long>(ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL);
    snprintf(client_id, sizeof(client_id), "Tab5_LVGL-%012llX", mac);
  }

  Serial.printf("MQTT: Verbinde mit %s:%u als %s\n", cfg.mqtt_host, cfg.mqtt_port, client_id);

  const char* stat_topic = mqttTopics.topic(TopicKey::STAT_CONN);
  if (!stat_topic || !*stat_topic) {
    stat_topic = "tab5/stat/connected";
  }

  bool ok = false;
  if (cfg.mqtt_user && cfg.mqtt_user[0]) {
    ok = mqtt_client.connect(client_id, cfg.mqtt_user, cfg.mqtt_pass,
                             stat_topic, 0, true, "0");
  } else {
    ok = mqtt_client.connect(client_id, nullptr, nullptr,
                             stat_topic, 0, true, "0");
  }

  if (!ok) {
    Serial.printf("MQTT: Verbindung fehlgeschlagen, State=%d\n", mqtt_client.state());
    return;
  }

  Serial.println("✓ MQTT verbunden");
  mqtt_connected_at = millis();
  logNetworkHeap("after-MQTT-connect");

  // Status publizieren und die Antwort-Topics direkt subscriben -- direkter
  // Client-Zugriff ist hier safe, weil connectMqtt() ausschliesslich auf dem
  // Worker laeuft (Single-Owner).
  mqtt_client.publish(stat_topic, "1", true);
  const char* ip_topic = mqttTopics.topic(TopicKey::STAT_IP);
  if (ip_topic && *ip_topic) {
    mqtt_client.publish(ip_topic, WiFi.localIP().toString().c_str(), true);
  }
  if (!bridge_apply_topic_.isEmpty()) {
    mqtt_client.subscribe(bridge_apply_topic_.c_str());
    Serial.printf("[MQTT] Listening for bridge config on %s\n", bridge_apply_topic_.c_str());
  }
  if (!history_response_topic_.isEmpty()) {
    mqtt_client.subscribe(history_response_topic_.c_str());
    Serial.printf("[MQTT] Listening for history responses on %s\n", history_response_topic_.c_str());
  }
  if (!energy_response_topic_.isEmpty()) {
    mqtt_client.subscribe(energy_response_topic_.c_str());
    Serial.printf("[MQTT] Listening for energy responses on %s\n", energy_response_topic_.c_str());
  }
  if (!bridge_icons_topic_.isEmpty()) {
    mqtt_client.subscribe(bridge_icons_topic_.c_str());
    Serial.printf("[MQTT] Listening for icon updates on %s\n", bridge_icons_topic_.c_str());
  }

  // Veraltete Kommandos aus der Offline-Zeit verwerfen: solange die
  // Verbindung weg war, hat der isMqttConnected()-Check der Publish-
  // Funktionen neue Enqueues verhindert -- was hier noch liegt, stammt aus
  // dem Moment des Abrisses und wuerde sonst verspaetet feuern.
  purgeOutboundQueue();

  mqtt_connected_flag = true;

  // Die App-Ebene (mqttSubscribeTopics/Discovery/DeviceSettings/Snapshot)
  // faehrt der LOOP-Task hoch (mqttServicePostConnect): diese Funktionen
  // scannen Flash, pumpen LVGL und lesen Batterie-I2C -- nichts davon darf
  // auf dem Worker laufen. Ihre publishes/subscribes kommen per
  // Outbound-Queue hierher zurueck.
  mqtt_post_connect_ready_at = mqtt_connected_at + kMqttPostConnectQuietMs;
  mqtt_post_connect_pending = true;

  // Bridge refresh is handled by the background refresh after startup.
  // Doing it here collides with retained media/weather payloads and forces
  // the MQTT buffer to 32 KB during the tightest internal-RAM window.
}

// ========== Single-Owner MQTT: Worker ==========
void Tab5NetworkManager::beginMqttWorker() {
  if (!g_mqtt_outbound_queue) {
    g_mqtt_outbound_queue = xQueueCreate(kMqttOutboundQueueDepth, sizeof(MqttOutboundCmd*));
    if (!g_mqtt_outbound_queue) {
      Serial.println("[MQTT] Outbound-Queue konnte nicht erstellt werden");
    }
  }
}

// Worker-Task-Body: die EINZIGE Stelle, die mqtt_client nach init() anfasst.
void Tab5NetworkManager::serviceMqttWorker() {
  // Reconfigure-Request zuerst und VOR dem mqtt_enabled-Gate geprueft: genau
  // dieses Flag soll hier live neu gesetzt werden (Erstkonfiguration ueber
  // die Admin-Seite, Host geleert, Host geaendert). Alle anderen Requests
  // unten bleiben bewusst hinter dem Gate, die betreffen nur ein Geraet, das
  // bereits mqtt_enabled war.
  if (mqtt_reconfig_requested) {
    mqtt_reconfig_requested = false;
    if (mqtt_client.connected()) {
      const char* stat_topic = mqttTopics.topic(TopicKey::STAT_CONN);
      if (stat_topic && *stat_topic) {
        // Sauberes "0" vor dem Disconnect -- ein regulaeres MQTT DISCONNECT
        // (was PubSubClient::disconnect() sendet) loest das Last-Will NICHT
        // aus, die Bridge wuerde also faelschlich "verbunden" weiterzeigen.
        mqtt_client.publish(stat_topic, "0", true);
      }
      mqtt_client.disconnect();
      Serial.println("[MQTT] Disconnect fuer Reconfigure");
    }
    purgeOutboundQueue();
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    mqtt_large_until = 0;
    if (mqtt_buffer_size > mqttNormalBufferSize()) {
      setMqttBufferSize(mqttNormalBufferSize(), "reconfig");
    }
    mqtt_connected_flag = false;

    mqtt_enabled = configManager.hasMqttConfig();
    if (mqtt_enabled) {
      const DeviceConfig& cfg = configManager.getConfig();
      mqtt_client.setClient(net_client);
      mqtt_client.setServer(cfg.mqtt_host, cfg.mqtt_port);
      mqtt_client.setCallback(mqttCallback);
      mqtt_retry_at = 0;  // sofortiger Verbindungsversuch, naechste Iteration
      Serial.println("[MQTT] Reconfigure: neue Einstellungen uebernommen");
    } else {
      Serial.println("[MQTT] Reconfigure: kein Host konfiguriert, bleibe getrennt");
    }
    return;
  }

  if (!mqtt_enabled) return;

  // Request-Flags zuerst -- auch im suspendierten Zustand, damit
  // restoreMqttBufferNormal() den Worker nach einem abgebrochenen OTA wieder
  // aufwecken kann.
  if (mqtt_ota_prep_requested) {
    mqtt_large_until = 0;
    if (mqtt_client.connected()) {
      mqtt_client.disconnect();
      Serial.println("[OTA] MQTT disconnected for OTA");
    }
    setMqttBufferSize(kMqttBufferOta, "ota");
    mqtt_connected_flag = false;
    mqtt_suspended = true;  // waehrend OTA weder reconnecten noch loop() pumpen
    mqtt_ota_prep_requested = false;
    return;
  }
  if (mqtt_restore_normal_requested) {
    mqtt_restore_normal_requested = false;
    mqtt_large_until = 0;
    setMqttBufferSize(mqttNormalBufferSize(), "normal");
    mqtt_suspended = false;  // nach abgebrochenem OTA weitermachen
    return;
  }
  if (mqtt_disconnect_requested) {
    if (mqtt_client.connected()) {
      mqtt_client.disconnect();
      Serial.println("[MQTT] Disconnect auf Anforderung (Hotspot-Modus)");
    }
    purgeOutboundQueue();
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    mqtt_large_until = 0;
    if (mqtt_buffer_size > mqttNormalBufferSize()) {
      setMqttBufferSize(mqttNormalBufferSize(), "disconnect");
    }
    mqtt_connected_flag = false;
    mqtt_disconnect_requested = false;
    return;
  }
  if (mqtt_suspended) return;

  if (WiFi.status() != WL_CONNECTED) {
    if (mqtt_connected_flag) mqtt_connected_flag = false;
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    return;
  }

  const uint32_t now_ms = millis();
  const uint32_t hold_until = mqtt_reconnect_hold_until;
  if (hold_until != 0 && (int32_t)(now_ms - hold_until) < 0) {
    if (mqtt_client.connected()) {
      mqtt_client.disconnect();
      Serial.println("[MQTT] Disconnect waehrend Reconnect-Ruhefenster");
    }
    if (mqtt_connected_flag) mqtt_connected_flag = false;
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    return;
  }
  if (hold_until != 0) {
    mqtt_reconnect_hold_until = 0;
  }

  serviceBufferHousekeeping(now_ms);

  if (!mqtt_client.connected()) {
    if (mqtt_connected_flag) {
      mqtt_connected_flag = false;
      Serial.println("[MQTT] Verbindung verloren");
    }
    if ((int32_t)(now_ms - mqtt_retry_at) >= 0) {
      connectMqtt();
    }
    return;
  }

  // Erst ausgehende Kommandos, dann den Socket pumpen. Housekeeping lief
  // davor, damit ein requestLargeMqttBuffer() des Loop-Tasks noch vor dem
  // zugehoerigen (gerade enqueueten) grossen Publish wirksam wird.
  const bool startup_storm =
      mqtt_connected_at != 0 && (uint32_t)(now_ms - mqtt_connected_at) < kMqttStormWindowMs;
  drainOutboundQueue(startup_storm ? kMqttOutboundDrainStorm : kMqttOutboundDrainNormal);
  mqtt_client.loop();
  if (!mqtt_client.connected()) {
    mqtt_connected_flag = false;
  }
}

void Tab5NetworkManager::drainOutboundQueue(uint8_t max_commands) {
  if (!g_mqtt_outbound_queue || max_commands == 0) return;
  MqttOutboundCmd* next_cmd = nullptr;
  if (xQueuePeek(g_mqtt_outbound_queue, &next_cmd, 0) != pdTRUE) return;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  const uint32_t now_ms = millis();
  if (g_mqtt_sdio_control_quiet_until != 0 &&
      static_cast<int32_t>(now_ms - g_mqtt_sdio_control_quiet_until) < 0) {
    return;
  }
  g_mqtt_sdio_control_quiet_until = 0;
  const size_t dma_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const bool next_is_sdio_control =
      next_cmd && next_cmd->kind != MqttCmdKind::PUBLISH;
  const size_t min_dma_largest = next_is_sdio_control
                                     ? kMqttMinDmaLargestBeforeControl
                                     : kMqttMinDmaLargestBeforeTx;
  if (dma_largest < min_dma_largest) {
    if ((uint32_t)(now_ms - g_mqtt_last_tx_guard_log_ms) >= 2000) {
      g_mqtt_last_tx_guard_log_ms = now_ms;
      Serial.printf("[MQTT] TX verschoben: DMA largest nur %u KB\n",
                    static_cast<unsigned>(dma_largest / 1024));
    }
    return;
  }
#endif
  MqttOutboundCmd* cmd = nullptr;
  uint32_t drained = 0;
  while (drained < max_commands &&
         xQueueReceive(g_mqtt_outbound_queue, &cmd, 0) == pdTRUE) {
    bool spaced_sdio_control = false;
    if (cmd) {
      bool ok = false;
      const char* verb = "?";
      switch (cmd->kind) {
        case MqttCmdKind::PUBLISH:
          verb = "publish";
          ok = mqtt_client.publish(cmd->topic, cmd->payload, cmd->payload_len, cmd->retain);
          break;
        case MqttCmdKind::SUBSCRIBE:
          verb = "subscribe";
          ok = mqtt_client.subscribe(cmd->topic);
          spaced_sdio_control = true;
          break;
        case MqttCmdKind::UNSUBSCRIBE:
          verb = "unsubscribe";
          ok = mqtt_client.unsubscribe(cmd->topic);
          spaced_sdio_control = true;
          break;
      }
      if (!ok) {
        Serial.printf("[MQTT] Worker: %s '%s' fehlgeschlagen\n", verb, cmd->topic);
      }
      heap_caps_free(cmd);
    }
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (spaced_sdio_control) {
      g_mqtt_sdio_control_quiet_until = millis() + kMqttSdioControlQuietMs;
    }
#endif
    // subscribe()/unsubscribe() warten laut PubSubClient-Quelle nicht auf ein
    // Ack (Fire-and-Forget-Write) -- trotzdem soll ein grosser Drain-Burst
    // den Idle-Task (Watchdog-Futter) nie aushungern: alle 8 Kommandos ein
    // echter Scheduler-Yield.
    if ((++drained & 0x07) == 0) {
      vTaskDelay(1);
    }
    if (spaced_sdio_control) break;
  }
}

// Grow/Shrink-Logik des Empfangspuffers, 1:1 aus dem frueheren update()-Code:
// laeuft nur noch auf dem Worker, weil setBufferSize() den Client anfasst.
void Tab5NetworkManager::serviceBufferHousekeeping(uint32_t now_ms) {
  const uint32_t large_until = mqtt_large_until;
  if (large_until == 0) {
    // Kein Large-Fenster aktiv: Normalgroesse an die Media-Konfiguration
    // angleichen (Media-Tile hinzugefuegt/entfernt -> 24 KB rauf/runter).
    // OTA-Modus (1-KB-Puffer) nicht anfassen; Grows warten wie der
    // Large-Grow das Startup-Sturmfenster ab, Shrinks sind sofort okay.
    const uint16_t normal_size = mqttNormalBufferSize();
    if (mqtt_buffer_size != 0 && mqtt_buffer_size != kMqttBufferOta &&
        mqtt_buffer_size != normal_size &&
        (mqtt_buffer_size > normal_size ||
         mqtt_connected_at == 0 ||
         (uint32_t)(now_ms - mqtt_connected_at) >= kMqttStormWindowMs)) {
      setMqttBufferSize(normal_size, "media-config");
    }
    return;
  }
  if ((int32_t)(now_ms - large_until) >= 0) {
    mqtt_large_until = 0;
    setMqttBufferSize(mqttNormalBufferSize(), "normal");
  } else if (mqtt_buffer_size < kMqttBufferLarge &&
             (mqtt_connected_at == 0 ||
              (uint32_t)(now_ms - mqtt_connected_at) >= kMqttStormWindowMs)) {
    // Ausserhalb des Startup-Sturms sofort vergroessern; im Sturm bleibt der
    // Grow aufgeschoben und wird hier automatisch nachgeholt, sobald das
    // Fenster vorbei ist (frueher "large-deferred" in update()).
    setMqttBufferSize(kMqttBufferLarge, "large");
  }
}

// ========== Single-Owner MQTT: API fuer andere Tasks ==========
bool Tab5NetworkManager::mqttEnqueuePublish(const char* topic, const char* payload, bool retain) {
  const size_t len = payload ? strlen(payload) : 0;
  return enqueueOutboundCmd(MqttCmdKind::PUBLISH, topic,
                            reinterpret_cast<const uint8_t*>(payload), len, retain);
}

bool Tab5NetworkManager::mqttEnqueuePublish(const char* topic, const uint8_t* payload,
                                            size_t length, bool retain) {
  return enqueueOutboundCmd(MqttCmdKind::PUBLISH, topic, payload, length, retain);
}

bool Tab5NetworkManager::mqttEnqueuePublishPriority(const char* topic,
                                                    const char* payload,
                                                    bool retain) {
  const size_t len = payload ? strlen(payload) : 0;
  return enqueueOutboundCmd(MqttCmdKind::PUBLISH, topic,
                            reinterpret_cast<const uint8_t*>(payload), len,
                            retain, true);
}

bool Tab5NetworkManager::mqttEnqueueSubscribe(const char* topic) {
  return enqueueOutboundCmd(MqttCmdKind::SUBSCRIBE, topic, nullptr, 0, false);
}

bool Tab5NetworkManager::mqttEnqueueUnsubscribe(const char* topic) {
  return enqueueOutboundCmd(MqttCmdKind::UNSUBSCRIBE, topic, nullptr, 0, false);
}

bool Tab5NetworkManager::consumeMqttPostConnectPending() {
  if (!mqtt_post_connect_pending) return false;
  if (!mqtt_connected_flag) {
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    return false;
  }
  const uint32_t ready_at = mqtt_post_connect_ready_at;
  if (ready_at != 0 && (int32_t)(millis() - ready_at) < 0) {
    return false;
  }
  mqtt_post_connect_pending = false;  // einziger Konsument ist der Loop-Task
  mqtt_post_connect_ready_at = 0;
  return true;
}

void Tab5NetworkManager::disconnectMqtt() {
  if (!mqtt_enabled) return;
  mqtt_disconnect_requested = true;
  // Der Worker prueft das Flag am Anfang jeder Iteration (~2ms Takt); der
  // Aufrufer (Hotspot-Eintritt) ist selten und darf kurz warten.
  for (int i = 0; i < 100 && mqtt_disconnect_requested; ++i) {
    delay(5);
  }
  if (mqtt_disconnect_requested) {
    Serial.println("[MQTT] WARNUNG: Worker hat Disconnect-Request nicht bestaetigt");
  }
}

void Tab5NetworkManager::requestMqttReconfigure() {
  // Bewusst KEIN "if (!mqtt_enabled) return;" wie bei disconnectMqtt() --
  // der Worker soll mqtt_enabled hier gerade erst neu bestimmen (z.B. erste
  // MQTT-Konfiguration ueberhaupt, wo es bislang false war).
  mqtt_reconfig_requested = true;
  for (int i = 0; i < 100 && mqtt_reconfig_requested; ++i) {
    delay(5);
  }
  if (mqtt_reconfig_requested) {
    Serial.println("[MQTT] WARNUNG: Worker hat Reconfigure-Request nicht bestaetigt");
  }
}

void Tab5NetworkManager::prepareMqttForOta() {
  if (!mqtt_enabled) return;
  mqtt_ota_prep_requested = true;
  for (int i = 0; i < 100 && mqtt_ota_prep_requested; ++i) {
    delay(5);
  }
  if (mqtt_ota_prep_requested) {
    Serial.println("[OTA] WARNUNG: MQTT-Worker hat OTA-Vorbereitung nicht bestaetigt");
  }
}

void Tab5NetworkManager::deferMqttReconnect(uint32_t hold_ms) {
  if (!mqtt_enabled) return;
  if (hold_ms == 0) hold_ms = 6000;
  mqtt_reconnect_hold_until = millis() + hold_ms;
  mqtt_post_connect_pending = false;
  mqtt_post_connect_ready_at = 0;
  mqtt_large_until = 0;
  purgeOutboundQueue();
  Serial.printf("[MQTT] Reconnect fuer %u ms pausiert\n",
                static_cast<unsigned>(hold_ms));
}

// ========== MQTT-Status ==========
uint16_t Tab5NetworkManager::mqttNormalBufferSize() const {
  return mqtt_media_buffer_needed ? kMqttBufferMedia : kMqttBufferNormal;
}

bool Tab5NetworkManager::setMqttBufferSize(uint16_t size, const char* reason) {
  if (size == 0) return false;
  const uint16_t before = mqtt_client.getBufferSize();
  if (before == size) {
    mqtt_buffer_size = size;
    return true;
  }

  if (!mqtt_client.setBufferSize(size)) {
    Serial.printf("[MQTT] Buffer resize failed: %u -> %u bytes (%s)\n",
                  static_cast<unsigned>(before),
                  static_cast<unsigned>(size),
                  reason ? reason : "?");
    return false;
  }

  mqtt_buffer_size = mqtt_client.getBufferSize();
  Serial.printf("[MQTT] Buffer: %u -> %u bytes (%s)\n",
                static_cast<unsigned>(before),
                static_cast<unsigned>(mqtt_buffer_size),
                reason ? reason : "?");
  return true;
}

void Tab5NetworkManager::requestLargeMqttBuffer(uint32_t hold_ms) {
  if (hold_ms == 0) hold_ms = 15000;
  // Nur den Zeitstempel setzen -- das eigentliche setBufferSize() macht der
  // Worker in serviceBufferHousekeeping() (inkl. Startup-Sturm-Aufschub).
  mqtt_large_until = millis() + hold_ms;
}

void Tab5NetworkManager::restoreMqttBufferNormal() {
  // Request-Flag statt direktem Client-Touch; der Worker setzt den Puffer
  // zurueck und hebt dabei auch eine evtl. OTA-Suspendierung wieder auf
  // (Aufrufer: restoreDisplayAfterOtaFailure()).
  mqtt_restore_normal_requested = true;
}

// ========== WiFi-Status ==========
bool Tab5NetworkManager::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

// ========== Telemetrie senden ==========
void Tab5NetworkManager::publishTelemetry() {
  if (!isMqttConnected()) return;

  uint32_t now = millis();
  if (now - last_telemetry > 30000UL) {  // 30 Sekunden
    last_telemetry = now;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(now / 1000UL));
    const char* tele_topic = mqttTopics.topic(TopicKey::TELE_UP);
    if (tele_topic && *tele_topic) {
      mqttEnqueuePublish(tele_topic, buf, true);
    }
    mqttPublishHomeSnapshot();
  }
}

void Tab5NetworkManager::publishBridgeConfig() {
  if (!isMqttConnected()) return;
  if (!configManager.isConfigured()) return;

  const DeviceConfig& cfg = configManager.getConfig();
  char did[24];
  buildDeviceId(did, sizeof(did));
  String payload = haBridgeConfig.buildJsonPayload(did, cfg.mqtt_base_topic, cfg.ha_prefix);
  if (payload.isEmpty()) return;

  String topic = "tab5_lvgl/config/";
  topic += did;
  topic += "/bridge";
  requestLargeMqttBuffer(15000);
  const size_t packet_estimate = payload.length() + topic.length() + 16;
  if (packet_estimate > getMqttBufferSize()) {
    Serial.printf("[Network] Bridge config too large for MQTT buffer: %u > %u bytes\n",
                  static_cast<unsigned>(packet_estimate),
                  static_cast<unsigned>(getMqttBufferSize()));
  }
  mqttEnqueuePublish(topic.c_str(), payload.c_str(), true);
  Serial.println("[Network] Home Assistant Bridge-Konfiguration publiziert");
}

const char* Tab5NetworkManager::getBridgeApplyTopic() const {
  return bridge_apply_topic_.length() ? bridge_apply_topic_.c_str() : nullptr;
}

void Tab5NetworkManager::publishBridgeRequest() {
  if (!isMqttConnected()) return;
  if (bridge_request_topic_.isEmpty()) return;
  requestLargeMqttBuffer(30000);
  mqttEnqueuePublish(bridge_request_topic_.c_str(), "", false);
  Serial.println("[Network] Home Assistant Bridge-Aktualisierung angefordert");
}

const char* Tab5NetworkManager::getBridgeRequestTopic() const {
  return bridge_request_topic_.length() ? bridge_request_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getHistoryRequestTopic() const {
  return history_request_topic_.length() ? history_request_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getHistoryResponseTopic() const {
  return history_response_topic_.length() ? history_response_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getWeatherRequestTopic() const {
  return weather_request_topic_.length() ? weather_request_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getEnergyRequestTopic() const {
  return energy_request_topic_.length() ? energy_request_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getEnergyResponseTopic() const {
  return energy_response_topic_.length() ? energy_response_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getBridgeIconsTopic() const {
  return bridge_icons_topic_.length() ? bridge_icons_topic_.c_str() : nullptr;
}

// ========== mDNS-Advertising ==========
// Rein additiv: erlaubt der HA-Bridge (Zeroconf), das Geraet zu finden BEVOR
// es MQTT-Zugangsdaten hat. Laeuft ausschliesslich, waehrend WiFi verbunden
// UND keine MQTT-Verbindung steht (siehe update()) -- ein bereits gepairtes,
// verbundenes Geraet muss nicht laenger auffindbar sein. Wird immer erst NACH
// webAdminServer.start() versucht -- ein haengender/fehlschlagender
// MDNS.begin() darf die admin-UI, die schon heute funktioniert, nicht
// verzoegern.
void Tab5NetworkManager::startMdns() {
  if (mdns_active) return;

  char did[24];
  buildDeviceId(did, sizeof(did));

  // device_id ist reiner Hex-Text (keine Unterstriche mehr) -- als
  // mDNS-Hostname direkt verwendbar, ohne RFC-952/1123-Zeichenersetzung.
  char hostname[24];
  snprintf(hostname, sizeof(hostname), "%s", did);

  logMdnsHeap("before-begin");
  if (!MDNS.begin(hostname)) {
    Serial.println("[mDNS] begin() fehlgeschlagen -- Advertising uebersprungen");
    return;
  }
  MDNS.addService("hometiles", "tcp", 80);

  // addServiceTxt() ist auf char*/const char*/String ueberladen. Ein Mix aus
  // String-Literalen (ueber die deprecated Literal->char*-Konvertierung auch
  // fuer die char*-Ueberladung gueltig) und einem char[]-Puffer macht die
  // Ueberladung mehrdeutig -> alle vier Argumente als benannte const char*-
  // Variablen uebergeben, dann ist nur noch die const-char*-Variante gueltig.
  const char* svc_name = "hometiles";
  const char* svc_proto = "tcp";
  const char* key_txtvers = "txtvers";
  const char* val_txtvers = "1";
  const char* key_device_id = "device_id";
  const char* val_device_id = did;
  const char* key_name = "name";
  const char* val_name = Device::displayName();
  const char* key_model = "model";
  const char* val_model = Device::profile().key;
  const DeviceConfig& cfg = configManager.getConfig();
  const char* key_base_topic = "base_topic";
  const char* val_base_topic = cfg.mqtt_base_topic;
  const char* key_ha_prefix = "ha_prefix";
  const char* val_ha_prefix = cfg.ha_prefix;

  MDNS.addServiceTxt(svc_name, svc_proto, key_txtvers, val_txtvers);
  MDNS.addServiceTxt(svc_name, svc_proto, key_device_id, val_device_id);
  MDNS.addServiceTxt(svc_name, svc_proto, key_name, val_name);
  MDNS.addServiceTxt(svc_name, svc_proto, key_model, val_model);
  MDNS.addServiceTxt(svc_name, svc_proto, key_base_topic, val_base_topic);
  MDNS.addServiceTxt(svc_name, svc_proto, key_ha_prefix, val_ha_prefix);
  mdns_active = true;
  logMdnsHeap("after-begin");
}

void Tab5NetworkManager::stopMdns() {
  if (!mdns_active) return;
  MDNS.end();
  mdns_active = false;
}

// ========== Update-Schleife (Loop-Task) ==========
void Tab5NetworkManager::update() {
  if (!configManager.isConfigured()) {
    return;
  }

  uint32_t now_ms = millis();
  bool is_connected = (WiFi.status() == WL_CONNECTED);

  // WiFi-Verbindung verwalten
  if (!is_connected) {
    wifi_ps_state_known = false;

    // Nicht verbunden - Retry (ausser der Nutzer hat manuell getrennt)
    if (!wifi_manual_disconnect && (int32_t)(now_ms - wifi_retry_at) >= 0) {
      connectWifi();
    }

    // WebAdmin stoppen wenn Verbindung verloren
    if (was_connected && webAdminServer.isRunning()) {
      webAdminServer.stop();
    }
    if (was_connected) {
      stopMdns();
    }
  } else {
    // Verbunden

    if (!was_connected) {
      logNetworkHeap("WiFi-connected");
    }

    // WebAdmin starten wenn gerade verbunden
    if (!was_connected && !webAdminServer.isRunning()) {
      webAdminServer.start();
    }

    // mDNS: nur an, solange (noch) KEINE MQTT-Verbindung steht -- ein bereits
    // gepairtes, verbundenes Geraet muss nicht laenger auffindbar sein (spart
    // das bisschen RAM, haelt "Discovered"-Karten in HA sauber). Faellt die
    // Verbindung spaeter weg (Broker offline etc.), wird wieder advertised.
    // Beide Aufrufe sind ueber mdns_active geguardet und im bereits erreichten
    // Zielzustand billige No-Ops -- unproblematisch, das jeden Tick zu pruefen.
    if (mqtt_enabled && isMqttConnected()) {
      stopMdns();
    } else if (webAdminServer.isRunning()) {
      startMdns();
    }

    // NTP-Sync triggern bei neuer Verbindung
    if (!was_connected) {
      uiManager.scheduleNtpSync(0);
    }

    // MQTT-Verbindung/Socket/Puffer verwaltet komplett der Worker-Task
    // (serviceMqttWorker). Hier bleibt nur die Telemetrie, weil
    // publishTelemetry() -> mqttPublishHomeSnapshot() den Batterie-SoC per
    // I2C liest und deshalb auf dem Loop-Task bleiben muss; gesendet wird
    // ueber die Outbound-Queue.
    if (mqtt_enabled && isMqttConnected()) {
      publishTelemetry();
    }
  }

  // WiFi-Status für nächste Runde merken
  was_connected = is_connected;
}

// ========== WiFi Power Management ==========
void Tab5NetworkManager::setWifiPowerSaving(bool enable) {
  if (!isWifiConnected()) {
    wifi_ps_state_known = false;
    return;
  }

#if defined(CONFIG_IDF_TARGET_ESP32P4)
  // ESP32-P4 uses an SDIO/esp-hosted WiFi transport. Modem sleep can trigger
  // transport TX asserts under WebUI, MQTT, or media-cover traffic.
  enable = false;
#endif

  if (wifi_ps_state_known && wifi_ps_enabled == enable) {
    return;
  }

  if (enable) {
    if (wifi_sleep_profile) {
      // Sleep-Profil: Verbindung minimal halten, maximale Ersparnis.
      WiFi.setSleep(WIFI_PS_MAX_MODEM);
      WiFi.setTxPower(WIFI_POWER_5dBm);
      Serial.println("🔋 WiFi Sleep Profile: Max Modem Sleep + 5dBm");
    } else {
      // Normaler Stromsparmodus im Idle.
      WiFi.setSleep(WIFI_PS_MIN_MODEM);
      WiFi.setTxPower(WIFI_POWER_11dBm);
      Serial.println("🔋 WiFi Power Saving: Light Sleep + 11dBm");
    }
  } else {
    // Netzteilmodus: Volle Performance
    WiFi.setSleep(WIFI_PS_NONE);       // Kein Sleep
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximale Reichweite
    Serial.println("🔌 WiFi Full Power: No Sleep + 19.5dBm");
  }

  wifi_ps_state_known = true;
  wifi_ps_enabled = enable;
}

void Tab5NetworkManager::setSleepWifiProfile(bool enable) {
  if (wifi_sleep_profile == enable) return;
  wifi_sleep_profile = enable;
  // Profilwechsel soll beim naechsten setWifiPowerSaving() sicher angewendet werden.
  wifi_ps_state_known = false;
}
