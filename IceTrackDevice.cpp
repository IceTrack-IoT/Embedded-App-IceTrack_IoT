/**
 * @file IceTrackDevice.cpp
 * @brief Implements the IceTrack refrigeration-monitoring node.
 */

#include "IceTrackDevice.h"
#include "AppConfig.h"

#include <Arduino.h>
#include <esp_system.h>
#include <time.h>

IceTrackDevice::IceTrackDevice()
    : dht(AppConfig::PIN_DHT, this),
      light(AppConfig::PIN_LIGHT, this),
      pir(AppConfig::PIN_PIR, this),
      button(AppConfig::PIN_BUTTON, this),
      led(AppConfig::PIN_LED_R, AppConfig::PIN_LED_G, AppConfig::PIN_LED_B, this),
      buzzer(AppConfig::PIN_BUZZER, this),
      buffer(AppConfig::TELEMETRY_BUFFER_CAPACITY),
      bootId(0),
      sequence(0),
      buttonSequence(0),
      lastSampleMs(0),
      nextSendMs(0),
      backoffMs(0),
      consecutiveFailures(0),
      timeSynced(false),
      ntpStarted(false),
      lastDhtValid(true),
      lastPresence(false),
      signalTestActive(false),
      lastIndicator(0xFF),
      lastBuzzerAlarm(false) {}

void IceTrackDevice::begin() {
    if (AppConfig::ENABLE_SERIAL_LOGS) {
        Serial.begin(115200);
        delay(100);
        Serial.println();
        Serial.println("==============================================");
        Serial.println(" IceTrack ESP32 refrigeration monitoring node");
        Serial.println(" Modest IoT Nano-framework (C++ Edition)");
        Serial.println("==============================================");
        Serial.printf("Device ID      : %s\n", AppConfig::DEVICE_ID);
        Serial.printf("Firmware       : %s\n", AppConfig::FIRMWARE_VERSION);
        Serial.printf("Gateway        : %s\n", AppConfig::GATEWAY_BASE_URL);
        Serial.printf("Sample period  : %lu ms\n",
                      static_cast<unsigned long>(AppConfig::READING_INTERVAL_MS));
        Serial.printf("Buffer capacity: %u readings\n",
                      static_cast<unsigned int>(buffer.capacity()));
    }

    bootId = esp_random();

    dht.begin();
    light.begin();
    pir.begin();
    button.begin();
    led.begin();
    buzzer.begin();
    gateway.begin();

    lastSampleMs = millis();
    nextSendMs = 0;
    sequence = 0;
    buttonSequence = 0;
    consecutiveFailures = 0;
    backoffMs = 0;

    applyIndicators();

    if (AppConfig::ENABLE_SERIAL_LOGS) {
        Serial.printf("[Node] Boot session 0x%08lx, initial state green\n",
                      static_cast<unsigned long>(bootId));
    }
}

void IceTrackDevice::loop() {
    gateway.update();
    updateClock();

    button.update();
    led.update();
    buzzer.update();

    const uint32_t now = millis();

    if ((now - lastSampleMs) >= AppConfig::READING_INTERVAL_MS) {
        lastSampleMs = now;
        sampleSensors();
    }

    if (!buffer.isEmpty() &&
        static_cast<int32_t>(now - nextSendMs) >= 0) {
        attemptSend();
    }

    applyIndicators();
}

void IceTrackDevice::on(Event event) {
    if (event == MaintenanceButton::LONG_PRESS_EVENT) {
        handleButtonLongPress();
        return;
    }

    if (event == Dht11Sensor::READING_EVENT) {
        if (!lastDhtValid && AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.println("[DHT11] Reading valid again");
        }
        lastDhtValid = true;
        return;
    }

    if (event == Dht11Sensor::READ_FAILED_EVENT) {
        if (lastDhtValid && AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.println("[DHT11] Invalid reading discarded; heartbeat flagged");
        }
        lastDhtValid = false;
        return;
    }

    if (event == PirSensor::PRESENCE_CHANGED_EVENT) {
        const bool present = pir.isPresent();
        if (present != lastPresence) {
            lastPresence = present;
            if (AppConfig::ENABLE_SERIAL_LOGS) {
                Serial.printf("[PIR] Presence %s\n", present ? "detected" : "cleared");
            }
        }
        return;
    }

    // LightSensor and generic sensor events need no local action: the value is
    // read through the sensor getter when the telemetry frame is assembled.
}

