# MQTT Single-Owner Architecture — Bauplan

> **STATUS (2026-07-02): UMGESETZT** — mit vier Korrekturen gegenüber diesem Plan:
>
> 1. **Schritt 4 war so nicht baubar:** Der Worker darf `mqttSubscribeTopics()`/
>    `mqttPublishDiscovery()`/`mqttPublishDeviceSettings()`/`mqttPublishHomeSnapshot()`
>    NICHT selbst aufrufen (auch nicht enqueue-basiert), weil diese Funktionen
>    nebenbei Flash scannen, LVGL pumpen und Batterie-I2C lesen. Stattdessen:
>    Worker setzt nach dem Connect ein `mqtt_post_connect_pending`-Flag, der
>    Loop-Task konsumiert es via `mqttServicePostConnect()` (neu in
>    mqtt_handlers.cpp) und führt die vier Funktionen bei sich aus.
> 2. `publishTelemetry()` bleibt aus demselben Grund (Batterie-I2C via
>    HomeSnapshot) im Loop-Task-`update()`, sendet per Outbound-Queue.
> 3. Der Inline-Fallback in `mqttCallback()` (processMqttMessage bei voller
>    Queue) wurde durch bounded `xQueueSend` (50ms) + Drop mit gedrosseltem Log
>    ersetzt — der Worker darf nie processMqttMessage() (Flash/LVGL) ausführen.
> 4. Worker wird während OTA suspendiert (`prepareMqttForOta` setzt
>    `mqtt_suspended`; `restoreMqttBufferNormal` hebt es wieder auf), sonst
>    würde er mitten im OTA reconnecten. Queue-Tiefe 128 statt 64
>    (Post-Connect-Burst). Zusätzlich drained/servict der Loop-Task die
>    Inbound-Queue jetzt auch im Display-Sleep (der Worker empfängt ja weiter).

Ziel: ein Task auf dem 2. Core kümmert sich ausschließlich um Netzwerk/MQTT-Daten,
der Loop-Task (Core 1) ausschließlich um die Oberfläche (LVGL). Kein Mutex nötig,
weil `PubSubClient`/`mqtt_client` nur noch von EINEM Task (dem Worker) je berührt wird.

Basis-Commit: `755ff50` (Branch `experiment/8inch-triple-partial`). Dieser Commit
ist stabil geflasht und bestätigt (kein Crash, MQTT/Sensoren normal). Er enthält
bereits den PubSubClient-Patch (siehe unten) — NICHT aber den Worker/die Queue.

## Vorgeschichte (warum diese Architektur, warum diese Reihenfolge)

Ein früherer Versuch, `mqtt_client.loop()` auf einem Worker-Task zu servicen und
alle Zugriffe per rekursivem Mutex abzusichern, ist zweimal gecrasht:

1. **Watchdog-Reboot** (`IDLE0`/CPU0 >5s nicht gefüttert, Worker-Task lief): Ursache
   war die Worker-Priorität (1, nicht Idle). Fix: Priorität auf `tskIDLE_PRIORITY`
   (0) gesetzt.
2. **Zweiter Watchdog-Reboot, trotz Idle-Priorität**, nach ca. 30 Minuten Laufzeit:
   Ursache im echten `PubSubClient`-Quellcode gefunden (nicht geraten):
   `readByte()` (`src/network/vendor/pubsubclient/PubSubClient.cpp`) yieldet NUR,
   während es auf das nächste Byte WARTET. `readPacket()` ruft `readByte()` einmal
   PRO BYTE des gesamten Payloads auf. Sobald ein TCP-Segment schon vollständig im
   Socket-Puffer liegt, läuft die innere Schleife tausende Bytes am Stück OHNE
   JEDEN YIELD — eine reine CPU-Schleife, die lange genug laufen kann, um den
   IDLE0-Task zu verhungern, UNABHÄNGIG von Priorität oder Mutex/Single-Owner
   (beide Ansätze rufen denselben Read-Call auf demselben Task auf).

   **Das ist bereits gefixt und Teil von `755ff50`:** PubSubClient wurde als eigene
   Projekt-Kopie nach `src/network/vendor/pubsubclient/` vendored (statt der
   globalen Arduino-Lib) und in `readPacket()`s Haupt-Schleife ein echtes
   `vTaskDelay(1)` alle 128 gelesenen Bytes eingebaut — auch wenn Daten sofort
   verfügbar sind, nicht nur beim Warten. Das gibt dem Scheduler regelmäßig eine
   echte Chance, unabhängig davon wie "gierig" der Socket Daten liefert.

