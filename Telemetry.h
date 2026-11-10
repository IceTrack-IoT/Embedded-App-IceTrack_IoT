#ifndef TELEMETRY_H
#define TELEMETRY_H

/**
 * @file Telemetry.h
 * @brief Payload exchanged between the ESP32 and the local IceTrack gateway.
 *
 * The struct carries the five measurements required by TS-09/TS-31
 * (temperature, humidity, light level, presence) plus traceability metadata
 * (sequence, capture time, unique reading id and sensor validity).
 */

#include <Arduino.h>

struct TelemetryReading {
    float temperatureC = 0.0f;      ///< Last valid raw temperature; the edge calibrates it.
    float humidityPct = 0.0f;       ///< Last valid relative humidity.
    int lightPercent = 0;           ///< 0-100 % light level (door context).
    bool presence = false;          ///< PIR presence flag; never identifies a person.
    bool presenceAvailable = true;  ///< False when the PIR cannot be interpreted.
    bool sensorValid = false;       ///< False when the DHT11 read was discarded.
    uint32_t sensorFailures = 0;    ///< Consecutive invalid DHT11 samples.
    uint32_t sequence = 0;          ///< Monotonic reading number since boot.
    uint32_t uptimeMs = 0;          ///< Capture time relative to boot.
    uint32_t capturedEpoch = 0;     ///< UTC capture time, 0 when SNTP is unavailable.
    char readingUid[64] = {0};      ///< Idempotency key: <device>-<boot>-<sequence>.
};

#endif // TELEMETRY_H
