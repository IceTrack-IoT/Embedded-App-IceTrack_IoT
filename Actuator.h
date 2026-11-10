#ifndef ACTUATOR_H
#define ACTUATOR_H

/**
 * @file Actuator.h
 * @brief Declares the Actuator base class.
 * 
 * This abstract base class represents output devices in the Modest IoT Nano-framework, providing
 * a foundation for actuators that respond to commands. It includes command propagation to an
 * optional handler, supporting the framework’s CQRS-inspired design.
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

#include "CommandHandler.h"

class Actuator : public CommandHandler {
protected:
    int pin; ///< GPIO pin assigned to the actuator.
    CommandHandler* handler; ///< Optional handler to receive propagated commands.

public:
    /**
     * @brief Constructs an Actuator with a pin and optional command handler.
     * @param pin The GPIO pin for the actuator.
     * @param commandHandler Pointer to a CommandHandler to receive commands (default: nullptr).
     */
    Actuator(int pin, CommandHandler* commandHandler = nullptr);

    /**
     * @brief Handles a command by propagating it to the assigned handler.
     * @param command The command to handle.
     */
    void handle(Command command) override;

    /**
     * @brief Sets or updates the command handler for this actuator.
     * @param commandHandler Pointer to the new CommandHandler.
     */
    void setHandler(CommandHandler* commandHandler);
};

#endif // ACTUATOR_H