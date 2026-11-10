/**
 * @file PirSensor.cpp
 * @brief Implements the PIR presence adapter.
 */

#include "PirSensor.h"
#include "AppConfig.h"
#include <Arduino.h>

const Event PirSensor::PRESENCE_CHANGED_EVENT = Event(PRESENCE_CHANGED_EVENT_ID);

PirSensor::PirSensor(int pin, EventHandler* handler)
    : Sensor(pin, handler), presence(false), available(true), lastHighMs(0) {}

void PirSensor::begin() {
    pinMode(static_cast<uint8_t>(pin), INPUT);
    lastHighMs = millis();
}

void PirSensor::update() {
    const uint32_t now = millis();
    const bool high = digitalRead(static_cast<uint8_t>(pin)) == HIGH;

    if (high) {
        lastHighMs = now;
        if (!presence) {
            presence = true;
            Sensor::on(PRESENCE_CHANGED_EVENT);
        }
        return;
    }

    if (presence && (now - lastHighMs) >= AppConfig::PIR_HOLD_MS) {
        presence = false;
        Sensor::on(PRESENCE_CHANGED_EVENT);
    }
}