void IceTrackDevice::handle(Command command) {
    // Actuators propagate the command they just applied. This hook is reserved
    // for observability or future edge acknowledgements; no action is needed.
    (void)command;
}

void IceTrackDevice::updateClock() {
    if (timeSynced || !AppConfig::ENABLE_NTP) {
        return;
    }

    if (!gateway.isMockMode() && !ntpStarted && gateway.isWifiConnected()) {
        configTime(0, 0, AppConfig::NTP_SERVER_1, AppConfig::NTP_SERVER_2);
        ntpStarted = true;
    }

    if (!ntpStarted) {
        return;
    }

    const time_t now = time(nullptr);
    if (now >= static_cast<time_t>(AppConfig::MIN_VALID_EPOCH)) {
        timeSynced = true;
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.printf("[NTP] Clock synchronized: %lu\n",
                          static_cast<unsigned long>(now));
        }
    }
}

void IceTrackDevice::sampleSensors() {
    const bool dhtValid = dht.update();
    light.update();
    pir.update();

    TelemetryReading reading;
    reading.temperatureC = dht.getTemperatureC();
    reading.humidityPct = dht.getHumidityPct();
    reading.sensorValid = dhtValid;
    reading.sensorFailures = dht.getConsecutiveFailures();
    reading.lightPercent = light.getPercent();
    reading.presence = pir.isPresent();
    reading.presenceAvailable = pir.isAvailable();
    reading.sequence = ++sequence;
    reading.uptimeMs = millis();
    reading.capturedEpoch = timeSynced
                                ? static_cast<uint32_t>(time(nullptr))
                                : 0;

    snprintf(reading.readingUid, sizeof(reading.readingUid),
             "%s-%08lx-%lu",
             AppConfig::DEVICE_ID,
             static_cast<unsigned long>(bootId),
             static_cast<unsigned long>(reading.sequence));

    const uint32_t droppedBefore = buffer.droppedCount();
    buffer.push(reading);

    if (buffer.droppedCount() != droppedBefore &&
        AppConfig::ENABLE_SERIAL_LOGS) {
        Serial.println("[Buffer] Full: oldest reading discarded");
    }

    if (AppConfig::ENABLE_SERIAL_LOGS) {
        Serial.printf(
            "[Sample] seq=%lu T=%.1fC H=%.0f%% light=%d%% presence=%d valid=%d buffered=%u\n",
            static_cast<unsigned long>(reading.sequence),
            reading.temperatureC,
            reading.humidityPct,
            reading.lightPercent,
            reading.presence ? 1 : 0,
            reading.sensorValid ? 1 : 0,
            static_cast<unsigned int>(buffer.size()));
    }
}

void IceTrackDevice::attemptSend() {
    const TelemetryReading* next = buffer.peek();
    if (next == nullptr) {
        return;
    }

    if (!gateway.isWifiConnected()) {
        registerFailure("wifi-offline");
        return;
    }

    GatewayDirectives incoming;
    if (gateway.postReading(*next, incoming)) {
        buffer.pop();
        consecutiveFailures = 0;
        backoffMs = 0;

        handleDirectives(incoming);

        nextSendMs = millis() +
                     (buffer.isEmpty() ? 0 : AppConfig::BACKLOG_DRAIN_GAP_MS);

        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.printf("[HTTP] 2xx reading %s sent (pending=%u)\n",
                          next->readingUid,
                          static_cast<unsigned int>(buffer.size()));
        }
        return;
    }

    const int status = gateway.lastHttpStatus();

    // A permanently invalid payload must not block the FIFO forever. 401/403
    // and 404 are configuration problems that the operator can fix, so those
    // are retried with backoff instead of silently dropping telemetry.
    const bool poisonPayload =
        (status == 400 || status == 413 || status == 422);

    if (poisonPayload) {
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.printf("[HTTP] %d rejected reading %s; dropping payload\n",
                          status, next->readingUid);
        }
        buffer.pop();
        consecutiveFailures = 0;
        backoffMs = 0;
        nextSendMs = millis() + AppConfig::BACKLOG_DRAIN_GAP_MS;
        return;
    }

    registerFailure(gateway.lastError());
}

