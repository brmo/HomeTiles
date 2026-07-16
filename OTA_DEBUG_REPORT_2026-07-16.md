# OTA-/ESP-Hosted-Untersuchungsbericht

Stand: 16.07.2026
Projekt: HomeTiles
Betroffenes Gerät: Waveshare ESP32-P4-WIFI6-Touch-LCD-8
Lokale Testversion: `v0.5.4`
GitHub-Testziel: Dummy-Release `v0.5.5`
Git-HEAD beim Erstellen dieses Berichts: `aff965d6a934406c697d051edc743475cf6d844a`

## Kurzfassung

Das bestätigte Fehlerbild ist:

- Direkt nach einem Neustart funktioniert der GitHub-OTA-Download häufig vollständig.
- Nach längerer Laufzeit bricht derselbe Download ab.
- Der manuelle Firmware-Upload über das Web-Panel funktioniert zuverlässig.
- Beim GitHub-OTA scheitert nicht die Auswahl der OTA-Partition, sondern der
  HTTPS-/WLAN-/SDIO-Pfad.
- Relevante Fehler waren:
  - `connection lost`
  - `connect failed: release-assets.githubusercontent.com`
  - `esp-aes: Failed to allocate memory for len buffer`
  - `esp-aes: Failed to allocate memory for end alignment buffer`
  - `esp-aes: Generating input DMA descriptors failed`
  - früherer Panic:
    `assert failed: sdio_push_data_to_queue sdio_drv.c:964 (pkt_rxbuff)`
- PSRAM-Staging allein löste das Problem nicht.
- Der bisher praktisch funktionierende Zustand ist Claudes Workaround:
  Bei einem Transportfehler wird ein Diagnosebericht gespeichert, das Gerät
  sicher neu gestartet und der OTA-Versuch nach dem Neustart automatisch
  wiederholt.

Die eigentliche Langzeitursache ist noch nicht abschließend behoben.

## Wichtige Speicherbereiche

Die zwei OTA-Partitionen befinden sich im Flash. Sie werden abwechselnd als
aktive und inaktive Firmware-Partition verwendet.

Davon unabhängig werden für den Download weitere Speicherbereiche benötigt:

- PSRAM:
  - Firmware-Zwischenspeicher
  - Display-Puffer
  - MQTT-Puffer
- internes RAM/DMA-RAM:
  - ESP-Hosted/SDIO-Paketpuffer
  - WLAN-/TCP-Puffer
  - TLS/mbedTLS
  - Hardware-AES-DMA-Deskriptoren und Alignment-Puffer

Eine freie OTA-Partition bedeutet deshalb nicht automatisch, dass ein
GitHub-Download genug internen DMA-Speicher besitzt.

## Bestätigte Beobachtungen

### 1. Manueller Web-OTA funktioniert

Der lokale Upload über das Web-Panel installierte die komplette Firmware:

```text
[OTA] Update.begin OK, target size: 6169328
[OTA] Install progress: ...
[OTA] Upload finished: ... (6169328 bytes)
[OTA] Install finished successfully, restarting device...
```

Vorher wurden MQTT, Webserver und Display-Puffer kontrolliert vorbereitet:

```text
[OTA/Mem] after-ota-prep | Int free=147 KB | DMA free=108 KB |
DMA largest=67 KB | MQTT buf=1024 B
```

Dieser Weg benötigt keine lange GitHub-TLS-Verbindung. Das erklärt, warum er
trotz desselben Flash-/Partitionslayouts stabiler ist.

### 2. Ursprünglicher GitHub-OTA-Crash lag im SDIO-RX-Treiber

Core-Dump beziehungsweise Boot-Diagnose:

```text
Crashed task: sdio_rx_buf
assert failed: sdio_push_data_to_queue sdio_drv.c:964 (pkt_rxbuff)
```

Zugehöriger Core-Dump:

```text
C:\Users\seb_w\Downloads\waveshare_touch_lcd_8-coredump (5).bin
```

Der Crash entstand, als der ESP-Hosted-Treiber keinen Paketpuffer mehr aus
seinem Mempool erhielt.

### 3. Nach einem frischen Neustart kann derselbe OTA-Download funktionieren

