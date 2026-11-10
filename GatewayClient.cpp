/**
 * @file GatewayClient.cpp
 * @brief Implements the LAN gateway transport and response parsing.
 */

#include "GatewayClient.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cstring>

GatewayClient::GatewayClient()
    : mockMode(AppConfig::USE_MOCK_GATEWAY ||
               strcmp(AppConfig::WIFI_SSID, "Wokwi-GUEST") == 0),
      wifiStarted(false),
      wasConnected(false),
      lastWifiAttemptMs(0),
      lastStatus(0),
      mockMaintenanceActive(false) {
    lastErrorText[0] = '\0';
}

void GatewayClient::begin() {
    if (mockMode) {
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.println("[Gateway] Mock mode enabled (no LAN traffic)");
        }
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD);
    wifiStarted = true;
    lastWifiAttemptMs = millis();

    if (AppConfig::ENABLE_SERIAL_LOGS) {
        Serial.printf("[WiFi] Connecting to \"%s\"...\n", AppConfig::WIFI_SSID);
        Serial.printf("[Gateway] Base URL: %s\n", AppConfig::GATEWAY_BASE_URL);
    }
}

void GatewayClient::update() {
    if (mockMode || !wifiStarted) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (!wasConnected) {
            wasConnected = true;
            if (AppConfig::ENABLE_SERIAL_LOGS) {
                Serial.printf("[WiFi] Connected, IP=%s\n",
                              WiFi.localIP().toString().c_str());
            }
        }
        return;
    }

    if (wasConnected) {
        wasConnected = false;
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.println("[WiFi] Link lost, buffering readings");
        }
    }

    const uint32_t now = millis();
    if ((now - lastWifiAttemptMs) >= AppConfig::WIFI_RETRY_INTERVAL_MS) {
        lastWifiAttemptMs = now;
        WiFi.disconnect();
        WiFi.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD);
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.println("[WiFi] Retrying connection...");
        }
    }
}

bool GatewayClient::isWifiConnected() const {
    return mockMode || WiFi.status() == WL_CONNECTED;
}

bool GatewayClient::postReading(const TelemetryReading& reading,
                                GatewayDirectives& directives) {
    lastStatus = 0;
    lastErrorText[0] = '\0';

    if (mockMode) {
        directives = GatewayDirectives();
        directives.hasAlarm = true;
        directives.alarmActive =
            reading.temperatureC >= AppConfig::MOCK_ALARM_TEMPERATURE_C;
        directives.hasBuzzerMuted = true;
        directives.buzzerMuted = false;
        directives.hasSignalTest = true;
        directives.signalTest = false;
        directives.hasMaintenance = true;
        directives.maintenanceActive = mockMaintenanceActive;
        copyText("BUTTON", directives.maintenanceOrigin,
                 sizeof(directives.maintenanceOrigin));
        copyText("mock", directives.maintenanceExpiresAt,
                 sizeof(directives.maintenanceExpiresAt));

        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.printf("[Gateway:mock] reading %s accepted, alarm=%d maintenance=%d\n",
                          reading.readingUid,
                          directives.alarmActive ? 1 : 0,
                          directives.maintenanceActive ? 1 : 0);
        }
        return true;
    }

    if (!isWifiConnected()) {
        setError("wifi-offline");
        return false;
    }

    JsonDocument doc;
    doc["deviceId"] = AppConfig::DEVICE_ID;
    doc["firmwareVersion"] = AppConfig::FIRMWARE_VERSION;
    doc["readingUid"] = reading.readingUid;
    doc["temperature"] = reading.temperatureC;
    doc["humidity"] = reading.humidityPct;
    doc["lightLevel"] = reading.lightPercent;
    doc["presence"] = reading.presence;
    doc["presenceAvailable"] = reading.presenceAvailable;
    doc["sensorValid"] = reading.sensorValid;
    doc["sensorFailures"] = reading.sensorFailures;
    doc["sequence"] = reading.sequence;
    doc["uptimeMs"] = reading.uptimeMs;
    doc["capturedAt"] = reading.capturedEpoch;

    String body;
    body.reserve(320);
    serializeJson(doc, body);

    String response;
    if (!postJson("/edge/api/v1/readings", body, response)) {
        return false;
    }

    if (response.length() > 0) {
        parseDirectives(response, directives);
    }
    return true;
}

bool GatewayClient::postButtonEvent(const char* eventUid,
                                    uint32_t uptimeMs,
                                    uint32_t capturedEpoch,
                                    ButtonEventResult& result) {
    lastStatus = 0;
    lastErrorText[0] = '\0';

    if (mockMode) {
        mockMaintenanceActive = !mockMaintenanceActive;

        result = ButtonEventResult();
        result.hasFeedback = true;
        result.feedback = mockMaintenanceActive
                              ? ButtonFeedback::ACTIVATED
                              : ButtonFeedback::DEACTIVATED;
        result.hasMaintenance = true;
        result.maintenanceActive = mockMaintenanceActive;
        copyText("BUTTON", result.origin, sizeof(result.origin));
        copyText("mock", result.expiresAt, sizeof(result.expiresAt));

        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.printf("[Gateway:mock] button %s -> %s\n", eventUid,
                          mockMaintenanceActive ? "ACTIVATED" : "DEACTIVATED");
        }
        return true;
    }

    if (!isWifiConnected()) {
        setError("wifi-offline");
        return false;
    }

    JsonDocument doc;
    doc["deviceId"] = AppConfig::DEVICE_ID;
    doc["pressType"] = "LONG";
    doc["timestamp"] = capturedEpoch > 0 ? capturedEpoch : uptimeMs;
    doc["capturedAt"] = capturedEpoch;
    doc["uptimeMs"] = uptimeMs;
    doc["eventUid"] = eventUid;

    String body;
    body.reserve(224);
    serializeJson(doc, body);

    String response;
    if (!postJson("/edge/api/v1/button-events", body, response)) {
        return false;
    }

    if (response.length() > 0) {
        parseButtonResult(response, result);
    }
    return true;
}

