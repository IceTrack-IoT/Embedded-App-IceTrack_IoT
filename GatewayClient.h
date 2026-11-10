#ifndef GATEWAY_CLIENT_H
#define GATEWAY_CLIENT_H

/**
 * @file GatewayClient.h
 * @brief LAN-only HTTP transport between the ESP32 and the IceTrack Edge API.
 *
 * Endpoints (TS-09):
 *   POST /edge/api/v1/readings
 *   POST /edge/api/v1/button-events
 *
 * The client also manages Wi-Fi reconnection. It only sends requests to the
 * configured gateway URL on the site LAN, satisfying NFR-04. A mock mode is
 * available for Wokwi and for bench demonstrations when no gateway is running.
 */

#include "AppConfig.h"
#include "Telemetry.h"
#include <Arduino.h>
#include <ArduinoJson.h>

struct GatewayDirectives {
    bool hasAlarm = false;
    bool alarmActive = false;

    bool hasBuzzerMuted = false;
    bool buzzerMuted = false;

    bool hasSignalTest = false;
    bool signalTest = false;

    bool hasMaintenance = false;
    bool maintenanceActive = false;
    char maintenanceOrigin[12] = {0};
    char maintenanceExpiresAt[32] = {0};
};

enum class ButtonFeedback : uint8_t {
    ACTIVATED,
    DEACTIVATED,
    REJECTED_DAILY_LIMIT,
    UNKNOWN
};

struct ButtonEventResult {
    bool hasFeedback = false;
    ButtonFeedback feedback = ButtonFeedback::UNKNOWN;

    bool hasMaintenance = false;
    bool maintenanceActive = false;
    char origin[12] = {0};
    char expiresAt[32] = {0};
};

class GatewayClient {
public:
    GatewayClient();

    /** @brief Starts the Wi-Fi station (or mock mode). */
    void begin();

    /** @brief Non-blocking Wi-Fi supervision; call from the main loop. */
    void update();

    bool isWifiConnected() const;
    bool isMockMode() const { return mockMode; }

    /**
     * @brief Posts one reading and parses the signalling directives.
     * @return true on a 2xx response.
     */
    bool postReading(const TelemetryReading& reading, GatewayDirectives& directives);

    /**
     * @brief Posts a LONG button event and parses the gateway feedback.
     * @return true on a 2xx response.
     */
    bool postButtonEvent(const char* eventUid,
                         uint32_t uptimeMs,
                         uint32_t capturedEpoch,
                         ButtonEventResult& result);

    int lastHttpStatus() const { return lastStatus; }
    const char* lastError() const { return lastErrorText; }

private:
    bool mockMode;
    bool wifiStarted;
    bool wasConnected;
    uint32_t lastWifiAttemptMs;
    int lastStatus;
    char lastErrorText[64];

    // Mock gateway state used only in simulation.
    bool mockMaintenanceActive;

    bool postJson(const char* path, const String& body, String& response);
    String buildUrl(const char* path) const;
    void parseDirectives(const String& payload, GatewayDirectives& directives);
    void parseButtonResult(const String& payload, ButtonEventResult& result);
    void copyText(const char* source, char* destination, size_t size);
    void copyJsonValue(JsonVariantConst value, char* destination, size_t size);
    void setError(const char* message);
};

#endif // GATEWAY_CLIENT_H