In Claudes Tests schlug ein Download nach Laufzeit an wechselnden Stellen fehl,
beispielsweise nach ungefähr 2,6 MB oder 4,2 MB. Nach dem sicheren Neustart
lief derselbe 6.169.328-Byte-Download mit allen zwölf Ranges vollständig durch.

Das ist die wichtigste reproduzierbare Beobachtung:

```text
gealterter Laufzeitzustand -> Download bricht ab
frischer Neustart         -> Download funktioniert häufig
```

### 4. Mehr Drosselung verhinderte den Fehler nicht

Claude verwendete:

- 512-KB-Ranges
- 2 ms Pause pro 2-KB-Lesechunk
- 750 ms Pause zwischen vollständigen Ranges
- drei Versuche pro Range
- 3 Sekunden Pause nach einem Streamabriss

Trotzdem wanderte der Fehler nur an eine andere Stelle. Ein stärker gedrosselter
Download brach in einem Test sogar früher ab als ein weniger gedrosselter.

Schlussfolgerung: Reine Geschwindigkeitsdrosselung ist kein bestätigter Fix.

### 5. PSRAM-Vollstaging ohne gleichzeitiges Flash-Schreiben reichte nicht

Codex änderte den Ablauf so, dass erst die gesamte Firmware in PSRAM geladen,
danach TLS geschlossen und erst anschließend `Update.begin()`/`Update.write()`
ausgeführt wird.

Testlog:

```text
[Mem] before-github-ota | Int free=143 KB | Int largest=49 KB |
DMA free=104 KB | DMA largest=49 KB
[Update] Safe staged download: ... 512KB Ranges, 6024KB PSRAM
[Update] Lade Range 388-524675 (Versuch 1/3)
[Update] Lade Range 524676-1048963 (Versuch 1/3)
E (...) esp-aes: Failed to allocate memory for len buffer
E (...) esp-aes: Failed to allocate memory for end alignment buffer
E (...) esp-aes: Generating input DMA descriptors failed
```

Die erste 512-KB-Range funktionierte. Beim Beginn der folgenden Range konnte
Hardware-AES keinen internen DMA-Puffer mehr anlegen.

Damit ist die Annahme widerlegt, dass ausschließlich die Überlappung von TLS
und Flash-Schreiben den Fehler verursacht.

### 6. In-Place-Neustart von ESP-Hosted/WLAN war schädlich

Es wurde versucht, nach einem Streamabriss ESP-Hosted beziehungsweise WLAN ohne
P4-Neustart neu zu initialisieren und danach die Range fortzusetzen.

Ergebnis:

- wiederholte `connection lost`
- RPC-Timeouts
- MQTT konnte sich nicht mehr verbinden
- Bedienoberfläche wurde extrem langsam beziehungsweise unbedienbar

Dieser Ansatz wurde entfernt und darf nicht erneut verwendet werden, solange
der komplette Arduino-/TCP-/WiFi-Zustand nicht nachweislich sauber gemeinsam
deinitialisiert werden kann.

### 7. Direkter TLS-zu-Flash-Stream schlug ebenfalls fehl

Ein Versuch streamte GitHub-Daten direkt in `Update.write()`:

```text
[Update] Direct OTA stream: 4096B TCP RX, 2048B Lesechunk, 512KB Ranges
E (...) esp-aes: Failed to allocate memory for len descriptor
[Update] Stream bei 4501 / 6169328 Bytes abgerissen: connection lost
```

Obwohl vor dem Start ungefähr 149 KB internes RAM und 110 KB DMA-RAM frei
waren, scheiterte AES fast sofort.

Dieser Ansatz ist verworfen.

### 8. Zusätzliche dauerhafte SDIO-Puffer waren ein Fehler

Codex testete eine zusätzliche Reserve von 31 SDIO-Paketpuffern.

Größenordnung:

```text
31 * ca. 1600 Byte (inklusive Alignment) = ca. 49.600 Byte internes DMA-RAM
```

Die Reserve galt ab dem Boot für den gesamten Betrieb, nicht nur während OTA.
Dadurch fehlte das RAM der normalen Bedienung, WLAN, TLS und anderen
DMA-Verbrauchern.

Folgen im Test:

- `DMA largest nur 0 KB`
- Lichter-/MQTT-Aktionen eskalierten sofort
- Gerät wurde praktisch unbedienbar

