#ifndef TOGGABLE_LED_DEVICE_H
#define TOGGABLE_LED_DEVICE_H

/**
 * @file ToggableLedDevice.h
 * @brief Declares the ToggableLedDevice class.
 * 
 * An example implementation of the Modest IoT Nano-framework (C++ Edition), demonstrating a device
 * that toggles an LED based on button presses. Depends on the framework from
 * https://github.com/avelasquezn/Modest-IoT-Nano-framework-Cpp.
 * 
 * @author Angel Velasquez
 * @date March 22, 2025
 * @version 0.1
 */

/*
 * This file is part of the ToggableLedDevice-Example-Cpp project.
 * Copyright (c) 2025 Angel Velasquez
 *
 * Licensed under the Creative Commons Attribution-NoDerivatives 4.0 International (CC BY-ND 4.0).
 * You may use, copy, and distribute this software in its original, unmodified form, provided
 * you give appropriate credit to the original author (Angel Velasquez) and include this notice.
 * Modifications, adaptations, or derivative works are not permitted.
 * 
 * Full license text: https://creativecommons.org/licenses/by-nd/4.0/legalcode
 */

#include "Device.h"
#include "Button.h"
#include "Led.h"

class ToggableLedDevice : public Device {
private:
    Button button; ///< Button sensor instance from the framework.
    Led led; ///< LED actuator instance from the framework.

public:
    static const int BUTTON_PIN = 26; ///< Default GPIO pin for the button.
    static const int LED_PIN = 27; ///< Default GPIO pin for the LED.

    ToggableLedDevice(int buttonPin = BUTTON_PIN, int ledPin = LED_PIN, bool initialLedState = false); ///< Constructor with configurable pins and initial LED state.
    void on(Event event) override; ///< Handles button press events.
    void handle(Command command) override; ///< Handles LED command responses (e.g., state reporting).
    Led& getLed(); ///< Provides access to the LED instance.
    void triggerButtonEvent(Event event); ///< Triggers a button event externally (e.g., from ISR).
};

#endif // TOGGABLE_LED_DEVICE_H