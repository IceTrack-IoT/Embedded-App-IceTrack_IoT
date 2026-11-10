/**
 * @file Buzzer.cpp
 * @brief Implements the non-blocking buzzer patterns.
 */

#include "Buzzer.h"
#include "AppConfig.h"
#include <Arduino.h>

const Command Buzzer::ALARM_ON_COMMAND = Command(ALARM_ON_COMMAND_ID);
const Command Buzzer::ALARM_OFF_COMMAND = Command(ALARM_OFF_COMMAND_ID);
const Command Buzzer::MUTE_COMMAND = Command(MUTE_COMMAND_ID);
const Command Buzzer::BEEP_ACTIVATED_COMMAND = Command(BEEP_ACTIVATED_COMMAND_ID);
const Command Buzzer::BEEP_DEACTIVATED_COMMAND = Command(BEEP_DEACTIVATED_COMMAND_ID);
const Command Buzzer::BEEP_WARNING_COMMAND = Command(BEEP_WARNING_COMMAND_ID);
const Command Buzzer::SIGNAL_TEST_COMMAND = Command(SIGNAL_TEST_COMMAND_ID);

Buzzer::Buzzer(int pin, CommandHandler* handler)
    : Actuator(pin, handler),
      pattern(PATTERN_NONE),
      stepIndex(0),
      outputOn(false),
      alarmRequested(false),
      muted(false),
      stepStartedMs(0) {}

void Buzzer::begin() {
    pinMode(static_cast<uint8_t>(pin), OUTPUT);
    noTone(static_cast<uint8_t>(pin));
    outputOn = false;
}

void Buzzer::handle(Command command) {
    if (command == ALARM_ON_COMMAND) {
        alarmRequested = true;
        if (!muted && pattern == PATTERN_NONE) {
            startPattern(PATTERN_ALARM);
        }
    } else if (command == ALARM_OFF_COMMAND) {
        alarmRequested = false;
        muted = false;
        if (pattern == PATTERN_ALARM) {
            stopOutput();
            pattern = PATTERN_NONE;
        }
    } else if (command == MUTE_COMMAND) {
        muted = true;
        if (pattern == PATTERN_ALARM) {
            stopOutput();
            pattern = PATTERN_NONE;
        }
    } else if (command == BEEP_ACTIVATED_COMMAND) {
        startPattern(PATTERN_DOUBLE);
    } else if (command == BEEP_DEACTIVATED_COMMAND) {
        startPattern(PATTERN_SINGLE);
    } else if (command == BEEP_WARNING_COMMAND) {
        startPattern(PATTERN_TRIPLE);
    } else if (command == SIGNAL_TEST_COMMAND) {
        startPattern(PATTERN_DOUBLE);
    }

    Actuator::handle(command); // CQRS observer hook.
}

void Buzzer::update() {
    const uint32_t now = millis();

    if (pattern == PATTERN_ALARM) {
        updateAlarmOutput();
        return;
    }

    if (pattern == PATTERN_NONE) {
        return;
    }

    uint16_t onMs = 0;
    uint16_t offMs = 0;
    if (!getStep(pattern, stepIndex, onMs, offMs)) {
        finishPattern();
        return;
    }

    if (outputOn) {
        if ((now - stepStartedMs) >= onMs) {
            stopOutput();

            if (offMs == 0) {
                finishPattern();
            } else {
                stepStartedMs = now;
            }
        }
        return;
    }

    if ((now - stepStartedMs) >= offMs) {
        ++stepIndex;
        if (stepIndex >= stepCount(pattern)) {
            finishPattern();
        } else {
            stepStartedMs = now;
            writeOutput(true);
        }
    }
}

void Buzzer::startPattern(Pattern newPattern) {
    pattern = newPattern;
    stepIndex = 0;
    stepStartedMs = millis();

    if (newPattern == PATTERN_ALARM) {
        updateAlarmOutput();
    } else {
        writeOutput(true);
    }
}

void Buzzer::finishPattern() {
    pattern = PATTERN_NONE;
    stepIndex = 0;
    stopOutput();

    if (alarmRequested && !muted) {
        startPattern(PATTERN_ALARM);
    }
}

void Buzzer::updateAlarmOutput() {
    const bool shouldSound = alarmRequested && !muted;
    if (shouldSound != outputOn) {
        writeOutput(shouldSound);
    }
}

void Buzzer::writeOutput(bool on) {
    if (on) {
        tone(static_cast<uint8_t>(pin), AppConfig::BUZZER_TONE_HZ);
    } else {
        noTone(static_cast<uint8_t>(pin));
    }
    outputOn = on;
}

void Buzzer::stopOutput() {
    if (outputOn) {
        noTone(static_cast<uint8_t>(pin));
        outputOn = false;
    }
}

uint8_t Buzzer::stepCount(Pattern value) const {
    switch (value) {
        case PATTERN_SINGLE: return 1;
        case PATTERN_DOUBLE: return 2;
        case PATTERN_TRIPLE: return 3;
        default: return 0;
    }
}

bool Buzzer::getStep(Pattern value, uint8_t index,
                     uint16_t& onMs, uint16_t& offMs) const {
    if (value == PATTERN_SINGLE && index == 0) {
        onMs = 220;
        offMs = 0;
        return true;
    }

    if (value == PATTERN_DOUBLE && index < 2) {
        onMs = 180;
        offMs = (index == 0) ? 140 : 0;
        return true;
    }

    if (value == PATTERN_TRIPLE && index < 3) {
        onMs = 120;
        offMs = (index < 2) ? 110 : 0;
        return true;
    }

    return false;
}
