#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

/**
 * @file EventHandler.h
 * @brief Defines the Event structure and EventHandler interface.
 * 
 * This file provides the foundation for event-driven behavior in the Modest IoT Nano-framework.
 * The `Event` structure represents a unique event type, and `EventHandler` is an abstract
 * interface for classes that respond to events, enabling asynchronous reactivity in IoT devices.
 * 
 * @author Angel Velasquez
 * @date March 22, 2025
 * @version 0.1
 */

/*
 * This file is part of the Modest IoT Nano-framework (C++ Edition).
 * Copyright (c) 2025 Angel Velasquez
 *
 * Licensed under the Creative Commons Attribution-NoDerivatives 4.0 International (CC BY-ND 4.0).
 * You may use, copy, and distribute this software in its original, unmodified form, provided
 * you give appropriate credit to the original author (Angel Velasquez) and include this notice.
 * Modifications, adaptations, or derivative works are not permitted.
 * 
 * Full license text: https://creativecommons.org/licenses/by-nd/4.0/legalcode
 */

/**
 * @brief Represents an event with a unique identifier.
 * 
 * Events are lightweight structs used to signal occurrences (e.g., sensor triggers) within
 * the framework. Define custom events by assigning unique IDs in your application.
 */
struct Event {
    int id; ///< Unique identifier for the event type.

    explicit Event(int eventId) : id(eventId) {}
    bool operator==(const Event& other) const { return id == other.id; }
};

/**
 * @brief Abstract interface for handling events.
 * 
 * Implement this interface in classes that need to react to events. The `on` method is called
 * when an event occurs, allowing for custom handling logic.
 */
class EventHandler {
public:
    virtual void on(Event event) = 0; ///< Pure virtual method to handle an event.
    virtual ~EventHandler() = default; ///< Virtual destructor for safe inheritance.
};

#endif // EVENT_HANDLER_H