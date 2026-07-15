#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <vector>

static constexpr uint8_t ENERGY_VALUES_MAX = 32;

struct EnergyEntryData {
  String id;
  String name;
  String unit;
  String category;
  String period;
  String start;
  bool is_cost = false;
  bool is_total = false;
  bool has_cost = false;
  int8_t sign = 1;
  float total = 0.0f;
  float cost = 0.0f;
  uint8_t value_count = 0;
  float values[ENERGY_VALUES_MAX] = {};
  bool value_valid[ENERGY_VALUES_MAX] = {};
};

void queue_energy_response(const char* payload, size_t len);
void process_energy_response_queue();
bool energy_request_period(const char* period, bool force = false);
bool energy_request_day_for_tiles(bool force = false);
void energy_service_periodic();
bool energy_find_entry(const String& id, const char* period, EnergyEntryData& out);
// Einheit aus einem beliebigen bereits empfangenen Energy-Zeitraum holen.
// Aggregierte Energy-IDs existieren nicht zwingend als normale HA-Entity und
// fehlen deshalb im allgemeinen HaBridgeConfig-Unit-Index.
String energy_find_cached_unit(const String& id);
// Fuegt alle IDs aus bereits empfangenen Energy-Antworten hinzu. Damit kann
// die WebUI ihre Quellen auch dann anbieten, wenn eine Bridge-Konfigurations-
// antwort voruebergehend keinen Energy-Block enthielt.
void energy_append_cached_entity_ids(std::vector<String>& ids);
