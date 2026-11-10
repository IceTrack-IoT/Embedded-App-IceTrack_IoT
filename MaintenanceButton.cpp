/**
 * @file MaintenanceButton.cpp
 * @brief Implements the long-press maintenance button.
 */

#include "MaintenanceButton.h"
#include "AppConfig.h"
#include <Arduino.h>

const Event MaintenanceButton::LONG_PRESS_EVENT = Event(LONG_PRESS_EVENT_ID);

MaintenanceButton::MaintenanceButton(int pin, EventHandler* handler)
    : Button(pin, handler),
      rawPressed(false),
      stablePressed(false),
      longPressFired(false),
      lastRawChangeMs(0),
      pressStartedMs(0) {}

void MaintenanceButton::begin() {
    const bool pressed = digitalRead(static_cast<uint8_t>(pin)) == LOW;
    rawPressed = pressed;
    stablePressed = pressed;
    longPressFired = false;
    lastRawChangeMs = millis();
    pressStartedMs = millis();
}

void MaintenanceButton::update() {
    const uint32_t now = millis();
    const bool pressed = digitalRead(static_cast<uint8_t>(pin)) == LOW;

    if (pressed != rawPressed) {
        rawPressed = pressed;
        lastRawChangeMs = now;
    }

    // Wait for the contact to be stable before accepting a new level.
    if ((now - lastRawChangeMs) < AppConfig::BUTTON_DEBOUNCE_MS) {
        return;
    }

    if (rawPressed != stablePressed) {
        stablePressed = rawPressed;

        if (stablePressed) {
            pressStartedMs = now;
            longPressFired = false;
        } else {
            longPressFired = false;
        }
    }

    if (stablePressed && !longPressFired &&
        (now - pressStartedMs) >= AppConfig::BUTTON_LONG_PRESS_MS) {
        longPressFired = true;
        Sensor::on(LONG_PRESS_EVENT);
    }
}