Der Mutex-Ansatz selbst wurde NICHT als fehlerhaft verworfen — der Crash lag am
Lese-Loop, nicht an der Sperr-Logik. Aber der Nutzer möchte grundsätzlich (nicht
nur für diesen einen Fall) eine sauberere Architektur ohne Mutex: **Single-Owner**
statt "geteilter Zugriff + Sperre".

## Zielarchitektur

- **Worker-Task** (PSRAM-Stack, Core 0, `tskIDLE_PRIORITY`, analog zum früheren
  `mqtt_worker_task` in der `.ino`) ist der EINZIGE Code, der `mqtt_client`
  (das `PubSubClient`-Objekt) je direkt anfasst: `connect()`, `loop()`,
  `publish()`, `subscribe()`, `unsubscribe()`, `disconnect()`, `setBufferSize()`,
  `connected()`, `state()`.
- **Outbound-Command-Queue**: alle anderen Tasks (v.a. der Loop-Task bei
  Touch-Events, Settings-Änderungen, HA-Bridge-Reload-Reaktionen) legen ihre
  Sende-Wünsche nur in eine Queue, der Worker holt sie ab und führt sie aus.
- **Verbindungsstatus**: ein einzelnes `volatile bool`, NUR vom Worker
  geschrieben, von allen anderen Tasks nur gelesen — kein Lock nötig (ein
  Schreiber, viele Leser, einfacher aligned bool-Read/Write ist auf dieser
  Architektur atomar).
- **Kein Mutex irgendwo.**

## Aktueller IST-Zustand (Basis für den Umbau)

Datei-Übersicht der Basis (`755ff50`), UNVERÄNDERT gegenüber dem Single-Core-Stand
außer dem PubSubClient-Vendoring:

- `ESP32_P4_HomeAssistant_Display.ino`
  - `loop()` (~Z.611): `networkManager.serviceMqttLoop(); mqtt_process_inbound_queue();`
    jede Iteration.
  - `.ino:127`: `if (networkManager.isMqttConnected()) networkManager.getMqttClient().disconnect();`
    (Hotspot/AP-Modus-Eintritt).
  - `.ino:194`, `.ino:512`, `.ino:627`: reine `isMqttConnected()`-Reads.
- `src/network/network_manager.h/.cpp`
  - `Tab5NetworkManager` besitzt `PubSubClient mqtt_client` als private Member.
  - `connectMqtt()`: baut bei JEDEM Aufruf (auch Reconnect) die Topic-Strings
    (`bridge_apply_topic_` etc.) neu, macht den `mqtt_client.connect(...)`-Handshake,
    published/subscribed direkt.
  - `update()`: WiFi-Reconnect + `if (mqtt_enabled) { ... connectMqtt() / publishTelemetry() / Puffer-Housekeeping ... }`,
    läuft alle 5 Loop-Iterationen (Throttle).
  - `serviceMqttLoop()`: pumpt nur `mqtt_client.loop()`, läuft aktuell JEDE
    Loop-Iteration direkt im Loop-Task (kein Worker).
  - `requestLargeMqttBuffer()`/`restoreMqttBufferNormal()`: setzen `mqtt_large_until`
    UND rufen sofort `setMqttBufferSize()` (direkter Client-Touch).
  - `prepareMqttForOta()`: disconnect + Puffer auf 1KB, synchron.
  - `publishTelemetry()`/`publishBridgeConfig()`/`publishBridgeRequest()`: je
    `if (!mqtt_client.connected()) return; ... mqtt_client.publish(...)`.
