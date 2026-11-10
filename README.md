# IceTrack — ESP32 Refrigeration Monitoring Node

Firmware for the IceTrack IoT device: an **ESP32 DevKit V1** that samples a
DHT11 temperature/humidity probe, an LDR light sensor and a PIR presence sensor,
sends telemetry every 10 seconds to the **on-site IceTrack Edge gateway over the
LAN**, and signals alarm, connectivity and maintenance states locally with an
RGB LED and a buzzer.

The application is built on the **Modest IoT Nano-framework (C++ Edition)** by
Angel Velasquez and follows its event-driven, CQRS-inspired flow:

```
Sensor --Event--> IceTrackDevice --Command--> Actuator
```

The ESP32 never accepts inbound connections and does not need the public
internet. Only the local gateway is contacted; see
[NFR-04 / LAN-only operation](#lan-only-operation).

---

## Table of contents

1. [What the firmware does](#what-the-firmware-does)
2. [Architecture](#architecture)
3. [Hardware and wiring](#hardware-and-wiring)
4. [Project structure](#project-structure)
5. [Provisioning](#provisioning)
6. [Build and flash](#build-and-flash)
7. [Simulation with Wokwi](#simulation-with-wokwi)
8. [Local mock gateway](#local-mock-gateway)
9. [Gateway contract](#gateway-contract)
10. [Runtime behaviour](#runtime-behaviour)
11. [Story traceability](#story-traceability)
12. [LAN-only operation](#lan-only-operation)
13. [Troubleshooting](#troubleshooting)
14. [Known limitations](#known-limitations)
15. [License and credits](#license-and-credits)

---

## What the firmware does

| Capability | Behaviour |
| --- | --- |
| Sampling | Reads DHT11, LDR and PIR every **10 s** (`READING_INTERVAL_MS`). |
| Telemetry | Sends one JSON reading to `POST /edge/api/v1/readings` on the LAN gateway. |
| Buffering | Keeps up to **360 readings** in a RAM FIFO (about 1 hour at 10 s). Oldest reading is discarded when full. |
| Retry | Exponential backoff from 1 s to 60 s with jitter; drains the backlog oldest-first after recovery. |
| Connectivity LED | Solid amber after **3 consecutive failed transmissions**. |
| Thermal alarm | The edge returns `alarmActive`; the device blinks the red LED and sounds the buzzer. |
| Buzzer mute | The edge returns `buzzerMuted`; silence lasts until the excursion ends. |
| Maintenance button | A **3-second long press** sends a button event immediately, without waiting for the next 10 s cycle. |
| Maintenance LED | Solid blue while a maintenance window is active, from any origin. |
| Feedback | Two beeps on activation, one on deactivation, three short warning beeps on rejection or failure. |
| Signal test | The edge can request a 3-second LED diagnostic sequence plus a double beep. |
| Sensor fault flag | Invalid DHT11 samples are discarded, the last valid value is kept, and the reading is marked `sensorValid=false` with `sensorFailures`. |
| Idempotency | Every reading and button event carries a unique `readingUid` / `eventUid`. |

---

## Architecture

```
                 +-----------------------------+
                 |          Sensors            |
                 |  DHT11   LDR   PIR   Button |
                 +-------------+---------------+
                               | Event
                               v
+--------------------------------------------------------------+
|                       IceTrackDevice                         |
|  event routing | 10 s scheduler | indicator state machine    |
|  maintenance feedback | JSON-originated commands             |
+-------+-------------------+-----------------------+----------+
        | Command           | Telemetry             | HTTP
        v                   v                       v
+---------------+   +------------------+   +--------------------+
| StatusLed     |   | TelemetryBuffer  |   | GatewayClient      |
| Buzzer        |   | (RAM FIFO, 360)  |   | Wi-Fi + HTTP POST  |
+---------------+   +------------------+   +---------+----------+
                                                    | LAN only
                                                    v
                                         +------------------------+
                                         | IceTrack Edge gateway  |
                                         | /edge/api/v1/...       |
                                         +------------------------+
```

The class diagram for the firmware is in
[`embedded-class-diagram.puml`](embedded-class-diagram.puml). The original
framework example diagram ([`class-diagram.puml`](class-diagram.puml)) is kept
for reference.

### Framework roles

| Framework type | IceTrack implementation | Responsibility |
| --- | --- | --- |
| `Device` | `IceTrackDevice` | Orchestrates the node, routes events, schedules sampling and transmission. |
| `Sensor` | `Dht11Sensor`, `LightSensor`, `PirSensor` | Produce events and expose their last measurement. |
| `Button` | `MaintenanceButton` | Debounces the physical button and emits a long-press event. |
| `Actuator` | `StatusLed`, `Buzzer` | Consume commands and drive non-blocking output patterns. |
| `Event` / `Command` | framework constants + class-specific IDs | Decouple sensor input from actuator output. |

---

## Hardware and wiring

### Bill of materials

| Qty | Component | Notes |
| ---: | --- | --- |
| 1 | ESP32 DevKit V1 (ESP32-WROOM-32) | 3.3 V logic, 2.4 GHz Wi-Fi. |
| 1 | DHT11 temperature/humidity sensor | 0–50 °C, 20–90 % RH; module or bare sensor with 10 kΩ pull-up. |
| 1 | LDR / photoresistor module | Analog output; bright = lower voltage on this module. |
| 1 | PIR motion sensor (HC-SR501 or similar) | Digital output; 5 V supply recommended. |
| 1 | Momentary push button | Maintenance button, active low. |
| 1 | Common-anode RGB LED | 5 mm, 4 pins. |
| 3 | 220 Ω resistors | One per RGB channel. |
| 1 | Passive piezo buzzer | The firmware drives it with `tone()` at 2 kHz; an active buzzer is not recommended. |
| 1 | 10 kΩ resistor | Only if the DHT11 is a bare sensor without a module pull-up. |

### Pin map (defaults in `AppConfig.h`)

| Component | Module pin | ESP32 pin | GPIO | Notes |
| --- | --- | --- | ---: | --- |
| DHT11 | VCC / + | `3V3` | — | 3.3 V is fine for DHT11. |
| DHT11 | DATA / SDA / OUT | `D15` | 15 | 10 kΩ pull-up to 3V3 if not on the module. |
| DHT11 | GND / − | `GND` | — | Common ground. |
| LDR module | VCC | `3V3` | — | |
| LDR module | GND | `GND` | — | |
| LDR module | AO (analog out) | `D34` | 34 | Input-only pin, ADC1; do not use `DO`. |
| PIR | VCC | `VIN` | — | `VIN` is 5 V when USB-powered; PIR output is 3.3 V compatible. |
| PIR | GND | `GND` | — | |
| PIR | OUT | `D33` | 33 | Digital input. |
| Button | terminal 1 | `D32` | 32 | Internal pull-up; active low. |
| Button | terminal 2 | `GND` | — | |
| RGB LED | COM (anode) | `3V3` | — | Common anode; code drives channels active low. |
| RGB LED | R | via 220 Ω → `D25` | 25 | |
| RGB LED | G | via 220 Ω → `D26` | 26 | |
| RGB LED | B | via 220 Ω → `D27` | 27 | |
| Buzzer | negative (1) | `GND` | — | |
| Buzzer | positive (2) | `D14` | 14 | `tone()` output. |

> **RGB LED polarity.** `AppConfig::RGB_COMMON_ANODE = true` by default, which
> matches the Wokwi `wokwi-rgb-led` part and the common-anode wiring above. If
> you use a common-cathode LED, connect COM to GND and set the flag to `false`.

> **GPIO34 is input-only.** It has no internal pull-up/down. That is fine for
> the LDR module's driven analog output.

---

## Project structure

| Path | Purpose |
| --- | --- |
| `sketch.ino` | Arduino entry point (`setup()` / `loop()`). |
| `AppConfig.h` | Wi-Fi, gateway, device ID, GPIO map, timing and limits. |
| `IceTrackDevice.h/.cpp` | Main `Device`: scheduling, event/command routing, state machine. |
| `Dht11Sensor.h/.cpp` | DHT11 adapter with range validation and last-good fallback. |
| `LightSensor.h/.cpp` | LDR adapter: 12-bit ADC, inverted 0–100 % mapping, smoothing. |
| `PirSensor.h/.cpp` | PIR adapter with a 2 s hold window; boolean only. |
| `MaintenanceButton.h/.cpp` | 50 ms debounce and 3 s long-press detection. |
| `StatusLed.h/.cpp` | Non-blocking RGB state machine (green/red/amber/blue/test). |
| `Buzzer.h/.cpp` | Non-blocking alarm and 1/2/3-beep patterns. |
| `GatewayClient.h/.cpp` | Wi-Fi supervision, LAN HTTP POST, JSON parsing, mock mode. |
| `Telemetry.h` | `TelemetryReading` payload struct. |
| `TelemetryBuffer.h/.cpp` | Fixed-size RAM ring buffer. |
| `ModestIoT.h`, `Device.*`, `Sensor.*`, `Actuator.*`, `Button.*`, `Led.*`, `EventHandler.h`, `CommandHandler.h` | Modest IoT framework files. |
| `diagram.json`, `libraries.txt` | Wokwi simulation layout and libraries. |
| `tools/mock_gateway.py` | Dependency-free Edge API simulator for bench tests. |
| `tools/build.ps1` | Stages and compiles/uploads the sketch with `arduino-cli`. |
| `product-backlog.md` | Requirements source. |
| `ToggableLedDevice.h/.cpp` | Original framework example, kept for reference and not referenced by the IceTrack entry point. |

---

## Provisioning

Everything site-specific lives in [`AppConfig.h`](AppConfig.h).

| Setting | Default | Change for a real site |
| --- | --- | --- |
| `WIFI_SSID` | `"Wokwi-GUEST"` | The site's **local** Wi-Fi SSID (2.4 GHz). |
| `WIFI_PASSWORD` | `""` | The local Wi-Fi password. |
| `GATEWAY_BASE_URL` | `"http://192.168.1.50:8080"` | Static LAN address and port of the edge gateway. |
| `DEVICE_ID` | `"ice-esp32-001"` | **Unique** per physical node. |
| `DEVICE_API_KEY` | `""` | Optional `X-Device-Key`; use a dedicated, revocable key in production. |
| `FIRMWARE_VERSION` | `"1.0.0"` | Bump when deploying. |
| `USE_MOCK_GATEWAY` | `false` | Leave `false` on hardware. Auto-enabled when the SSID is `Wokwi-GUEST`. |

Notes:

- Use a **static DHCP reservation** (or a fixed IP) for the edge gateway so the
  URL stays valid.
- The gateway URL must be reachable from the site LAN. Do not configure a public
  hostname or port-forward the ESP32.
- `DEVICE_ID` becomes part of every `readingUid` and `eventUid`. Two nodes with
  the same ID would collide.
- Wi-Fi credentials are compile-time constants in this reference firmware. For
  production, move them to NVS or a provisioning flow.

### Timing and tuning

| Constant | Default | Meaning |
| --- | ---: | --- |
| `READING_INTERVAL_MS` | 10000 | Telemetry period. |
| `BUTTON_LONG_PRESS_MS` | 3000 | Maintenance long press. |
| `BUTTON_DEBOUNCE_MS` | 50 | Contact debounce. |
| `PIR_HOLD_MS` | 2000 | Presence hold after the PIR output falls. |
| `HTTP_TIMEOUT_MS` | 4000 | Per-request timeout. |
| `WIFI_RETRY_INTERVAL_MS` | 15000 | Wi-Fi reconnect attempt period. |
| `OFFLINE_FAILURE_THRESHOLD` | 3 | Failures before the amber LED. |
| `MAX_BACKOFF_MS` | 60000 | Backoff cap. |
| `BACKLOG_DRAIN_GAP_MS` | 300 | Pause between backlog transmissions. |
| `TELEMETRY_BUFFER_CAPACITY` | 360 | RAM FIFO size in readings. |
| `MIN/MAX_VALID_TEMPERATURE_C` | 0 / 50 | DHT11 plausible range. |
| `MIN/MAX_VALID_HUMIDITY_PCT` | 20 / 90 | DHT11 plausible range. |
| `MOCK_ALARM_TEMPERATURE_C` | 30 | Alarm threshold used **only** by the built-in firmware mock. |

---

## Build and flash

### Requirements

1. **Arduino IDE 2.x** or **arduino-cli**.
2. **ESP32 board support** (`esp32:esp32`; developed against core 3.3.12).
3. Libraries:
   - **DHT sensor library for ESPx** (`DHTesp`, tested 1.19)
   - **ArduinoJson** 7.x (tested 7.4.3)
4. Board selection: **ESP32 DevKit V1** / FQBN `esp32:esp32:esp32`.

### Recommended: build script

Arduino requires the main `.ino` file to match its containing folder name. This
repository keeps `sketch.ino` so it opens directly in Wokwi, so
[`tools/build.ps1`](tools/build.ps1) stages the sources into a temporary,
correctly named sketch folder and builds there.

```powershell
# Compile only
.\tools\build.ps1

# Compile and upload
.\tools\build.ps1 -Upload -Port COM5

# Different board/port or arduino-cli location
.\tools\build.ps1 -Fqbn esp32:esp32:esp32 -Port COM5 -Upload
.\tools\build.ps1 -ArduinoCli "C:\path\to\arduino-cli.exe"
```

The script does not modify the repository. It writes the staged sketch and build
output under `%TEMP%\icetrack-build`.

If PowerShell blocks local scripts, run it explicitly:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build.ps1
```

### Arduino IDE

Because the folder is named `ice-track-embedded-app` and the entry file is
`sketch.ino`, open a **copy** of the project in a folder named `sketch`, or
rename the copy's `sketch.ino` to match its folder name. The Wokwi-compatible
original stays untouched.

### Expected footprint

With the ESP32 Arduino core 3.3.12 and the default partition table:

```
Sketch uses 972802 bytes (74%) of program storage space.
Global variables use 98304 bytes (30%) of dynamic memory.
```

The RAM figure includes the 512-entry backing array of `TelemetryBuffer`
(configured capacity 360 readings). If flash or RAM becomes tight, reduce
`TELEMETRY_BUFFER_CAPACITY` or lower the buffer's `MAX_CAPACITY`.

---

## Simulation with Wokwi

The repository is a self-contained Wokwi project:

- [`diagram.json`](diagram.json) — ESP32 DevKit V1, DHT22, photoresistor module,
  PIR, push button, common-anode RGB LED with 220 Ω resistors, and buzzer.
- [`libraries.txt`](libraries.txt) — `DHT sensor library for ESPx` and
  `ArduinoJson`.
- `sketch.ino` — firmware entry point.

Import `diagram.json` at <https://wokwi.com>, or open the folder with the Wokwi
VS Code extension if you have it configured.

### Why a DHT22 in the simulation?

Wokwi provides a `wokwi-dht22` part, not a DHT11. `Dht11Sensor` uses
`DHTesp::AUTO_DETECT`, so the same firmware works with the simulated DHT22 and a
physical DHT11. The application still validates samples against the DHT11
envelope (0–50 °C, 20–90 % RH), as required by the backlog.

### Built-in mock mode

When `WIFI_SSID` is exactly `Wokwi-GUEST`, `GatewayClient` enables its internal
mock gateway:

- no Wi-Fi or HTTP traffic is generated;
- every reading is accepted;
- `alarmActive` becomes true at **30 °C** (`MOCK_ALARM_TEMPERATURE_C`);
- a long press toggles the simulated maintenance window and returns
  `ACTIVATED` / `DEACTIVATED`.

`USE_MOCK_GATEWAY = true` forces the same behaviour on a bench without a
gateway. Set it back to `false` for real deployments.

The internal mock always accepts readings, so the **solid amber** connection-loss
state, the daily activation quota, buzzer muting and signal-test commands cannot
be demonstrated there. Use the Python gateway on a real LAN for those cases.

### Things to try

| Action in Wokwi | Expected result |
| --- | --- |
| Run the simulation | Serial banner at 115200 baud; solid green LED. |
| Click the DHT22 and raise the temperature above 30 °C | Blinking red LED + continuous 2 kHz buzzer. |
| Lower the temperature below 30 °C | Solid green; buzzer stops. |
| Click the PIR and choose **Simulate Motion** | `[PIR] Presence detected`; presence appears in the next reading. |
| Change the photoresistor `lux` (10 = dark/closed, ≥ 500 = bright/open) | `lightLevel` rises with brightness in the serial log. |
| Hold the button (key **B**) for 3 seconds | Two beeps; blue LED; mock maintenance active. |
| Hold again for 3 seconds | One beep; back to green. |

> Wokwi's button has a keyboard shortcut **B** and a `pressed` automation
> control. Ctrl-click (Cmd-click on macOS) makes it stay pressed.

---

## Local mock gateway

[`tools/mock_gateway.py`](tools/mock_gateway.py) is a dependency-free Python 3
HTTP server that emulates the Edge API subset used by the firmware. It is useful
for bench tests, classroom demos and for exercising the real HTTP/backoff path
without the actual IceTrack gateway.

```powershell
python tools\mock_gateway.py --port 8080 --threshold 8.0
```

Then set:

```cpp
constexpr char WIFI_SSID[] = "your-site-lan";
constexpr char WIFI_PASSWORD[] = "your-password";
constexpr char GATEWAY_BASE_URL[] = "http://192.168.1.50:8080";
constexpr bool USE_MOCK_GATEWAY = false;
```

The server binds `0.0.0.0` so the ESP32 can reach it. Open
`http://127.0.0.1:8080/` for a small dashboard, or `GET /status` for JSON.

### Options

| Option | Default | Meaning |
| --- | ---: | --- |
| `--host` | `0.0.0.0` | Bind address. |
| `--port` | `8080` | TCP port; must match `GATEWAY_BASE_URL`. |
| `--threshold` | `8.0` | Temperature alarm threshold in °C. |
| `--maintenance-minutes` | `30` | Duration of a BUTTON-origin window. |
| `--daily-limit` | `3` | BUTTON activations per rolling 24 h. |
| `--device-key` | disabled | If set, every POST must carry `X-Device-Key`. |
| `--quiet` | off | Only warnings/errors. |

### Bench controls

These endpoints are simulator conveniences; they mirror the technician commands
described by TS-27:

```powershell
# Mute the buzzer (latched until the excursion ends)
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8080/edge/api/v1/mute `
  -ContentType 'application/json' -Body '{"muted":true}'

# Request a one-shot LED/buzzer signal test
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8080/edge/api/v1/signal-test `
  -ContentType 'application/json' -Body '{"active":true}'

# Open or close an APP-origin maintenance window (120 minutes)
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8080/edge/api/v1/maintenance `
  -ContentType 'application/json' -Body '{"active":true}'
```

The simulator enforces the TS-09/TS-27 rules that the firmware delegates to the
edge:

- one shared maintenance window per device;
- BUTTON origin with a 30-minute expiry (configurable);
- APP origin with a 120-minute expiry;
- maximum 3 BUTTON activations per rolling 24 hours;
- automatic expiry;
- buzzer mute latched while `alarmActive` is true and reset when the excursion
  ends;
- one-shot `signalTest`;
- validation of `deviceId`, temperature (0–50 °C), humidity (20–90 %),
  light level (0–100) and boolean presence.

> The mock gateway stores everything in memory and uses plain HTTP. Do not
> expose it outside a trusted lab LAN. It is not a replacement for the real
> SQLite-backed edge service.

---

## Gateway contract

All requests are JSON over HTTP to the LAN gateway. Any **2xx** response is
treated as success; the body is parsed for directives.

### `POST /edge/api/v1/readings`

Request (one reading every 10 seconds):

```json
{
  "deviceId": "ice-esp32-001",
  "firmwareVersion": "1.0.0",
  "readingUid": "ice-esp32-001-1a2b3c4d-42",
  "temperature": 5.5,
  "humidity": 55.0,
  "lightLevel": 12,
  "presence": false,
  "presenceAvailable": true,
  "sensorValid": true,
  "sensorFailures": 0,
  "sequence": 42,
  "uptimeMs": 420000,
  "capturedAt": 0
}
```

| Field | Type | Meaning |
| --- | --- | --- |
| `deviceId` | string | Provisioned device identifier. |
| `firmwareVersion` | string | Firmware version for support. |
| `readingUid` | string | Idempotency key: `<device>-<bootId>-<sequence>`. |
| `temperature` | number | Last valid **raw** DHT11 temperature in °C. The edge applies the calibration offset (US-56). |
| `humidity` | number | Last valid relative humidity in %. |
| `lightLevel` | integer | 0–100 %; 100 = bright (open door), 0 = dark (closed). |
| `presence` | boolean | PIR presence; never identifies a person. |
| `presenceAvailable` | boolean | False when the PIR value cannot be interpreted. |
| `sensorValid` | boolean | False when the DHT11 sample was discarded. |
| `sensorFailures` | integer | Consecutive invalid DHT11 samples. |
| `sequence` | integer | Monotonic reading number since boot. |
| `uptimeMs` | integer | Capture time relative to boot. |
| `capturedAt` | integer | Optional UTC epoch seconds; `0` when SNTP is disabled/unsynchronized. The gateway uses its receipt timestamp. |

Response example:

```json
{
  "alarmActive": false,
  "buzzerMuted": false,
  "signalTest": false,
  "maintenanceActive": false,
  "maintenanceMode": {
    "active": false,
    "origin": null,
    "expiresAt": null
  }
}
```

| Field | Type | Device reaction |
| --- | --- | --- |
| `alarmActive` | boolean | Blinking red LED; continuous buzzer unless muted. |
| `buzzerMuted` | boolean | Silences the alarm. The LED keeps blinking. The device clears the mute when `alarmActive` becomes false. |
| `signalTest` | boolean | One-shot 3-second LED sequence + double beep. |
| `maintenanceMode.active` / `maintenanceActive` | boolean | Solid blue LED; buzzer suppressed. |
| `maintenanceMode.origin` | string | `BUTTON`, `APP` or another edge-defined origin (kept for diagnostics). |
| `maintenanceMode.expiresAt` | string/integer | Expiry as returned by the edge (kept for diagnostics). |

The firmware accepts either the nested `maintenanceMode` object or the
top-level `maintenanceActive` boolean.

### `POST /edge/api/v1/button-events`

Sent immediately on a 3-second long press:

```json
{
  "deviceId": "ice-esp32-001",
  "pressType": "LONG",
  "timestamp": 0,
  "capturedAt": 0,
  "uptimeMs": 540000,
  "eventUid": "ice-esp32-001-1a2b3c4d-btn-1"
}
```

`timestamp` is `capturedAt` when a clock is available, otherwise `uptimeMs`.

Response example:

```json
{
  "feedback": "ACTIVATED",
  "maintenanceActive": true,
  "maintenanceMode": {
    "active": true,
    "origin": "BUTTON",
    "expiresAt": "2026-11-09T12:30:00Z"
  }
}
```

`feedback` is one of:

| Value | Buzzer feedback | LED |
| --- | --- | --- |
| `ACTIVATED` | Two beeps | Blue (maintenance) |
| `DEACTIVATED` | One beep | True operational state |
| `REJECTED_DAILY_LIMIT` | Three short warning beeps | True operational state |
| missing/unknown | Three short warning beeps | True operational state |

If the POST fails or the device is offline, no state changes and the device
emits three warning beeps. The edge owns the daily quota and the window
decision; the device only reflects the result.

### HTTP status handling

| Status | Behaviour |
| --- | --- |
| `200`–`299` | Success; parse directives. |
| `400`, `413`, `422` | Payload rejected; drop it and continue with the next reading (avoids blocking the FIFO forever). |
| `401`, `403`, `404` | Configuration/registration problem; retry with backoff. |
| `5xx`, timeout, `-1` | Transient failure; retry with backoff. |

---

## Runtime behaviour

### Telemetry pipeline

1. `loop()` samples sensors every 10 s and appends a `TelemetryReading` to the
   RAM FIFO.
2. If the FIFO is not empty and the backoff timer has elapsed, the oldest
   reading is sent.
3. On success: pop it, reset failures/backoff, apply directives, and either send
   the next backlog item after 300 ms or wait for the next sample.
4. On failure: keep the reading, increment the failure counter, and schedule a
   retry.
5. After the Wi-Fi link returns, the backlog drains oldest-first while new
   samples keep being appended.

Backoff sequence (with up to 250 ms jitter):

```
1 s → 2 s → 4 s → 8 s → 16 s → 32 s → 60 s (cap)
```

After the third consecutive failure the LED turns solid amber. A successful
transmission resets the counter and returns the LED to its true state.

### Indicator priority

| Priority | State | LED | Buzzer |
| ---: | --- | --- | --- |
| 1 | Gateway unreachable (≥ 3 failures) | Solid amber | Off |
| 2 | Maintenance window active | Solid blue | Off |
| 3 | `alarmActive` | Blinking red (500 ms) | 2 kHz continuous unless muted |
| 4 | Normal | Solid green | Off |
| — | Signal test | 3 s R → G → B → amber → white | Double beep, then previous state |

> When the node is offline it cannot receive new directives, so the last known
> maintenance/alarm state is retained in memory and reapplied when the link
> recovers. The amber connection-loss display takes precedence meanwhile.

### DHT11 validation

A sample is accepted only when:

- the driver status is `ERROR_NONE`;
- both values are finite;
- temperature is within `0.0 … 50.0 °C`;
- humidity is within `20.0 … 90.0 % RH`.

Invalid samples are discarded, but the node still sends a heartbeat using the
last valid temperature/humidity with `sensorValid=false` and an incrementing
`sensorFailures`. Before the first valid sample, fallbacks are **25 °C / 50 %**.
The edge uses this to raise `SENSOR_FAULT` instead of `DEVICE_OFFLINE` while the
device is still reporting.

### Light level

The module drives a **higher** voltage in darkness, so the firmware inverts the
12-bit ADC value:

```
raw 0    → 100 % light
raw 4095 →   0 % light
```

A 3:1 exponential smoothing filter reduces flicker without hiding a real door
opening:

```
percent = (percent * 3 + instantaneous + 2) / 4
```

Door debounce and open-duration policy belong to the gateway (US-52, TS-32).

### Presence

The PIR output is read as a boolean with a 2-second hold window so short
detections are not lost. Only the presence flag and timestamp are transmitted;
no image, identity or biometric data is handled. The firmware cannot detect a
disconnected PIR by itself, so `presenceAvailable` remains true; the gateway
marks presence unavailable when the field is missing (US-53 scenario 4).

### Clock

SNTP is **disabled by default** (`ENABLE_NTP = false`) so the node only talks to
the LAN. In that configuration `capturedAt` is `0` and the gateway's receipt
timestamp is authoritative. If a LAN time source exists, set
`ENABLE_NTP = true` and point `NTP_SERVER_1/2` at it. `uptimeMs` is always
present as a monotonic fallback.

### Idempotency

- Reading UID: `<DEVICE_ID>-<bootId>-<sequence>`
- Button event UID: `<DEVICE_ID>-<bootId>-btn-<buttonSequence>`

`bootId` is a random 32-bit value generated at startup, so IDs remain unique
across reboots even though `sequence` restarts at 1.

---

## Story traceability

This repository is the **device firmware**. The backlog assigns threshold
evaluation, door policy, calibration, daily quotas, SQLite persistence and
cloud synchronization to the edge/cloud; those rules are consumed as gateway
directives here.

| Story | Device-side implementation |
| --- | --- |
| **US-26** — Send telemetry to local gateway | 10 s sampling; HTTP POST to `/edge/api/v1/readings`; RAM FIFO; exponential backoff; oldest-first forwarding; immediate long-press notification. |
| **US-39** — Physical status signalling | Green / blinking red / solid amber / solid blue state machine; continuous alarm buzzer; maintenance suppresses the buzzer; signal-test sequence. |
| **US-40** — Check local device status on-site | Device reports the latest reading, validity flags, sequence, uptime and idempotent IDs. The gateway/mobile owns the query endpoint and pending-sync counters. |
| **US-49** — Mute buzzer and test signalling | Consumes `buzzerMuted` (latched until the excursion ends) and `signalTest` (3 s LED sequence + double beep), then restores the true operational state. Authorization is enforced by the edge. |
| **US-51** — Toggle maintenance via hardware button | 3 s long press; immediate button-event POST; blue LED + two beeps on activation; one beep on deactivation; three beeps when offline/rejected; edge owns the 30-minute window, order linking, safety override and daily quota. |
| **US-52** — Door-open detection | Reports the mapped `lightLevel` every 10 s (0 = dark/closed, 100 = bright/open). Door debounce, duration and alert policy live in the gateway (TS-31/TS-32). |
| **US-53** — Presence recording | Boolean `presence` with 2 s hold; no identity data; `presenceAvailable` for unavailable sensors. |
| **US-55** — Defective temperature sensor | Rejects out-of-range/invalid DHT11 samples, keeps the last valid value, sets `sensorValid=false`, reports `sensorFailures`. Sustained-fault and frozen-value detection live in the gateway. |
| **US-56** — Calibrate temperature sensor | Sends the **raw** temperature with a stable `readingUid`; the edge applies the −5…+5 °C offset and preserves the raw value for audit. |
| **TS-09** — Reading ingestion endpoint | Implements the client side of `POST /edge/api/v1/readings` and `POST /edge/api/v1/button-events`, including directive parsing and 400/4xx/5xx handling. |
| **TS-10** — Local threshold evaluation | Applies `alarmActive` directives to the LED/buzzer; hysteresis and excursion duration are evaluated by the edge. |
| **TS-27** — Local technician commands | Consumes `maintenanceMode`, `buzzerMuted` and `signalTest`; sends button events; reflects `ACTIVATED`, `DEACTIVATED`, `REJECTED_DAILY_LIMIT`. |
| **TS-31** — Telemetry/event contract | Includes `lightLevel`, `presence`, `presenceAvailable`, `sensorValid`, `sensorFailures`, `capturedAt` and unique IDs. |
| **TS-32** — Door policy and calibration config | Provides raw readings and stable IDs so the edge can apply `doorOpenLightThresholdPercent`, `doorOpenAlertMinutes`, `lightDebounceSeconds` and `calibrationOffsetCelsius` offline. |

---

## LAN-only operation

NFR-04 requires the ESP32↔gateway link to stay inside the on-site LAN. The
firmware is designed accordingly:

- It makes **outbound HTTP requests only** to `GATEWAY_BASE_URL`.
- It never opens a listening socket, web server or inbound port.
- It does not require the internet to sample, buffer, signal or toggle
  maintenance via the button.
- SNTP is off by default. The optional clock points at LAN servers when enabled,
  never at public time services.
- The optional `X-Device-Key` header lets the edge authenticate the device.

For production, also:

- put the ESP32 and gateway on a dedicated IoT VLAN or Wi-Fi SSID;
- use a DHCP reservation for the gateway;
- rotate `DEVICE_API_KEY` per device and revoke lost nodes;
- keep the gateway's public-facing API behind TLS (the LAN hop is plain HTTP in
  this reference implementation; see [Known limitations](#known-limitations)).

---

## Troubleshooting

### The LED is always amber

- Confirm the ESP32 and the gateway host are on the same LAN and can ping each
  other.
- Check `GATEWAY_BASE_URL` (scheme, IP, port, no trailing path).
- Allow the port through the host firewall (for the mock gateway, allow inbound
  TCP 8080).
- Remember that ESP32 Wi-Fi is **2.4 GHz only**.
- Open the serial monitor at **115200 baud** and read the `[HTTP] Failure …`
  lines; they include the error and the next retry delay.

### DHT11 always reports `sensorValid=false`

- Check the data line (GPIO15) and the 10 kΩ pull-up to 3V3.
- Power the sensor from 3.3 V and use a common ground.
- Some DHT11 modules need a moment after power-up; the first samples may fail.
- The firmware discards values outside 0–50 °C / 20–90 %; a DHT22 reading
  outside that envelope is intentionally rejected.

### The RGB LED colours look inverted

- The default is common anode. If your LED is common cathode, connect COM to GND
  and set `AppConfig::RGB_COMMON_ANODE = false`.
- Verify the three 220 Ω resistors and the R/G/B pin order.

### The button does nothing

- Use a momentary button between GPIO32 and GND; GPIO32 uses the internal
  pull-up and is active low.
- Hold for the full 3 seconds. Short presses are ignored by design.
- If the gateway is unreachable, the device emits three warning beeps and does
  not change state (US-51 scenario 4).

### The PIR never triggers

- PIR sensors need a 30–60 s warm-up after power-up.
- Power HC-SR501 modules from `VIN` (5 V) rather than 3.3 V when possible.
- Adjust the sensitivity/delay potentiometers on the module.
- In Wokwi, click the PIR and choose **Simulate Motion**.

### Compilation fails with “sketch file not found” or a name mismatch

Arduino requires the `.ino` name to match the folder. Use
`.\tools\build.ps1`, or follow [Arduino IDE](#arduino-ide) to open a correctly
named copy. Wokwi does not have this restriction.

### Readings are accepted, then dropped with 400/413/422

The gateway rejected the payload. The firmware deliberately drops poison
payloads so one bad reading cannot block the FIFO. Check the JSON contract,
sensor ranges and the gateway logs. `401/403/404` are retried instead because
they usually indicate a provisioning problem.

### `capturedAt` is always 0

That is expected with `ENABLE_NTP = false`. The edge stamps the reading on
receipt. Enable SNTP only if a LAN time source is available.

---

## Known limitations

- **RAM-only buffer.** Readings survive Wi-Fi drops but not a power cycle. The
  gateway owns durable storage (NFR-05).
- **Plain HTTP on the LAN.** The ESP32↔gateway hop in this reference firmware
  uses HTTP for simplicity and to match the LAN-only requirement. Add TLS/mTLS
  if your threat model requires it; the ESP32 has limited TLS headroom.
- **Compile-time provisioning.** Wi-Fi credentials, gateway URL and device ID
  are constants. Production should use NVS or a provisioning app.
- **No OTA updates.** Flashing is over USB.
- **One gateway URL.** No failover between edge hosts.
- **Frozen-sensor detection is edge-side.** The device reports validity and a
  consecutive-failure count; the gateway decides when a fault alert opens or
  resolves.
- **PIR availability is not self-diagnosed.** A disconnected PIR reads low; the
  gateway treats a missing field as unavailable, but this firmware always sends
  the field.
- **HTTP POST blocks for up to 4 s.** The loop is otherwise cooperative;
  sampling may be delayed briefly during a slow request.
- **The built-in mock mode is intentionally simple.** It does not enforce the
  daily quota or persist state. Use `tools/mock_gateway.py` for those rules.

---

## License and credits

- Application code: see [`LICENSE.md`](LICENSE.md).
- Modest IoT Nano-framework (C++ Edition) by **Angel Velasquez**. The framework
  files are included unmodified and retain their original license headers; see
  [`LICENSE.md`](LICENSE.md) for the framework license notice.
- Tutorial reference: [ESP32 IoT Learning Kit](https://esp32-iot-learning-kit1.readthedocs.io/en/latest/).
- Requirements: [`product-backlog.md`](product-backlog.md).
