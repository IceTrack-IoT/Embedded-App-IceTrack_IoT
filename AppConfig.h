#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * @file AppConfig.h
 * @brief Compile-time provisioning and tuning for the IceTrack ESP32 node.
 *
 * Everything that changes between installations (Wi-Fi credentials, gateway
 * address, device identifier, GPIO wiring and timing) is centralized here.
 * The values below are safe defaults for the ESP32 DevKit V1 and the Wokwi
 * simulation. For a real site, replace WIFI_SSID, WIFI_PASSWORD and
 * GATEWAY_BASE_URL, and give every physical node a unique DEVICE_ID.
 */

#include <Arduino.h>

namespace AppConfig {

// ---------------------------------------------------------------------------
// Provisioning
// ---------------------------------------------------------------------------

/** @brief Local site Wi-Fi network (never a public hotspot). */
constexpr char WIFI_SSID[] = "Wokwi-GUEST";

/** @brief Local site Wi-Fi password (empty for open networks). */
constexpr char WIFI_PASSWORD[] = "";

/**
 * @brief Base URL of the on-site IceTrack Edge gateway.
 *
 * The ESP32 only talks to this LAN address; it never reaches the public
 * internet. Use the gateway's static LAN IP, for example:
 *   http://192.168.1.50:8080
 */
constexpr char GATEWAY_BASE_URL[] = "http://192.168.1.50:8080";

/** @brief Unique identifier provisioned for this physical device. */
constexpr char DEVICE_ID[] = "ice-esp32-001";

/** @brief Optional device key sent as X-Device-Key (empty = disabled). */
constexpr char DEVICE_API_KEY[] = "";

/** @brief Firmware version reported in telemetry (useful for support). */
constexpr char FIRMWARE_VERSION[] = "1.0.0";

// ---------------------------------------------------------------------------
// GPIO wiring (ESP32 DevKit V1)
// ---------------------------------------------------------------------------

constexpr uint8_t PIN_DHT = 15;      ///< DHT11 data line.
constexpr uint8_t PIN_LIGHT = 34;    ///< LDR module analog output (input only).
constexpr uint8_t PIN_PIR = 33;      ///< PIR motion sensor digital output.
constexpr uint8_t PIN_BUTTON = 32;   ///< Maintenance push button (active low).
constexpr uint8_t PIN_LED_R = 25;    ///< RGB LED red channel.
constexpr uint8_t PIN_LED_G = 26;    ///< RGB LED green channel.
constexpr uint8_t PIN_LED_B = 27;    ///< RGB LED blue channel.
constexpr uint8_t PIN_BUZZER = 14;   ///< Active/piezo buzzer.

/** @brief Set true for a common-anode RGB LED (channels are active low). */
constexpr bool RGB_COMMON_ANODE = true;

// ---------------------------------------------------------------------------
// Timing and behaviour
// ---------------------------------------------------------------------------

/** @brief Telemetry period required by (one reading every 10 s). */
constexpr uint32_t READING_INTERVAL_MS = 10000UL;

/** @brief Long press required to toggle maintenance mode. */
constexpr uint32_t BUTTON_LONG_PRESS_MS = 3000UL;

/** @brief Contact debounce for the maintenance button. */
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50UL;

/** @brief PIR hold time so short detections are not missed. */
constexpr uint32_t PIR_HOLD_MS = 2000UL;

/** @brief HTTP timeout for a gateway request. */
constexpr uint32_t HTTP_TIMEOUT_MS = 4000UL;

/** @brief How often a disconnected Wi-Fi station is retried. */
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 15000UL;

/** @brief Consecutive failed transmissions before the amber LED. */
constexpr uint8_t OFFLINE_FAILURE_THRESHOLD = 3;

/** @brief Maximum retry backoff with jitter. */
constexpr uint32_t MAX_BACKOFF_MS = 60000UL;

/** @brief Small gap while draining a backlog so the gateway is not flooded. */
constexpr uint32_t BACKLOG_DRAIN_GAP_MS = 300UL;

/** @brief In-memory buffer capacity (360 readings = 1 hour at 10 s). */
constexpr uint16_t TELEMETRY_BUFFER_CAPACITY = 360;

/** @brief Tone frequency for the passive piezo buzzer. */
constexpr uint16_t BUZZER_TONE_HZ = 2000;

/** @brief Use the built-in mock gateway. Also auto-enabled on Wokwi-GUEST. */
constexpr bool USE_MOCK_GATEWAY = false;

/** @brief Enable verbose serial traces (115200 baud). */
constexpr bool ENABLE_SERIAL_LOGS = true;

/**
 * @brief Best-effort SNTP clock used only for the optional capture timestamp.
 *
 * Disabled by default because NFR-04 keeps the ESP32 link inside the on-site
 * LAN. The gateway stamps every reading with its own receipt time, so leaving
 * this off only makes TelemetryReading::capturedEpoch zero. If enabled, point
 * NTP_SERVER_* at a LAN time server or at the edge host; never at a public
 * server on an air-gapped site.
 */
constexpr bool ENABLE_NTP = false;
constexpr char NTP_SERVER_1[] = "192.168.1.50";
constexpr char NTP_SERVER_2[] = "192.168.1.51";

/** @brief Unix time below this value means SNTP has not synchronized yet. */
constexpr uint32_t MIN_VALID_EPOCH = 1700000000UL;

// ---------------------------------------------------------------------------
// Sensor plausibility limits (DHT11 datasheet, product backlog scope)
// ---------------------------------------------------------------------------

constexpr float MIN_VALID_TEMPERATURE_C = 0.0f;
constexpr float MAX_VALID_TEMPERATURE_C = 50.0f;
constexpr float MIN_VALID_HUMIDITY_PCT = 20.0f;
constexpr float MAX_VALID_HUMIDITY_PCT = 90.0f;

/** @brief Fallback values used only while a faulty DHT has no last good read. */
constexpr float DHT_FALLBACK_TEMPERATURE_C = 25.0f;
constexpr float DHT_FALLBACK_HUMIDITY_PCT = 50.0f;

// ---------------------------------------------------------------------------
// Wokwi / mock gateway
// ---------------------------------------------------------------------------

/** @brief Mock alarm threshold used only by the simulator gateway. */
constexpr float MOCK_ALARM_TEMPERATURE_C = 30.0f;

} // namespace AppConfig

#endif // APP_CONFIG_H
