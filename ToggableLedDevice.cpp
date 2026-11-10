/**
 * @file ToggableLedDevice.cpp
 * @brief Implements the ToggableLedDevice class.
 * 
 * Demonstrates LED toggling in the Modest IoT Nano-framework (C++ Edition).
 * 
 * @author Angel Velasquez
 * @date March 22, 2025
 * @version 0.1
 */

/*
 * This file is part of the Modest IoT Nano-framework (C++ Edition).
 * Copyright (c) 2025 Angel Velasquez
 *
 * Licensed under the Creative Commons Attribution-NoDerivatives 4.0 International (CC BY-ND 4.0).
 * You may use, copy, and distribute this software in its original, unmodified form, provided
 * you give appropriate credit to the original author (Angel Velasquez) and include this notice.
 * Modifications, adaptations, or derivative works are not permitted.
 * 
 * Full license text: https://creativecommons.org/licenses/by-nd/4.0/legalcode
 */

#include "ToggableLedDevice.h"
#include <Arduino.h>

ToggableLedDevice::ToggableLedDevice(int buttonPin, int ledPin, bool initialLedState) 
    : button(buttonPin, this), led(ledPin, initialLedState, this) {}

void ToggableLedDevice::on(Event event) {
    if (event == Button::BUTTON_PRESSED_EVENT) {
        led.handle(Led::TOGGLE_LED_COMMAND);
    }
}

void ToggableLedDevice::handle(Command command) {
    if (command == Led::TOGGLE_LED_COMMAND || 
        command == Led::TURN_ON_COMMAND || 
        command == Led::TURN_OFF_COMMAND) {
        Serial.printf("LED state: %d\n", led.getState());
    }
}

Led& ToggableLedDevice::getLed() {
    return led;
}

void ToggableLedDevice::triggerButtonEvent(Event event) {
    button.on(event);
}