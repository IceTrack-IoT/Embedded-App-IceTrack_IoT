/**
 * @file TelemetryBuffer.cpp
 * @brief Implements the fixed-size telemetry ring buffer.
 */

#include "TelemetryBuffer.h"

TelemetryBuffer::TelemetryBuffer(uint16_t capacity)
    : capacitySize(capacity > MAX_CAPACITY ? MAX_CAPACITY : capacity),
      head(0),
      tail(0),
      count(0),
      dropped(0) {}

bool TelemetryBuffer::push(const TelemetryReading& reading) {
    if (capacitySize == 0) {
        return false;
    }

    if (count == capacitySize) {
        // Keep the most recent data: overwrite the oldest entry.
        head = static_cast<uint16_t>((head + 1) % capacitySize);
        --count;
        ++dropped;
    }

    items[tail] = reading;
    tail = static_cast<uint16_t>((tail + 1) % capacitySize);
    ++count;
    return true;
}

bool TelemetryBuffer::pop() {
    if (count == 0) {
        return false;
    }

    head = static_cast<uint16_t>((head + 1) % capacitySize);
    --count;
    return true;
}

const TelemetryReading* TelemetryBuffer::peek() const {
    return count == 0 ? nullptr : &items[head];
}
