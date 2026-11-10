#ifndef PIR_SENSOR_H
#define PIR_SENSOR_H

/**
 * @file PirSensor.h
 * @brief PIR presence adapter for the Modest IoT framework.
 *
 * Only a boolean presence flag is stored: no image, identity or biometric data
 * is ever handled. The gateway combines this flag with door
 * openings to provide context for alerts.
 */

#include "Sensor.h"
#include <Arduino.h>

class PirSensor : public Sensor {
public:
    static const int PRESENCE_CHANGED_EVENT_ID = 120;
    static const Event PRESENCE_CHANGED_EVENT;

    PirSensor(int pin, EventHandler* handler = nullptr);

    void begin();

    /** @brief Samples the PIR output and applies a short hold window. */
    void update();

    bool isPresent() const { return presence; }

    /**
     * @brief Whether the PIR can provide a meaningful value.
     *
     * The ESP32 cannot detect a disconnected PIR by itself; the gateway marks
     * presence as unavailable when telemetry omits the field. The firmware
     * keeps this flag so the contract can evolve without changing consumers.
     */
    bool isAvailable() const { return available; }

private:
    bool presence;
    bool available;
    uint32_t lastHighMs;
};

#endif // PIR_SENSOR_H
