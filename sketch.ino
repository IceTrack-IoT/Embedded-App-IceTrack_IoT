/**
 * @file sketch.ino
 * @brief IceTrack ESP32 refrigeration-monitoring node.
 *
 * Entry point for the embedded application. All behaviour lives in
 * IceTrackDevice, which composes the Modest IoT Nano-framework sensors,
 * actuators and device abstractions.
 *
 * Target board : ESP32 DevKit V1 (ESP32-WROOM-32)
 * Framework    : Modest IoT Nano-framework (C++ Edition) by Angel Velasquez
 * Scope        : Refrigeration above 0 °C with a DHT11 probe (0-50 °C)
 *
 * Required libraries:
 *   - "DHT sensor library for ESPx" (DHTesp)
 *   - "ArduinoJson" 7.x
 *
 * See README.md for wiring, provisioning, Wokwi and gateway contract details.
 */

#include "ModestIoT.h"
#include "IceTrackDevice.h"

IceTrackDevice device;

void setup() {
    device.begin();
}

void loop() {
    device.loop();
}