bool GatewayClient::postJson(const char* path, const String& body,
                             String& response) {
    WiFiClient client;
    HTTPClient http;
    const String url = buildUrl(path);

    if (!http.begin(client, url)) {
        setError("http-begin-failed");
        return false;
    }

    http.setTimeout(static_cast<uint16_t>(AppConfig::HTTP_TIMEOUT_MS));
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    if (strlen(AppConfig::DEVICE_API_KEY) > 0) {
        http.addHeader("X-Device-Key", AppConfig::DEVICE_API_KEY);
    }

    const int status = http.POST(
        reinterpret_cast<uint8_t*>(const_cast<char*>(body.c_str())),
        body.length());

    lastStatus = status;

    if (status > 0) {
        response = http.getString();
    } else {
        response = "";
    }
    http.end();

    if (status >= 200 && status < 300) {
        return true;
    }

    char message[32];
    snprintf(message, sizeof(message), "http-%d", status);
    setError(message);
    return false;
}

String GatewayClient::buildUrl(const char* path) const {
    String base(AppConfig::GATEWAY_BASE_URL);
    while (base.endsWith("/")) {
        base.remove(base.length() - 1);
    }
    return base + path;
}

void GatewayClient::parseDirectives(const String& payload,
                                    GatewayDirectives& directives) {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        setError("bad-json");
        return;
    }

    if (doc["alarmActive"].is<bool>()) {
        directives.hasAlarm = true;
        directives.alarmActive = doc["alarmActive"].as<bool>();
    }

    if (doc["buzzerMuted"].is<bool>()) {
        directives.hasBuzzerMuted = true;
        directives.buzzerMuted = doc["buzzerMuted"].as<bool>();
    }

    if (doc["signalTest"].is<bool>()) {
        directives.hasSignalTest = true;
        directives.signalTest = doc["signalTest"].as<bool>();
    }

    if (doc["maintenanceActive"].is<bool>()) {
        directives.hasMaintenance = true;
        directives.maintenanceActive = doc["maintenanceActive"].as<bool>();
    }

    JsonObject maintenance = doc["maintenanceMode"].as<JsonObject>();
    if (!maintenance.isNull()) {
        directives.hasMaintenance = true;
        if (maintenance["active"].is<bool>()) {
            directives.maintenanceActive = maintenance["active"].as<bool>();
        }
        copyJsonValue(maintenance["origin"], directives.maintenanceOrigin,
                      sizeof(directives.maintenanceOrigin));
        copyJsonValue(maintenance["expiresAt"], directives.maintenanceExpiresAt,
                      sizeof(directives.maintenanceExpiresAt));
    }
}

void GatewayClient::parseButtonResult(const String& payload,
                                      ButtonEventResult& result) {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        setError("bad-json");
        return;
    }

    if (doc["feedback"].is<const char*>()) {
        const char* feedback = doc["feedback"].as<const char*>();
        result.hasFeedback = true;

        if (strcmp(feedback, "ACTIVATED") == 0) {
            result.feedback = ButtonFeedback::ACTIVATED;
        } else if (strcmp(feedback, "DEACTIVATED") == 0) {
            result.feedback = ButtonFeedback::DEACTIVATED;
        } else if (strcmp(feedback, "REJECTED_DAILY_LIMIT") == 0) {
            result.feedback = ButtonFeedback::REJECTED_DAILY_LIMIT;
        } else {
            result.feedback = ButtonFeedback::UNKNOWN;
        }
    }

    JsonObject maintenance = doc["maintenanceMode"].as<JsonObject>();
    if (!maintenance.isNull()) {
        result.hasMaintenance = true;
        if (maintenance["active"].is<bool>()) {
            result.maintenanceActive = maintenance["active"].as<bool>();
        }
        copyJsonValue(maintenance["origin"], result.origin, sizeof(result.origin));
        copyJsonValue(maintenance["expiresAt"], result.expiresAt,
                      sizeof(result.expiresAt));
    }
}

void GatewayClient::copyText(const char* source, char* destination,
                             size_t size) {
    if (destination == nullptr || size == 0) {
        return;
    }

    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}

void GatewayClient::copyJsonValue(JsonVariantConst value, char* destination,
                                  size_t size) {
    if (value.is<const char*>()) {
        copyText(value.as<const char*>(), destination, size);
        return;
    }

    if (value.is<unsigned long>() || value.is<long>() || value.is<int>()) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lu", value.as<unsigned long>());
        copyText(buffer, destination, size);
        return;
    }

    if (!value.isNull()) {
        String text;
        serializeJson(value, text);
        copyText(text.c_str(), destination, size);
        return;
    }

    if (destination != nullptr && size > 0) {
        destination[0] = '\0';
    }
}

void GatewayClient::setError(const char* message) {
    copyText(message, lastErrorText, sizeof(lastErrorText));
}
