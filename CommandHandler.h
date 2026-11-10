#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

/**
 * @file CommandHandler.h
 * @brief Defines the Command structure and CommandHandler interface.
 * 
 * This file establishes the foundation for command-driven behavior in the Modest IoT Nano-framework.
 * The `Command` structure represents an action to be performed, and `CommandHandler` is an abstract
 * interface for classes that process commands, supporting a CQRS-inspired design.
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
 * @brief Represents a command with a unique identifier.
 * 
 * Commands are lightweight structs used to instruct devices or actuators to perform actions.
 * Define custom commands by assigning unique IDs in your application.
 */
struct Command {
    int id; ///< Unique identifier for the command type.

    explicit Command(int commandId) : id(commandId) {}
    bool operator==(const Command& other) const { return id == other.id; }
};

/**
 * @brief Abstract interface for handling commands.
 * 
 * Implement this interface in classes that need to execute commands. The `handle` method is called
 * when a command is issued, allowing for custom execution logic.
 */
class CommandHandler {
public:
    virtual void handle(Command command) = 0; ///< Pure virtual method to process a command.
    virtual ~CommandHandler() = default; ///< Virtual destructor for safe inheritance.
};

#endif // COMMAND_HANDLER_H