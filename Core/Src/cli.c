/**
  ******************************************************************************
  * @file    cli.c
  * @brief   CLI facility
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 ALI CHOUCHENE.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
 ******************************************************************************
 */

#include "string.h"
#include "stdbool.h"
#include "cli.h"
#include "serial.h"

#define CLI_MAX_COMMAND_LENGTH     64    // Maximum length of a CLI command
#define CLI_RX_READ_BUFFER_SIZE    256   // Size of the temporary RX read buffer

const char _unknown[] = "unknown\r\n";   // Response for an unrecognized command

typedef struct {
    const cli_command_t* table;                     // CLI command table
    uint8_t entry_count;                            // Number of registered commands
    const serial_interface_t* serial_interface;     // Serial interface instance
    char cmd[CLI_MAX_COMMAND_LENGTH];               // Command being assembled
    uint8_t cmd_len;                                // Current command length
    bool cmd_overflow;                              // Command buffer overflow flag
    bool skip_lf;                                   // Skip LF after CR
    bool running;                                   // CLI running state
} cli_ctx_t;

static cli_ctx_t _cli_ctx;

/******************************************************************************/
/*                         CLI exported API                                   */
/******************************************************************************/

// CLI initialization
bool cli_init(const cli_command_t* table, size_t entry_count)
{
    // Sanity
    if (!table) {
        return false;
    }

    // Sanity
    if ((!entry_count) || (entry_count > CLI_MAX_COMMAND_COUNT)) {
        return false;
    }

    // Copy table reference, should be static
    _cli_ctx.table = table;
    _cli_ctx.entry_count = entry_count;

    // Initialize command state
    _cli_ctx.cmd_len = 0;
    _cli_ctx.cmd_overflow = false;
    _cli_ctx.skip_lf = false;

    // Open the serial interface
    _cli_ctx.serial_interface = serial_interface_init();
    _cli_ctx.running = true;
    _cli_ctx.serial_interface->open(NULL);

    return true;
}

// CLI print
void cli_print(const char* str)
{
    // Sanity
    if (!str) {
        return;
    }

    // Check if the CLI is running
    if (!_cli_ctx.running) {
        return;
    }

    // Print the str
    while (*str != '\0') {
        _cli_ctx.serial_interface->write((const uint8_t*)str, 1);
        str++;
    }
}

// CLI command processing
void cli_run(void)
{
    uint8_t data[CLI_RX_READ_BUFFER_SIZE];
    size_t len = 0;
    bool found;

    // Check if the CLI is running
    if (!_cli_ctx.running) {
        return;
    }

    // Read available data from the serial interface
    _cli_ctx.serial_interface->read(data, &len);

    // Process received data
    for (size_t j = 0; j < len; j++) {

        // Skip LF
        if (_cli_ctx.skip_lf) {
            _cli_ctx.skip_lf = false;

            if (data[j] == '\n') {
                continue;
            }
        }

        // Check LF & CR
        if ((data[j] == '\r') || (data[j] == '\n')) {

            // Skip LF
            if (data[j] == '\r') {
                _cli_ctx.skip_lf = true;
            }

            // Isolate the command
            _cli_ctx.cmd[_cli_ctx.cmd_len] = '\0';

            // Echo a new line
            cli_print("\r\n");

            // Possible command received
            if (_cli_ctx.cmd_len > 0 || _cli_ctx.cmd_overflow) {

                found = false;

                // Loop the user command entry table
                if (!_cli_ctx.cmd_overflow) {
                    for (size_t i = 0; i < _cli_ctx.entry_count; i++) {

                        // Ignore command entries without a name
                        if (!_cli_ctx.table[i].name) {
                            continue;
                        }

                        // Search for a command matching the received input
                        if (strcmp(_cli_ctx.cmd, _cli_ctx.table[i].name) == 0) {

                            // Found!
                            found = true;

                            // Execute the associated command handler
                            if (_cli_ctx.table[i].action) {
                                _cli_ctx.table[i].action();
                            }

                            break;
                        }
                    }
                }

                // Report an unknown command if no match was found
                if (!found) {
                    cli_print(_unknown);
                }
            }

            // Clear the command state before receiving the next command
            _cli_ctx.cmd_len = 0;
            _cli_ctx.cmd_overflow = false;

            // Print the prompt for the next command
            cli_print("->");

            // Stop processing if the CLI was suspended or closed by a command
            if (!_cli_ctx.running) {
                return;
            }

            continue;
        }

        // Build the command from incoming characters
        if (_cli_ctx.cmd_len < (CLI_MAX_COMMAND_LENGTH - 1)) {
            _cli_ctx.cmd[_cli_ctx.cmd_len++] = (char)data[j];
        }
        else {
            // Discard the command if it exceeds the maximum length
            _cli_ctx.cmd_overflow = true;
        }
    }
}

// Suspend the CLI
void cli_suspend(void)
{
    // Check if the CLI is suspended
    if (!_cli_ctx.running) {
        return;
    }

    // Suspend it
    _cli_ctx.running = false;
    _cli_ctx.serial_interface->close();
}

// Resume the CLI
void cli_resume(void)
{
    // Check if the CLI is running
    if (_cli_ctx.running) {
        return;
    }

    // Re-init command state
    _cli_ctx.cmd_len = 0;
    _cli_ctx.cmd_overflow = false;
    _cli_ctx.skip_lf = false;

    // Reopen the serial interface
    _cli_ctx.serial_interface = serial_interface_init();
    _cli_ctx.running = true;
    _cli_ctx.serial_interface->open(NULL);
}

// Close the CLI
void cli_close(void)
{
    // Check if the CLI is running
    if (!_cli_ctx.running) {
        return;
    }

    // Close it
    _cli_ctx.running = false;
    _cli_ctx.serial_interface->close();
}
