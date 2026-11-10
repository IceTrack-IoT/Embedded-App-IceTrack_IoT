#ifndef MAINTENANCE_BUTTON_H
#define MAINTENANCE_BUTTON_H

/**
 * @file MaintenanceButton.h
 * @brief Physical maintenance button with a 3-second long-press gesture.
 *
 * Extends the framework Button sensor. Short presses are ignored; a press held
 * for BUTTON_LONG_PRESS_MS emits LONG_PRESS_EVENT exactly once. The
 * actual maintenance-window decision belongs to the edge gateway.
 */

#include "Button.h"
#include <Arduino.h>

class MaintenanceButton : public Button {
public:
    static const int LONG_PRESS_EVENT_ID = 130;
    static const Event LONG_PRESS_EVENT;

    MaintenanceButton(int pin, EventHandler* handler = nullptr);

    /** @brief Captures the initial pin state to avoid a false press at boot. */
    void begin();

    /** @brief Non-blocking debounce and long-press detector. */
    void update();

    bool isPressed() const { return stablePressed; }

private:
    bool rawPressed;
    bool stablePressed;
    bool longPressFired;
    uint32_t lastRawChangeMs;
    uint32_t pressStartedMs;
};

#endif // MAINTENANCE_BUTTON_H
