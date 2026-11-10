#ifndef STATUS_LED_H
#define STATUS_LED_H

/**
 * @file StatusLed.h
 * @brief RGB status indicator for the IceTrack node.
 *
 * Colour contract:
 *   - solid green  : normal operation
 *   - blinking red : sustained thermal excursion / alarm
 *   - solid amber  : local gateway unreachable after three failed attempts
 *   - solid blue   : maintenance window active
 *
 * The actuator is non-blocking: update() advances the blink and diagnostic
 * sequence from the main loop.
 */

#include "Actuator.h"
#include <Arduino.h>

class StatusLed : public Actuator {
public:
    enum Mode : uint8_t {
        MODE_OFF = 0,
        MODE_NORMAL,
        MODE_EXCURSION,
        MODE_CONNECTION_LOST,
        MODE_MAINTENANCE,
        MODE_SIGNAL_TEST
    };

    static const int NORMAL_COMMAND_ID = 200;
    static const int EXCURSION_COMMAND_ID = 201;
    static const int CONNECTION_LOST_COMMAND_ID = 202;
    static const int MAINTENANCE_COMMAND_ID = 203;
    static const int OFF_COMMAND_ID = 204;
    static const int SIGNAL_TEST_COMMAND_ID = 205;

    static const Command NORMAL_COMMAND;
    static const Command EXCURSION_COMMAND;
    static const Command CONNECTION_LOST_COMMAND;
    static const Command MAINTENANCE_COMMAND;
    static const Command OFF_COMMAND;
    static const Command SIGNAL_TEST_COMMAND;

    StatusLed(int redPin, int greenPin, int bluePin,
              CommandHandler* handler = nullptr);

    void begin();

    void handle(Command command) override;

    /** @brief Advances blink/diagnostic animations. */
    void update();

    Mode getMode() const { return mode; }

private:
    int greenPin;
    int bluePin;
    Mode mode;
    Mode modeBeforeTest;
    bool blinkPhase;
    uint32_t lastBlinkMs;
    uint32_t testStartedMs;
    bool colorInitialized;
    bool lastRed;
    bool lastGreen;
    bool lastBlue;

    void setMode(Mode newMode);
    void applyModeColor();
    void applyColor(bool red, bool green, bool blue);
    void writeChannel(int channelPin, bool on);
};

#endif // STATUS_LED_H
