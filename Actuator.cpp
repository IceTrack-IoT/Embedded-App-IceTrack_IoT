/**
* @file Actuator.cpp
 * @brief Implements the Actuator base class.
 *
 * Provides core functionality for actuators in the Modest IoT Nano-framework, including command
 * propagation to an assigned handler. Subclasses should configure hardware and define specific
 * command execution logic.
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

#include "Actuator.h"

Actuator::Actuator(int pin, CommandHandler* commandHandler)
    : pin(pin), handler(commandHandler) {}

void Actuator::handle(Command command) {
    if (handler != nullptr) {
        handler->handle(command);
    }
}

void Actuator::setHandler(CommandHandler* commandHandler) {
    handler = commandHandler;
}