- `src/network/mqtt_handlers.cpp` (1934 Zeilen)
  - Inbound-Queue (Etappe 1, bereits fertig/stabil): `mqttCallback()` reiht nur
    ein, `mqtt_process_inbound_queue()` verarbeitet auf dem Loop-Task
    (`processMqttMessage()`). **Diese Seite bleibt unverändert.**
  - ~15 Funktionen holen sich `PubSubClient& mqtt = networkManager.getMqttClient();`
    und rufen direkt `mqtt.publish(...)`/`.connected()` auf:
    `sync_external_temp_entity()` (Z.625), `mqttSubscribeTopics()` (Z.1474),
    `mqttPublishHomeSnapshot()` (Z.1488), `mqttPublishDeviceSettings()` (Z.1504),
    `mqttPublishScene()` (Z.1543), `mqttPublishSwitchCommand()` (Z.1557),
    `mqttPublishMediaCommand()` (Z.1586), `mqttPublishMediaVolume()` (Z.1608),
    `mqttPublishMediaMute()` (Z.1635), `mqttPublishLightCommand()` (Z.1660),
    `mqttPublishHistoryRequest()` (Z.1721), `mqttPublishWeatherRequest()` (Z.1782),
    `mqttPublishEnergyRequest()` (Z.1799), `mqttPublishDiscovery()` (Z.1831),
    `mqttReloadDynamicSlots()` (Z.1892).
- Weitere Aufrufer von `mqttPublishDeviceSettings()`/`mqttReloadDynamicSlots()`/
  `isMqttConnected()` außerhalb von network/mqtt_handlers: `src/core/power_manager.cpp`,
  `src/web/web_admin_handlers.cpp`, `src/ui/ui_manager.cpp`, `src/ui/tab_settings.cpp`,
  `src/types/energy/energy_data.cpp` — laufen alle auf dem Loop-Task (Touch/Web/Settings-Events).

## Umbau-Schritte

### 1. `network_manager.h` — neue öffentliche API

```cpp
// Single-Owner MQTT: nur der Worker-Task fasst mqtt_client je direkt an.
void beginMqttWorker();          // einmalig aus setup(), VOR Worker-Start (legt Queue an)
void serviceMqttWorker();        // Worker-Task-Body, wird in einer Schleife aufgerufen

bool isMqttConnected() const;    // liest nur noch das volatile Flag, kein Client-Touch mehr
uint16_t getMqttBufferSize() const;

// Von JEDEM Task sicher aufrufbar -- baut ein PSRAM-Command und legt es in die Queue.
bool mqttEnqueuePublish(const char* topic, const char* payload, bool retain);
bool mqttEnqueuePublish(const char* topic, const uint8_t* payload, size_t length, bool retain);
bool mqttEnqueueSubscribe(const char* topic);
bool mqttEnqueueUnsubscribe(const char* topic);

// Ersetzt .ino:127s direkten getMqttClient().disconnect()-Call. Setzt ein Flag,
// wartet kurz (<=500ms) blockierend bis der Worker es abgearbeitet hat -- der
// Aufrufer (Hotspot-Modus-Eintritt) ist selten/nicht zeitkritisch.
void disconnectMqtt();
```

`getMqttClient()` wird aus der öffentlichen API entfernt (nirgendwo mehr außerhalb
der Klasse gebraucht). `PubSubClient mqtt_client` bleibt privates Member.

Neue private Member:
```cpp
volatile bool mqtt_connected_flag = false;     // NUR vom Worker geschrieben
volatile bool mqtt_disconnect_requested = false;
volatile bool mqtt_ota_prep_requested = false;

void connectMqtt();                             // bleibt private, läuft NUR NOCH auf dem Worker
void drainOutboundQueue();                      // worker-only
void serviceBufferHousekeeping(uint32_t now_ms);// worker-only, siehe Schritt 4
```

### 2. Outbound-Queue (in `network_manager.cpp`, gleiches Muster wie die
bestehende Inbound-Queue in `mqtt_handlers.cpp`)

```cpp
enum class MqttCmdKind : uint8_t { PUBLISH, SUBSCRIBE, UNSUBSCRIBE };

struct MqttOutboundCmd {
  MqttCmdKind kind;
  bool retain;
  size_t payload_len;
  char* topic;       // zeigt in dieselbe Allokation
  uint8_t* payload;  // zeigt in dieselbe Allokation (leer bei SUBSCRIBE/UNSUBSCRIBE)
};

constexpr size_t kMqttOutboundQueueDepth = 64;  // reicht für einen vollen
                                                 // mqttReloadDynamicSlots()-Burst
                                                 // (~30 unsubscribe + ~30 subscribe)
```

