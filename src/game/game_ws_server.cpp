#include "src/game/game_ws_server.h"
#include "src/network/ha_bridge_config.h"
#include "src/ui/tab_tiles_unified.h"
#include <ArduinoJson.h>

// Globale Instanz
GameWSServer gameWSServer;

// Statische Referenz für Callback
static GameWSServer* g_instance = nullptr;

static bool handle_sensor_update_message(const uint8_t* payload, size_t length) {
  if (!payload || length == 0) return false;

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.printf("[GameWS] JSON parse error: %s\n", err.c_str());
    return false;
  }

  const char* type = doc["type"] | "";
  if (strcmp(type, "pc_metrics") != 0 && strcmp(type, "sensor_update") != 0) {
    return false;
  }

  JsonArray sensors = doc["sensors"].as<JsonArray>();
  if (sensors.isNull()) {
    sensors = doc["metrics"].as<JsonArray>();
  }
  if (sensors.isNull()) {
    return true;
  }

  for (JsonVariant v : sensors) {
    if (!v.is<JsonObject>()) continue;
    const char* entity_id = v["entity_id"] | v["entity"] | v["id"] | "";
    if (!entity_id || !*entity_id) continue;

    String value = v["value"].as<String>();
    value.trim();
    if (!value.length()) continue;

    String unit = v["unit"].as<String>();
    unit.trim();
    String name = v["name"].as<String>();
    name.trim();

    haBridgeConfig.registerSensorMeta(entity_id, name, unit);
    haBridgeConfig.updateSensorValue(entity_id, value);
    tiles_update_sensor_by_entity(GridType::TAB0, entity_id, value.c_str());
    tiles_update_sensor_by_entity(GridType::TAB1, entity_id, value.c_str());
    tiles_update_sensor_by_entity(GridType::TAB2, entity_id, value.c_str());
  }

  return true;
}

void GameWSServer::init(uint16_t port) {
  if (running) {
    Serial.println("[GameWS] Bereits initialisiert");
    return;
  }

  Serial.printf("[GameWS] Starte WebSocket Server auf Port %u...\n", port);

  ws = new WebSocketsServer(port);
  g_instance = this;

  ws->begin();
  ws->onEvent(onWebSocketEvent);

  // Ping/Pong Keepalive aktivieren (alle 25 Sekunden)
  ws->enableHeartbeat(25000, 3000, 2);

  running = true;

  Serial.printf("[GameWS] WebSocket Server läuft auf Port %u (Keepalive aktiv)\n", port);
  Serial.println("[GameWS] Clients können sich verbinden: ws://<tab5-ip>:8081");
}

void GameWSServer::handle() {
  if (ws && running) {
    ws->loop();
  }
}

void GameWSServer::broadcastButtonPress(uint8_t slot, const char* name, uint8_t key_code, uint8_t modifier) {
  if (!ws || !running) return;

  // JSON erstellen
  JsonDocument doc;
  doc["type"] = "button_press";
  doc["slot"] = slot;
  doc["name"] = name;
  doc["key"] = key_code;
  doc["modifier"] = modifier;
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);

  // An alle verbundenen Clients broadcasten
  ws->broadcastTXT(json);

  Serial.printf("[GameWS] Broadcast: %s\n", json.c_str());
}

uint8_t GameWSServer::getClientCount() const {
  if (!ws || !running) return 0;
  return ws->connectedClients();
}

void GameWSServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[GameWS] Client #%u disconnected\n", num);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = g_instance->ws->remoteIP(num);
      Serial.printf("[GameWS] Client #%u connected from %s\n", num, ip.toString().c_str());

      // Willkommensnachricht senden
      JsonDocument doc;
      doc["type"] = "connected";
      doc["message"] = "Tab5 Game Controls WebSocket";
      doc["version"] = "1.0";

      String json;
      serializeJson(doc, json);
      g_instance->ws->sendTXT(num, json);
      break;
    }

    case WStype_TEXT:
      if (!handle_sensor_update_message(payload, length)) {
        Serial.printf("[GameWS] Client #%u sent: %s\n", num, payload);
      }
      // Hier könnten wir später Befehle vom Client empfangen (z.B. Button-Config)
      break;

    case WStype_ERROR:
      Serial.printf("[GameWS] Client #%u error\n", num);
      break;

    default:
      break;
  }
}
