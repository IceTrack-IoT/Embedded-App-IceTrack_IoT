#ifndef DEVICE_H
#define DEVICE_H

/**
 * @file Device.h
 * @brief Declares the Device interface.
 *
 * This abstract base class in the Modest IoT Nano-framework combines event and command handling,
 * serving as the foundation for complete IoT devices that integrate sensors and actuators.
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

#include "EventHandler.h"
#include "CommandHandler.h"

class Device : public EventHandler, public CommandHandler {
public:
    /**
     * @brief Handles an event received by the device.
     * @param event The event to process.
     */
    virtual void on(Event event) override = 0;

    /**
     * @brief Handles a command issued to the device.
     * @param command The command to execute.
     */
    virtual void handle(Command command) override = 0;

    virtual ~Device() = default; ///< Virtual destructor for safe inheritance.
};

#endif // DEVICE_H
