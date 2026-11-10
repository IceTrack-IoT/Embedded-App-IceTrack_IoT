/**
 * @file Led.cpp
 * @brief Implements the Led class.
 *
 * Manages LED state changes in the Modest IoT Nano-framework, responding to predefined commands
 * (toggle, on, off) and updating the hardware pin accordingly. This serves as an example
 * implementation of an actuator.
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

#include "Led.h"
#include <Arduino.h>

const Command Led::TOGGLE_LED_COMMAND = Command(TOGGLE_LED_COMMAND_ID);
const Command Led::TURN_ON_COMMAND = Command(TURN_ON_COMMAND_ID);
const Command Led::TURN_OFF_COMMAND = Command(TURN_OFF_COMMAND_ID);

Led::Led(int pin, bool initialState, CommandHandler* commandHandler)
    : Actuator(pin, commandHandler), state(initialState) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state);
}

void Led::handle(Command command) {
    if (command == TOGGLE_LED_COMMAND) {
        state = !state;
        digitalWrite(pin, state);
    } else if (command == TURN_ON_COMMAND) {
        state = true;
        digitalWrite(pin, state);
    } else if (command == TURN_OFF_COMMAND) {
        state = false;
        digitalWrite(pin, state);
    }
    Actuator::handle(command); // Propagate to handler if set
}

bool Led::getState() const {
    return state;
}

void Led::setState(bool newState) {
    state = newState;
    digitalWrite(pin, state);
}