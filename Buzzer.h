#ifndef BUZZER_H
#define BUZZER_H

/**
 * @file Buzzer.h
 * @brief Non-blocking buzzer actuator for alarm and feedback patterns.
 *
 * Alarm tone is continuous while the edge reports an active excursion and the
 * buzzer is not muted. Short patterns confirm maintenance button gestures or
 * warning conditions. Muting silences the alarm until the
 * excursion ends.
 */

#include "Actuator.h"
#include <Arduino.h>

class Buzzer : public Actuator {
public:
    static const int ALARM_ON_COMMAND_ID = 210;
    static const int ALARM_OFF_COMMAND_ID = 211;
    static const int MUTE_COMMAND_ID = 212;
    static const int BEEP_ACTIVATED_COMMAND_ID = 213;
    static const int BEEP_DEACTIVATED_COMMAND_ID = 214;
    static const int BEEP_WARNING_COMMAND_ID = 215;
    static const int SIGNAL_TEST_COMMAND_ID = 216;

    static const Command ALARM_ON_COMMAND;
    static const Command ALARM_OFF_COMMAND;
    static const Command MUTE_COMMAND;
    static const Command BEEP_ACTIVATED_COMMAND;
    static const Command BEEP_DEACTIVATED_COMMAND;
    static const Command BEEP_WARNING_COMMAND;
    static const Command SIGNAL_TEST_COMMAND;

    Buzzer(int pin, CommandHandler* handler = nullptr);

    void begin();

    void handle(Command command) override;

    /** @brief Advances the active beep pattern. */
    void update();

    bool isMuted() const { return muted; }
    bool isAlarmRequested() const { return alarmRequested; }

private:
    enum Pattern : uint8_t {
        PATTERN_NONE = 0,
        PATTERN_ALARM,
        PATTERN_SINGLE,
        PATTERN_DOUBLE,
        PATTERN_TRIPLE
    };

    Pattern pattern;
    uint8_t stepIndex;
    bool outputOn;
    bool alarmRequested;
    bool muted;
    uint32_t stepStartedMs;

    void startPattern(Pattern newPattern);
    void finishPattern();
    void updateAlarmOutput();
    void writeOutput(bool on);
    void stopOutput();

    uint8_t stepCount(Pattern value) const;
    bool getStep(Pattern value, uint8_t index,
                 uint16_t& onMs, uint16_t& offMs) const;
};

#endif // BUZZER_H
