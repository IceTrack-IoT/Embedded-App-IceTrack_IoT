/**
 * @file Dht11Sensor.cpp
 * @brief Implements the DHT11 sensor adapter.
 */

#include "Dht11Sensor.h"
#include "AppConfig.h"
#include <Arduino.h>
#include <math.h>

const Event Dht11Sensor::READING_EVENT = Event(READING_EVENT_ID);
const Event Dht11Sensor::READ_FAILED_EVENT = Event(READ_FAILED_EVENT_ID);

Dht11Sensor::Dht11Sensor(int pin, EventHandler* handler)
    : Sensor(pin, handler),
      started(false),
      lastReadingValid(false),
      hasEverRead(false),
      lastValidTemperatureC(AppConfig::DHT_FALLBACK_TEMPERATURE_C),
      lastValidHumidityPct(AppConfig::DHT_FALLBACK_HUMIDITY_PCT),
      consecutiveFailures(0) {}

void Dht11Sensor::begin() {
    if (started) {
        return;
    }

    // AUTO_DETECT tries DHT22 first and falls back to DHT11 on timeout. This
    // lets the same firmware run against a real DHT11 and the Wokwi DHT22 part.
    dht.setup(static_cast<uint8_t>(pin), DHTesp::AUTO_DETECT);
    started = true;

    if (AppConfig::ENABLE_SERIAL_LOGS) {
        Serial.printf("[DHT11] Driver ready on GPIO%d (auto-detect)\n", pin);
    }
}

bool Dht11Sensor::update() {
    if (!started) {
        return false;
    }

    TempAndHumidity sample = dht.getTempAndHumidity();

    const bool statusOk = (dht.getStatus() == DHTesp::ERROR_NONE);
    const bool finite = !isnan(sample.temperature) && !isnan(sample.humidity);
    const bool inRange =
        sample.temperature >= AppConfig::MIN_VALID_TEMPERATURE_C &&
        sample.temperature <= AppConfig::MAX_VALID_TEMPERATURE_C &&
        sample.humidity >= AppConfig::MIN_VALID_HUMIDITY_PCT &&
        sample.humidity <= AppConfig::MAX_VALID_HUMIDITY_PCT;

    lastReadingValid = statusOk && finite && inRange;

    if (lastReadingValid) {
        lastValidTemperatureC = sample.temperature;
        lastValidHumidityPct = sample.humidity;
        hasEverRead = true;
        consecutiveFailures = 0;
        Sensor::on(READING_EVENT);
    } else {
        if (consecutiveFailures < 0xFFFFFFFFUL) {
            ++consecutiveFailures;
        }
        Sensor::on(READ_FAILED_EVENT);
    }

    return lastReadingValid;
}

float Dht11Sensor::getTemperatureC() const {
    return hasEverRead ? lastValidTemperatureC : AppConfig::DHT_FALLBACK_TEMPERATURE_C;
}

float Dht11Sensor::getHumidityPct() const {
    return hasEverRead ? lastValidHumidityPct : AppConfig::DHT_FALLBACK_HUMIDITY_PCT;
}
