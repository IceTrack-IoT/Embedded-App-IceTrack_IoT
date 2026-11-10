/**
* @file Sensor.cpp
 * @brief Implements the Sensor base class.
 *
 * Provides core functionality for sensors in the Modest IoT Nano-framework, including event
 * propagation to an assigned handler. Subclasses should configure hardware and define specific
 * event generation logic.
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

#include "Sensor.h"

Sensor::Sensor(int pin, EventHandler* eventHandler)
    : pin(pin), handler(eventHandler) {}

void Sensor::on(Event event) {
    if (handler != nullptr) {
        handler->on(event);
    }
}

void Sensor::setHandler(EventHandler* eventHandler) {
    handler = eventHandler;
}
