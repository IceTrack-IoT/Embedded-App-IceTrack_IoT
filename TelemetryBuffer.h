#ifndef TELEMETRY_BUFFER_H
#define TELEMETRY_BUFFER_H

/**
 * @file TelemetryBuffer.h
 * @brief Fixed-size in-memory FIFO for readings captured during local link loss.
 *
 * US-26 Scenario 2 and NFR-05 require the node to buffer recent readings while
 * the gateway is unreachable and forward them once connectivity returns. The
 * buffer is intentionally in RAM only: the gateway owns durable storage.
 */

#include "AppConfig.h"
#include "Telemetry.h"

class TelemetryBuffer {
public:
    explicit TelemetryBuffer(uint16_t capacity = AppConfig::TELEMETRY_BUFFER_CAPACITY);

    /**
     * @brief Adds a reading. If the buffer is full, the oldest one is discarded.
     * @return true when a reading was stored, false when the capacity is zero.
     */
    bool push(const TelemetryReading& reading);

    /** @brief Removes the oldest reading. */
    bool pop();

    /** @brief Returns the oldest reading, or nullptr when empty. */
    const TelemetryReading* peek() const;

    uint16_t size() const { return count; }
    uint16_t capacity() const { return capacitySize; }
    bool isEmpty() const { return count == 0; }
    uint32_t droppedCount() const { return dropped; }

private:
    static const uint16_t MAX_CAPACITY = 512;

    TelemetryReading items[MAX_CAPACITY];
    uint16_t capacitySize;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint32_t dropped;
};

#endif // TELEMETRY_BUFFER_H
