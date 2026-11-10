#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

/**
 * @file LightSensor.h
 * @brief LDR module adapter used for door-open detection context.
 *
 * The photoresistor module outputs a higher voltage in darkness. The adapter
 * converts the raw ADC value to a 0-100 % light level, where 100 % means a
 * brightly lit (open) compartment and 0 % means darkness (closed door).
 * Debounce and door duration policies live in the edge gateway (TS-31/TS-32).
 */

#include "Sensor.h"

class LightSensor : public Sensor {
public:
    static const int LIGHT_READING_EVENT_ID = 110;
    static const Event LIGHT_READING_EVENT;

    LightSensor(int pin, EventHandler* handler = nullptr);

    /** @brief Configures the ADC input. */
    void begin();

    /** @brief Samples the LDR and emits LIGHT_READING_EVENT. */
    void update();

    int getPercent() const { return lightPercent; }
    int getRaw() const { return rawValue; }

private:
    int rawValue;
    int lightPercent;
    bool initialized;
};

#endif // LIGHT_SENSOR_H