Ein Allokations-Block pro Command: `[MqttOutboundCmd][topic\0][payload]`,
PSRAM bevorzugt (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, Fallback intern) — 1:1
das gleiche Muster wie `mqttAllocInbound()` in `mqtt_handlers.cpp`.

`enqueueOutboundCmd()`: bei vollem Queue/Alloc-Fehler NICHT blockieren, NICHT
inline verarbeiten (das würde `mqtt_client` vom falschen Task berühren) —
verwerfen + gedrosselt loggen (`[MQTT] Outbound-Queue voll -> verworfen (#N)`),
gleiches Muster wie der Inbound-Fallback in `mqttCallback()`.

`drainOutboundQueue()` (worker-only, in `serviceMqttWorker()` aufgerufen):
komplett leeren pro Worker-Iteration; `subscribe()`/`unsubscribe()` warten laut
PubSubClient-Quellcode NICHT auf ein Ack (reines Fire-and-Forget-Write, verifiziert),
also kein Yield-Hunger-Risiko wie beim Read-Pfad — trotzdem defensiv alle 8
gedrainten Commands ein `vTaskDelay(1)` einbauen.

### 3. Worker-Task (in der `.ino`, ersetzt den alten `mqtt_worker_task`)

```cpp
static TaskHandle_t g_mqtt_worker_handle = nullptr;

static void mqtt_worker_task(void* param) {
  (void)param;
  for (;;) {
    networkManager.serviceMqttWorker();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
```

In `setup()`, im `if (has_config)`-Block, nach `gameWSServer.init(8081)`:
```cpp
networkManager.beginMqttWorker();
const BaseType_t worker_core = (ARDUINO_RUNNING_CORE == 0) ? 1 : 0;
xTaskCreatePinnedToCoreWithCaps(mqtt_worker_task, "mqttWorker", 12288, nullptr,
                                tskIDLE_PRIORITY, &g_mqtt_worker_handle,
                                worker_core, MALLOC_CAP_SPIRAM);
```
(`#include <freertos/idf_additions.h>` für `xTaskCreatePinnedToCoreWithCaps`.)

In `loop()`: `networkManager.serviceMqttLoop()` entfällt (läuft jetzt nur noch im
Worker als `serviceMqttWorker()`). `mqtt_process_inbound_queue()` bleibt
unverändert im Loop-Task (Etappe 1 ist unangetastet korrekt).

`.ino:127` (Hotspot-Eintritt): `if (networkManager.isMqttConnected()) networkManager.disconnectMqtt();`

Fallback falls Worker-Start fehlschlägt: analog zum früheren Ansatz einen Hinweis
loggen; OHNE Worker gibt es dann gar kein `serviceMqttLoop()` mehr im Loop-Task
(anders als vorher) — das ist bewusst so, weil Single-Owner ohne Worker keinen
Sinn ergibt. Bei Fehlschlag lieber laut scheitern/loggen als still auf einen
unsicheren Fallback zurückzufallen.

### 4. `connectMqtt()` + `update()` + Puffer-Housekeeping — auf den Worker verlagert

**Wichtiger Korrektheits-Fund (unbedingt einbauen, sonst neuer Race):** Die
Topic-Strings (`bridge_apply_topic_`, `bridge_request_topic_`,
`history_request_topic_`, `history_response_topic_`, `weather_request_topic_`,
`energy_request_topic_`, `energy_response_topic_`, `bridge_icons_topic_`) werden
aktuell bei JEDEM `connectMqtt()`-Aufruf neu gebaut (`String`-Reassignment, mehrere
Heap-Operationen, NICHT atomar). Sobald `connectMqtt()` nur noch auf dem Worker
läuft, während der Loop-Task über `getBridgeApplyTopic()` etc. weiterliest, wäre
ein Reconnect mitten im Neuaufbau ein echter Wettlauf-Fehler (kaputter/freigegebener
Pointer). Diese Strings hängen aber NUR von der (laufzeit-konstanten) Efuse-MAC ab
und ändern sich nie zwischen Reconnects. **Fix: einmalig in `init()` bauen (läuft
vor Worker-Start), NIE in `connectMqtt()` neu bauen.**

