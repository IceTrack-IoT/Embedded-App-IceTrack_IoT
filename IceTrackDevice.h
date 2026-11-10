#ifndef ICE_TRACK_DEVICE_H
#define ICE_TRACK_DEVICE_H

/**
 * @file IceTrackDevice.h
 * @brief IceTrack refrigeration-monitoring node built on the Modest IoT framework.
 *
 * The class is the concrete Device of the application. It composes framework
 * sensors (DHT11, LDR, PIR, maintenance button) and actuators (RGB status LED,
 * buzzer), then applies the CQRS-inspired flow:
 *
 *   Sensor --Event--> IceTrackDevice --Command--> Actuator
 *
 * Telemetry is sampled every 10 s, buffered in RAM and transmitted to the
 * on-site gateway over the LAN. Gateway directives drive the physical
 * signalling.
 */

#include "Device.h"
#include "Buzzer.h"
#include "Dht11Sensor.h"
#include "GatewayClient.h"
#include "LightSensor.h"
#include "MaintenanceButton.h"
#include "PirSensor.h"
#include "StatusLed.h"
#include "TelemetryBuffer.h"

class IceTrackDevice : public Device {
public:
    IceTrackDevice();

    /** @brief Initializes hardware, networking and the initial green state. */
    void begin();

    /** @brief Cooperative main loop; never blocks longer than an HTTP timeout. */
    void loop();

    void on(Event event) override;
    void handle(Command command) override;

private:
    Dht11Sensor dht;
    LightSensor light;
    PirSensor pir;
    MaintenanceButton button;
    StatusLed led;
    Buzzer buzzer;
    GatewayClient gateway;
    TelemetryBuffer buffer;

    GatewayDirectives directives;

    uint32_t bootId;
    uint32_t sequence;
    uint32_t buttonSequence;
    uint32_t lastSampleMs;
    uint32_t nextSendMs;
    uint32_t backoffMs;
    uint8_t consecutiveFailures;

    bool timeSynced;
    bool ntpStarted;
    bool lastDhtValid;
    bool lastPresence;
    bool signalTestActive;
    uint8_t lastIndicator;
    bool lastBuzzerAlarm;

    void updateClock();
    void sampleSensors();
    void attemptSend();
    void registerFailure(const char* reason);
    void handleButtonLongPress();
    void handleDirectives(const GatewayDirectives& incoming);
    void applyIndicators(bool force = false);
    bool connectionLost() const;
    bool maintenanceActive() const;
    const char* indicatorName(uint8_t indicator) const;
};

#endif // ICE_TRACK_DEVICE_H
