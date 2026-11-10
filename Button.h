#ifndef BUTTON_H
#define BUTTON_H

/**
 * @file Button.h
 * @brief Declares the Button class.
 * 
 * A concrete sensor class in the Modest IoT Nano-framework for detecting button presses and
 * generating events. It serves as an example of extending the `Sensor` base class for input devices.
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

class Button : public Sensor {
public:
    static const int BUTTON_PRESSED_EVENT_ID = 0; ///< Unique ID for button press event.
    static const Event BUTTON_PRESSED_EVENT; ///< Predefined event for button presses.

    /**
     * @brief Constructs a Button sensor.
     * @param pin The GPIO pin for the button (configured as INPUT_PULLUP).
     * @param eventHandler Optional handler to receive button events (default: nullptr).
     */
    Button(int pin, EventHandler* eventHandler = nullptr);
};

#endif // BUTTON_H