`connectMqtt()` wird `private`, läuft NUR NOCH aus `serviceMqttWorker()` heraus.
Die paar Calls DIREKT darin (`mqtt_client.connect(...)`, die 4 initialen
`subscribe()`-Calls auf die Antwort-Topics, `mqtt.publish(stat_topic, "1", true)`)
dürfen weiterhin `mqtt_client` direkt anfassen (safe, weil nur vom Worker
aufgerufen) — MÜSSEN aber NICHT mehr `mqttSubscribeTopics()`/
`mqttPublishDiscovery()`/`mqttPublishDeviceSettings()`/`mqttPublishHomeSnapshot()`
direkt aufrufen, sondern über deren (jetzt enqueue-basierte, siehe Schritt 5)
öffentliche Version — das kostet nur einen Queue-Rundlauf (der Worker drained
seine eigene Queue in derselben oder nächsten Iteration).

Der komplette "MQTT verwalten"-Block aus `update()` (Retry-Timer, `connectMqtt()`-
Aufruf, `publishTelemetry()`, Puffer-Housekeeping mit `mqtt_large_until`/
`mqtt_connected_at`/`kMqttStormWindowMs`) wandert 1:1 in `serviceMqttWorker()`.
`update()` behält nur noch WiFi-Reconnect/WebAdmin/NTP (unverändert). Damit werden
`mqtt_retry_at`/`mqtt_connected_at`/`mqtt_large_until` faktisch worker-exklusiv
(kein Cross-Task-Zugriff mehr nötig für diese Felder).

`requestLargeMqttBuffer()`/`restoreMqttBufferNormal()` werden reine
Timestamp-Setter (nur `mqtt_large_until = ...`, KEIN direkter Client-Touch mehr
— sie werden vom Loop-Task aus `mqtt_handlers.cpp` heraus aufgerufen). Die
tatsächliche `setMqttBufferSize()`-Logik (Storm-Window/Grow/Shrink) läuft 1:1
aus dem bisherigen `update()`-Code portiert in `serviceBufferHousekeeping()`
innerhalb von `serviceMqttWorker()`.

`prepareMqttForOta()`: setzt `mqtt_ota_prep_requested = true`, wartet kurz
(≤500ms, `delay(5)`-Polling) bis der Worker es abgearbeitet hat (Worker prüft das
Flag am Anfang jeder Iteration: falls gesetzt, `disconnect()` + `setMqttBufferSize(OTA)`
direkt, Flag löschen, Iteration überspringen). Gleiches Muster für
`disconnectMqtt()` (nur Disconnect, kein Puffer-Resize) mit `mqtt_disconnect_requested`.

### 5. `mqtt_handlers.cpp` — alle ~15 Publish-Funktionen umstellen

Diese Funktionen laufen SOWOHL vom Worker (beim Connect-Handshake, indirekt über
`mqttSubscribeTopics()`/`mqttPublishDiscovery()` etc.) ALS AUCH vom Loop-Task
(Touch-Events, Settings-Änderungen, `processMqttMessage()` bei eingehenden
Nachrichten) — müssen also für BEIDE sicher sein. Lösung: sie rufen IMMER
`networkManager.mqttEnqueueXXX()` auf, NIE `mqtt_client`/`PubSubClient&` direkt.

Muster (Beispiel `mqttPublishSwitchCommand`):
```cpp
// vorher:
PubSubClient& mqtt = networkManager.getMqttClient();
if (!mqtt.connected()) { ...log...; return; }
...
bool ok = mqtt.publish(topic, payload, false);
Serial.printf(..., ok ? "ok" : "fail");

// nachher:
if (!networkManager.isMqttConnected()) { ...log...; return; }
...
bool queued = networkManager.mqttEnqueuePublish(topic, payload, false);
Serial.printf(..., queued ? "queued" : "queue-full");
```