void IceTrackDevice::registerFailure(const char* reason) {
    if (consecutiveFailures < 255) {
        ++consecutiveFailures;
    }

    const uint8_t exponent =
        (consecutiveFailures == 0)
            ? 0
            : static_cast<uint8_t>(consecutiveFailures > 6 ? 6
                                                           : consecutiveFailures - 1);
    uint32_t delayMs = 1000UL << exponent;
    delayMs += static_cast<uint32_t>(random(0, 251));

    if (delayMs > AppConfig::MAX_BACKOFF_MS) {
        delayMs = AppConfig::MAX_BACKOFF_MS;
    }

    backoffMs = delayMs;
    nextSendMs = millis() + backoffMs;

    if (AppConfig::ENABLE_SERIAL_LOGS) {
        Serial.printf("[HTTP] Failure %u (%s), retry in %lu ms, pending=%u\n",
                      static_cast<unsigned int>(consecutiveFailures),
                      reason,
                      static_cast<unsigned long>(backoffMs),
                      static_cast<unsigned int>(buffer.size()));
    }

    applyIndicators();
}

void IceTrackDevice::handleButtonLongPress() {
    char eventUid[64];
    ++buttonSequence;
    snprintf(eventUid, sizeof(eventUid),
             "%s-%08lx-btn-%lu",
             AppConfig::DEVICE_ID,
             static_cast<unsigned long>(bootId),
             static_cast<unsigned long>(buttonSequence));

    const uint32_t uptimeMs = millis();
    const uint32_t epoch = timeSynced
                               ? static_cast<uint32_t>(time(nullptr))
                               : 0;

    if (!gateway.isWifiConnected()) {
        buzzer.handle(Buzzer::BEEP_WARNING_COMMAND);
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.println("[Button] LONG press ignored: gateway unreachable (3 beeps)");
        }
        return;
    }

    ButtonEventResult result;
    if (!gateway.postButtonEvent(eventUid, uptimeMs, epoch, result)) {
        buzzer.handle(Buzzer::BEEP_WARNING_COMMAND);
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.printf("[Button] LONG press not delivered (%s); 3 beeps\n",
                          gateway.lastError());
        }
        return;
    }

    if (result.hasMaintenance) {
        directives.hasMaintenance = true;
        directives.maintenanceActive = result.maintenanceActive;
        snprintf(directives.maintenanceOrigin,
                 sizeof(directives.maintenanceOrigin), "%s",
                 result.origin[0] != '\0' ? result.origin : "BUTTON");
        snprintf(directives.maintenanceExpiresAt,
                 sizeof(directives.maintenanceExpiresAt), "%s",
                 result.expiresAt);
    }

    switch (result.feedback) {
        case ButtonFeedback::ACTIVATED:
            buzzer.handle(Buzzer::BEEP_ACTIVATED_COMMAND);
            if (AppConfig::ENABLE_SERIAL_LOGS) {
                Serial.println("[Button] Maintenance window ACTIVATED (blue LED, 2 beeps)");
            }
            break;

        case ButtonFeedback::DEACTIVATED:
            buzzer.handle(Buzzer::BEEP_DEACTIVATED_COMMAND);
            if (AppConfig::ENABLE_SERIAL_LOGS) {
                Serial.println("[Button] Maintenance window DEACTIVATED (1 beep)");
            }
            break;

        case ButtonFeedback::REJECTED_DAILY_LIMIT:
            buzzer.handle(Buzzer::BEEP_WARNING_COMMAND);
            if (AppConfig::ENABLE_SERIAL_LOGS) {
                Serial.println("[Button] Rejected: daily activation limit (3 beeps)");
            }
            break;

        case ButtonFeedback::UNKNOWN:
        default:
            buzzer.handle(Buzzer::BEEP_WARNING_COMMAND);
            if (AppConfig::ENABLE_SERIAL_LOGS) {
                Serial.println("[Button] Gateway returned no usable feedback");
            }
            break;
    }

    applyIndicators();
}