Die zusätzliche Reserve wurde vollständig verworfen und aus Repository sowie
lokalem Arduino-Core entfernt.

## Separater gefundener MQTT-Fehler

Ein Log enthielt 376 direkte Wechsel:

```text
[MQTT] Buffer: 24576 -> 32768 bytes (queued-publish, PSRAM)
[MQTT] Buffer: 32768 -> 24576 bytes (media-config, PSRAM)
```

Ursache:

- Ein großes Publish vergrößerte den PSRAM-MQTT-Puffer.
- Bei zu kleinem `DMA largest` wurde das Publish zurück in die Queue gelegt.
- `mqtt_large_until` wurde erst nach der DMA-Prüfung gesetzt.
- Deshalb verkleinerte das Housekeeping den Puffer sofort wieder.
- Der nächste Worker-Durchlauf begann erneut.

Codex hatte die Reihenfolge korrigiert. Auf ausdrücklichen Wunsch wurde jedoch
der vollständige Claude-Zustand wiederhergestellt. Diese MQTT-Korrektur ist
deshalb im aktuellen Stand nicht enthalten.

Der MQTT-Loop ist nicht als ursprüngliche OTA-Ursache bestätigt, kann einen
bereits schlechten DMA-Zustand aber stark verschlimmern.

## ESP-Hosted-Mempool: neuer, noch unbestätigter Ursachenhinweis

Der zum Arduino-Core 3.3.7 gehörende ESP-Hosted-Mempool wurde im Quellcode
untersucht.

Beobachtetes Verhalten:

- Wenn die freie Mempool-Liste leer ist, legt `mempool_alloc()` einen neuen
  aligned Block im internen Speicher an.
- `mempool_free()` gibt einen nicht mehr verwendeten Block nicht an den
  System-Heap zurück.
- Der Block wird stattdessen dauerhaft in die freie Mempool-Liste eingehängt.
- Erst `mempool_destroy()` gibt diese gecachten Blöcke an den Heap zurück.
- Im normalen Betrieb wird der Mempool nicht zerstört; ein kompletter Neustart
  räumt ihn dagegen auf.

Das passt mechanistisch zum beobachteten Muster:

```text
lange Laufzeit / RX-Spitzen -> hoher gecachter Mempool-Höchststand
Neustart                    -> Cache vollständig weg
```

Es ist noch nicht durch Laufzeit-Zähler bewiesen, dass genau dieser Cache den
AES-Fehler verursacht. Bevor sein Verhalten geändert wird, sollten deshalb
Mempool-Statistiken instrumentiert werden.

## Aktuell wiederhergestellter Claude-Zustand

Der aktuelle Arbeitsstand entspricht wieder Claudes letztem Zustand.

### GitHub-OTA

- Firmware wird in 512-KB-Ranges vollständig in PSRAM geladen.
- Lesepause: 2 ms pro 2-KB-Chunk.
- Pause zwischen Ranges: 750 ms.
- Drei Versuche pro Range.
- Nach einem Abriss 3 Sekunden Pause vor dem nächsten Versuch.
- Fehlerdetails und Speicherwerte werden nach `/crashlog.txt` geschrieben.
- Bei einem retry-fähigen Transportfehler:
  - Tag und Versuchszähler werden im NVS-Namespace `otaretry` gespeichert.
  - Gerät wird sicher neu gestartet.
  - Nach 30 Sekunden und bestehender WLAN-Verbindung wird der Install
    automatisch erneut gestartet.
  - Maximal drei automatische Wiederholungen.

### Aktuelle Treiberobjekte

Die lokalen Arduino-Core-Archive und das Repository verwenden wieder Claudes
damalige ESP-Hosted-Objekte:

```text
esp32p4-libs/sdio_drv.c.obj
SHA-256 3F75A936F9581585D954C1A22AFD2724DE76832F1245A1F4F13A36501C8D9CD7

esp32p4_es-libs/sdio_drv.c.obj
SHA-256 5C44563AEDCF633470DE5BA1BDBEE0CAA1C7E6B54655A26A794B728562469DAF
```

### Aktuell veränderte Projektdateien

```text
HomeTiles.ino
src/core/github_update.cpp
src/core/github_update.h
src/core/crash_log.cpp
src/core/crash_log.h
version.txt
```

