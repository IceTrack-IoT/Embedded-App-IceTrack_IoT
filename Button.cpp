/**
* @file Button.cpp
 * @brief Implements the Button class.
 *
 * Configures a button as an input device in the Modest IoT Nano-framework, setting up the pin
 * with an internal pull-up resistor. Event generation (e.g., BUTTON_PRESSED_EVENT) is typically
 * triggered externally via interrupt or polling in user code.
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

#include "Button.h"
#include <Arduino.h>

const Event Button::BUTTON_PRESSED_EVENT = Event(BUTTON_PRESSED_EVENT_ID);

Button::Button(int pin, EventHandler* eventHandler)
    : Sensor(pin, eventHandler) {
    pinMode(pin, INPUT_PULLUP);
}
