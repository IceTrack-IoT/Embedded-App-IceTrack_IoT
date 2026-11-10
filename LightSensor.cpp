/**
 * @file LightSensor.cpp
 * @brief Implements the LDR light-level adapter.
 */

#include "LightSensor.h"
#include "AppConfig.h"
#include <Arduino.h>

const Event LightSensor::LIGHT_READING_EVENT = Event(LIGHT_READING_EVENT_ID);

LightSensor::LightSensor(int pin, EventHandler* handler)
    : Sensor(pin, handler), rawValue(0), lightPercent(0), initialized(false) {}

void LightSensor::begin() {
    pinMode(static_cast<uint8_t>(pin), INPUT);
    analogReadResolution(12);
}

void LightSensor::update() {
    rawValue = analogRead(static_cast<uint8_t>(pin));
    rawValue = constrain(rawValue, 0, 4095);

    // The module drives a higher voltage when the LDR is dark, so the
    // percentage is inverted: 0 raw -> 100 % light, 4095 raw -> 0 % light.
    int instantaneous = map(rawValue, 0, 4095, 100, 0);
    instantaneous = constrain(instantaneous, 0, 100);

    if (!initialized) {
        lightPercent = instantaneous;
        initialized = true;
    } else {
        // Light exponential smoothing reduces single-sample flicker without
        // hiding a real door opening.
        lightPercent = (lightPercent * 3 + instantaneous + 2) / 4;
    }

    Sensor::on(LIGHT_READING_EVENT);
}