`version.txt` meldet weiterhin `v0.5.4`.

Die Dateien `docs/images/8in-home-new.png` und `forum-assets/` gehören nicht
zur OTA-Untersuchung und wurden nicht verändert.

Nach der Wiederherstellung wurde kein Firmware-Build durch Codex gestartet.

## Nicht erneut versuchen

Folgende Ansätze waren erfolglos oder machten das Gerät schlechter:

1. Zusätzliche 31 SDIO-Puffer dauerhaft reservieren.
2. ESP-Hosted/WLAN im laufenden Arduino-Netzwerkstack deinitialisieren und
   wieder initialisieren.
3. GitHub-TLS direkt mit `Update.write()`/Flash-Schreiben überlappen.
4. Nur die Range-Pausen beziehungsweise Download-Drosselung erhöhen und dies
   als Ursachen-Fix betrachten.
5. Nur auf `Heap free` oder `Int free` schauen. Relevant sind zusätzlich:
   - `DMA free`
   - `DMA largest`
   - AES-Allokationsfehler
   - SDIO-Mempool-Belegung
6. Eine vorhandene alte Test-`.bin` verwenden, ohne zu wissen, welche
   Treiberobjekte darin gelinkt wurden.

## Empfohlene nächste Schritte

### Schritt 1: Nur messen, noch nichts am Speicherverhalten ändern

Im ESP-Hosted-Mempool sollten folgende Werte rate-limited protokolliert werden:

- aktuell ausgeliehene Blöcke
- freie/gecachte Blöcke
- bisheriger Höchststand
- Anzahl frischer `_h_malloc_align`-Allokationen
- Anzahl wiederverwendeter Blöcke

Zusätzlich nach jeder vollständig geladenen 512-KB-Range:

```text
Int free
Int largest
DMA free
DMA largest
Mempool active
Mempool cached
```

Damit lässt sich prüfen, ob der erste erfolgreiche Downloadblock tatsächlich
DMA-RAM im Mempool bindet, das vor dem nächsten TLS-/AES-Vorgang nicht mehr
zurückgegeben wird.

### Schritt 2: Erst bei bestätigtem Mempool-Wachstum einen begrenzten Cache testen

Möglicher späterer Fix:

- Nur eine kleine Zahl freier SDIO-Blöcke im Cache behalten.
- Überschüssige, bereits zurückgegebene Blöcke mit `_h_free_align()` wieder an
  den internen Heap freigeben.
- Keine aktiven beziehungsweise in Queues liegenden Blöcke anfassen.
- Keine pauschale Startreservierung.

Dieser Fix wurde noch nicht implementiert oder auf dem Gerät getestet.

### Schritt 3: Vergleichstest mit dem 4B

Mit identischer Firmwaredatei beziehungsweise identischem OTA-Ablauf sollten
auf 8-Zoll und 4B dieselben Mempool-/DMA-Zähler erfasst werden.

Ziel:

- Prüfen, ob nur das 8-Zoll-Gerät einen wachsenden SDIO-Cache beziehungsweise
  einen stärkeren DMA-Verlust zeigt.
- Keine Hardwareursache behaupten, bevor diese Messwerte vorliegen.

## Relevante lokale Logs

```text
C:\Users\seb_w\.codex\attachments\3c0ed2d3-ba44-47b4-9a12-4f458658fe22\pasted-text.txt
C:\Users\seb_w\.codex\attachments\616569ab-eba5-46f0-bcc3-6924a524c946\pasted-text.txt
C:\Users\seb_w\.codex\attachments\8e4f2129-2b2f-4311-977f-1803fb15f168\pasted-text.txt
C:\Users\seb_w\.codex\attachments\c0966e10-0e68-415e-bf10-e1546cf00912\pasted-text.txt
C:\Users\seb_w\.codex\attachments\51df41b6-10d2-4ac5-96eb-5648224a503c\pasted-text.txt
```

Claudes vollständiges Sitzungsprotokoll mit den wiederhergestellten
Auto-Retry-Änderungen:

```text
C:\Users\seb_w\.claude\projects\C--Users-seb-w-Desktop-Projekte-HomeTiles\01f8f21b-bdc5-4fa0-b20f-7e8db8fb97b3.jsonl
```
