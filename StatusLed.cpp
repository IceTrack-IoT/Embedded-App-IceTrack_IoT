/**
 * @file StatusLed.cpp
 * @brief Implements the RGB status indicator.
 */

#include "StatusLed.h"
#include "AppConfig.h"
#include <Arduino.h>

const Command StatusLed::NORMAL_COMMAND = Command(NORMAL_COMMAND_ID);
const Command StatusLed::EXCURSION_COMMAND = Command(EXCURSION_COMMAND_ID);
const Command StatusLed::CONNECTION_LOST_COMMAND = Command(CONNECTION_LOST_COMMAND_ID);
const Command StatusLed::MAINTENANCE_COMMAND = Command(MAINTENANCE_COMMAND_ID);
const Command StatusLed::OFF_COMMAND = Command(OFF_COMMAND_ID);
const Command StatusLed::SIGNAL_TEST_COMMAND = Command(SIGNAL_TEST_COMMAND_ID);

StatusLed::StatusLed(int redPin, int greenPin, int bluePin,
                     CommandHandler* handler)
    : Actuator(redPin, handler),
      greenPin(greenPin),
      bluePin(bluePin),
      mode(MODE_NORMAL),
      modeBeforeTest(MODE_NORMAL),
      blinkPhase(true),
      lastBlinkMs(0),
      testStartedMs(0),
      colorInitialized(false),
      lastRed(false),
      lastGreen(false),
      lastBlue(false) {}

void StatusLed::begin() {
    pinMode(static_cast<uint8_t>(pin), OUTPUT);
    pinMode(static_cast<uint8_t>(greenPin), OUTPUT);
    pinMode(static_cast<uint8_t>(bluePin), OUTPUT);

    lastRed = false;
    lastGreen = false;
    lastBlue = false;
    colorInitialized = true;

    setMode(MODE_NORMAL);
}

void StatusLed::handle(Command command) {
    if (command == NORMAL_COMMAND) {
        setMode(MODE_NORMAL);
    } else if (command == EXCURSION_COMMAND) {
        setMode(MODE_EXCURSION);
    } else if (command == CONNECTION_LOST_COMMAND) {
        setMode(MODE_CONNECTION_LOST);
    } else if (command == MAINTENANCE_COMMAND) {
        setMode(MODE_MAINTENANCE);
    } else if (command == OFF_COMMAND) {
        setMode(MODE_OFF);
    } else if (command == SIGNAL_TEST_COMMAND) {
        if (mode != MODE_SIGNAL_TEST) {
            modeBeforeTest = mode;
            mode = MODE_SIGNAL_TEST;
            testStartedMs = millis();
            lastBlinkMs = testStartedMs;
        }
    }

    Actuator::handle(command); // CQRS observer hook.
}

void StatusLed::update() {
    const uint32_t now = millis();

    if (mode == MODE_SIGNAL_TEST) {
        const uint32_t elapsed = now - testStartedMs;
        if (elapsed >= 3000UL) {
            mode = (modeBeforeTest == MODE_SIGNAL_TEST) ? MODE_NORMAL : modeBeforeTest;
            blinkPhase = true;
            applyModeColor();
            return;
        }

        // Rapid red -> green -> blue -> amber -> white sequence.
        const uint8_t step = static_cast<uint8_t>((elapsed / 600UL) % 5UL);
        switch (step) {
            case 0: applyColor(true, false, false); break;
            case 1: applyColor(false, true, false); break;
            case 2: applyColor(false, false, true); break;
            case 3: applyColor(true, true, false); break;
            default: applyColor(true, true, true); break;
        }
        return;
    }

    if (mode == MODE_EXCURSION) {
        if ((now - lastBlinkMs) >= 500UL) {
            lastBlinkMs = now;
            blinkPhase = !blinkPhase;
            applyColor(blinkPhase, false, false);
        }
        return;
    }

    applyModeColor();
}

void StatusLed::setMode(Mode newMode) {
    mode = newMode;
    blinkPhase = true;
    lastBlinkMs = millis();
    applyModeColor();
}

void StatusLed::applyModeColor() {
    switch (mode) {
        case MODE_NORMAL:
            applyColor(false, true, false); // solid green
            break;
        case MODE_EXCURSION:
            applyColor(blinkPhase, false, false); // blinking red
            break;
        case MODE_CONNECTION_LOST:
            applyColor(true, true, false); // solid amber
            break;
        case MODE_MAINTENANCE:
            applyColor(false, false, true); // solid blue
            break;
        case MODE_OFF:
        default:
            applyColor(false, false, false);
            break;
    }
}

void StatusLed::applyColor(bool red, bool green, bool blue) {
    if (colorInitialized && red == lastRed && green == lastGreen && blue == lastBlue) {
        return;
    }

    writeChannel(pin, red);
    writeChannel(greenPin, green);
    writeChannel(bluePin, blue);

    lastRed = red;
    lastGreen = green;
    lastBlue = blue;
    colorInitialized = true;
}

void StatusLed::writeChannel(int channelPin, bool on) {
    const bool level = AppConfig::RGB_COMMON_ANODE ? !on : on;
    digitalWrite(channelPin, level ? HIGH : LOW);
}
