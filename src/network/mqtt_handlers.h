#ifndef MQTT_HANDLERS_H
#define MQTT_HANDLERS_H

#include <Arduino.h>

// MQTT Callback-Funktionen
void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
// Drains inbound MQTT messages that mqttCallback() queued (see mqtt_handlers.cpp
// header comment) and runs the real per-topic processing on the caller's task.
// Call from the main loop(). max_msgs=0 drains everything currently queued.
void mqtt_process_inbound_queue(uint8_t max_msgs = 0);
void mqttSubscribeTopics();
void mqttPublishDiscovery();
void mqttPublishScene(const char* scene_name);
void mqttPublishSwitchCommand(const char* entity_id, const char* state);
void mqttPublishMediaCommand(const char* entity_id, const char* command);
void mqttPublishMediaVolume(const char* entity_id, float volume_level);
void mqttPublishMediaMute(const char* entity_id, bool muted);
void mqttPublishLightCommand(const char* entity_id,
                             const char* state,
                             int brightness_pct,
                             bool has_color,
                             uint32_t color,
                             int color_temp_kelvin = -1);
void mqttPublishHistoryRequest(const char* entity_id,
                               uint16_t hours = 24,
                               uint16_t period_minutes = 5,
                               uint16_t points = 288);
void mqttPublishWeatherRequest(const char* entity_id);
bool mqttPublishEnergyRequest(const char* period = "day");
void mqttPublishHomeSnapshot();
void mqttPublishDeviceSettings();
void mqttServiceLocalSensors();
void mqttReloadDynamicSlots();

#endif // MQTT_HANDLERS_H