void IceTrackDevice::handleDirectives(const GatewayDirectives& incoming) {
    if (incoming.hasAlarm) {
        directives.hasAlarm = true;
        directives.alarmActive = incoming.alarmActive;

        // Silence is latched until the excursion ends.
        if (!incoming.alarmActive) {
            directives.buzzerMuted = false;
        }
    }

    if (incoming.hasBuzzerMuted) {
        directives.hasBuzzerMuted = true;
        directives.buzzerMuted = incoming.buzzerMuted;
    }

    if (incoming.hasSignalTest) {
        directives.hasSignalTest = true;
        directives.signalTest = incoming.signalTest;
    }

    if (incoming.hasMaintenance) {
        directives.hasMaintenance = true;
        directives.maintenanceActive = incoming.maintenanceActive;
        snprintf(directives.maintenanceOrigin,
                 sizeof(directives.maintenanceOrigin), "%s",
                 incoming.maintenanceOrigin);
        snprintf(directives.maintenanceExpiresAt,
                 sizeof(directives.maintenanceExpiresAt), "%s",
                 incoming.maintenanceExpiresAt);
    }

    const bool signalRequested =
        directives.hasSignalTest && directives.signalTest;

    if (signalRequested && !signalTestActive) {
        led.handle(StatusLed::SIGNAL_TEST_COMMAND);
        buzzer.handle(Buzzer::SIGNAL_TEST_COMMAND);
        if (AppConfig::ENABLE_SERIAL_LOGS) {
            Serial.println("[Signal] Diagnostic sequence requested by technician");
        }
    }
    signalTestActive = signalRequested;

    applyIndicators();
}

void IceTrackDevice::applyIndicators(bool force) {
    uint8_t desired = 0; // normal

    if (connectionLost()) {
        desired = 2;
    } else if (maintenanceActive()) {
        desired = 3;
    } else if (directives.hasAlarm && directives.alarmActive) {
        desired = 1;
    }

    if (force || desired != lastIndicator) {
        switch (desired) {
            case 2:
                led.handle(StatusLed::CONNECTION_LOST_COMMAND);
                break;
            case 3:
                led.handle(StatusLed::MAINTENANCE_COMMAND);
                break;
            case 1:
                led.handle(StatusLed::EXCURSION_COMMAND);
                break;
            case 0:
            default:
                led.handle(StatusLed::NORMAL_COMMAND);
                break;
        }

        if (AppConfig::ENABLE_SERIAL_LOGS && desired != lastIndicator) {
            Serial.printf("[Signal] LED state -> %s\n", indicatorName(desired));
        }
        lastIndicator = desired;
    }

    const bool alarmRequested =
        (desired == 1) &&
        !(directives.hasBuzzerMuted && directives.buzzerMuted);

    if (force || alarmRequested != lastBuzzerAlarm) {
        buzzer.handle(alarmRequested ? Buzzer::ALARM_ON_COMMAND
                                     : Buzzer::ALARM_OFF_COMMAND);
        lastBuzzerAlarm = alarmRequested;
    }
}

bool IceTrackDevice::connectionLost() const {
    return consecutiveFailures >= AppConfig::OFFLINE_FAILURE_THRESHOLD;
}

bool IceTrackDevice::maintenanceActive() const {
    return directives.hasMaintenance && directives.maintenanceActive;
}

const char* IceTrackDevice::indicatorName(uint8_t indicator) const {
    switch (indicator) {
        case 1: return "EXCURSION (blinking red)";
        case 2: return "CONNECTION_LOST (solid amber)";
        case 3: return "MAINTENANCE (solid blue)";
        case 0:
        default: return "NORMAL (solid green)";
    }
}
