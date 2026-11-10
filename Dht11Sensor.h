#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

/**
 * @file Dht11Sensor.h
 * @brief DHT11 temperature/humidity sensor adapter for the Modest IoT framework.
 *
 * Extends Sensor, validates the reading against the DHT11 physical envelope
 * (0-50 °C, 20-90 % RH) and emits an event whenever a sample is available.
 * Faulty samples are never used as measurements; the last valid value is kept
 * so the gateway can still receive a heartbeat flagged with sensorValid=false.
 */

#include "Sensor.h"
#include <Arduino.h>
#include <DHTesp.h>

class Dht11Sensor : public Sensor {
public:
    static const int READING_EVENT_ID = 100;
    static const Event READING_EVENT;
    static const int READ_FAILED_EVENT_ID = 101;
    static const Event READ_FAILED_EVENT;

    Dht11Sensor(int pin, EventHandler* handler = nullptr);

    /** @brief Starts the DHT driver (auto-detects DHT11/DHT22 in simulation). */
    void begin();

    /**
     * @brief Performs one sample.
     * @return true when a plausible reading was obtained.
     */
    bool update();

    float getTemperatureC() const;
    float getHumidityPct() const;
    bool isLastReadingValid() const { return lastReadingValid; }
    bool hasValidReading() const { return hasEverRead; }
    uint32_t getConsecutiveFailures() const { return consecutiveFailures; }

private:
    DHTesp dht;
    bool started;
    bool lastReadingValid;
    bool hasEverRead;
    float lastValidTemperatureC;
    float lastValidHumidityPct;
    uint32_t consecutiveFailures;
};

#endif // DHT11_SENSOR_H