Betroffene Funktionen (alle in `mqtt_handlers.cpp`, Zeilen aus dem `755ff50`-Stand):
`sync_external_temp_entity()` (Z.625), `mqttSubscribeTopics()` (Z.1474 — Schleife
über `kRoutes` + Aufruf `mqttReloadDynamicSlots()`), `mqttPublishHomeSnapshot()`
(Z.1488), `mqttPublishDeviceSettings()` (Z.1504), `mqttPublishScene()` (Z.1543),
`mqttPublishSwitchCommand()` (Z.1557), `mqttPublishMediaCommand()` (Z.1586),
`mqttPublishMediaVolume()` (Z.1608), `mqttPublishMediaMute()` (Z.1635),
`mqttPublishLightCommand()` (Z.1660), `mqttPublishHistoryRequest()` (Z.1721 —
Rückgabewert `ok` steuert `mark_pending_history_request()`, wird zu "queued"),
`mqttPublishWeatherRequest()` (Z.1782), `mqttPublishEnergyRequest()` (Z.1799 —
gibt `bool` an den Aufrufer zurück, wird zu "queued statt gesendet"),
`mqttPublishDiscovery()` (Z.1831 — ~10 `publish()`-Calls),
`mqttReloadDynamicSlots()` (Z.1892 — Schleifen über `g_dynamic_routes`/
`g_dynamic_weather_routes`, `unsubscribe()`/`subscribe()` je durch
`mqttEnqueueUnsubscribe()`/`mqttEnqueueSubscribe()` ersetzen; die
`lvglServiceDuringBlockingWork()`-Pumps zwischen den Einträgen bleiben, sind
aber jetzt weniger kritisch weil enqueue nicht mehr blockierend/netzwerk-I/O ist).

Analog in `network_manager.cpp`: `publishTelemetry()`, `publishBridgeConfig()`,
`publishBridgeRequest()` — `mqtt_client.connected()`-Check wird `isMqttConnected()`
(Flag-Read), `mqtt_client.publish(...)` wird `mqttEnqueuePublish(...)`.

**Include nicht vergessen:** `mqtt_handlers.cpp` braucht ggf. kein
`#include "src/network/vendor/pubsubclient/PubSubClient.h"` mehr, falls nach dem
Umbau nirgendwo mehr ein `PubSubClient&`-Typ im File auftaucht (bitte prüfen statt
raten).

### 6. Verifikation vor dem Testen (kein Compile-Tool verlässlich verfügbar)

- Grep-Sweep: `grep -rn "getMqttClient\|mqtt_client\." src/ ESP32_P4_HomeAssistant_Display.ino`
  darf danach NUR NOCH Treffer innerhalb von `network_manager.cpp` (privater
  Bereich: `connectMqtt()`, `serviceMqttWorker()`, `drainOutboundQueue()`,
  `serviceBufferHousekeeping()`) zeigen — nirgendwo sonst.
- Klammern-/Include-Balance manuell prüfen (kein zuverlässiges `arduino-cli`
  verfügbar — IDE-eigener Daemon blockiert parallele Compile-Läufe).
- Rückfallpunkt: `git reset --hard 755ff50` falls etwas nicht passt.

## Test-Fokus beim Flashen

1. `[Setup] MQTT-Worker auf Core 0 gestartet` (oder äquivalentes Log) erscheint.
2. `[Mem]`-Log nach Worker-Start: interner RAM ~unverändert (nur TCB-Overhead),
   PSRAM sinkt um die Worker-Stackgröße.
3. Kein Crash/Watchdog/Guru Meditation, auch über längere Laufzeit (letzter Crash
   brauchte ~30 Min zur Reproduktion — kurze Tests reichen nicht als Beweis).
4. MQTT verbindet, Sensoren aktualisieren sich, Bridge-Reload (großer Payload)
   funktioniert.
5. Touch-Events, die publishen (Licht/Switch/Szene/Media), kommen bei HA an
   (Queue-Rundlauf funktioniert).
6. Selten/nie `[MQTT] Outbound-Queue voll` im Log.
7. **Der eigentliche Zweck:** `[PixelAnim] tick gap` bleibt auch während eines
   Bridge-Reloads nahe am fps-Normalwert (kein Freeze mehr, auch nicht der
   ~150-200ms-Rest aus Runde 12).

## Wichtige Nebenregel (User-Feedback aus dieser Session)

Alles in EINEM Rutsch bauen, nicht Einzelschritt-für-Einzelschritt testen — ein
aussagekräftiger Test (der Crash brauchte ~30 Minuten zur Reproduktion) ist teuer,
mehrere Test-Runden für Teilschritte sind bei diesem Nutzer nicht praktikabel.
