/**
  ******************************************************************************
  * @file    cli.h
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

#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>

#define CLI_MAX_COMMAND_COUNT    10			//  Maximum number of commands supported by the CLI

/**
 * @brief CLI command handler function type.
 */
typedef void (*cli_action_t)(void);


typedef struct {
    const char* name;       // Command name
    const char* helper;     // Command description
    cli_action_t action;    // Command handler
} cli_command_t;

/**
 * @brief Initialize the CLI.
 *
 * @param table Pointer to the command table.
 * @param entry_count Number of entries in the command table.
 * @return true if initialization succeeds, false otherwise.
 */
bool cli_init(const cli_command_t* table, size_t entry_count);

/**
 * @brief Process pending CLI input.
 */
void cli_process(void);

/**
 * @brief Print a string through the CLI serial interface.
 *
 * @param str String to print.
 */
void cli_print(const char* str);

/**
 * @brief Process received data and execute CLI commands.
 */
void cli_run(void);

/**
 * @brief Suspend the CLI and close the serial interface.
 */
void cli_suspend(void);

/**
 * @brief Resume the CLI and reopen the serial interface.
 */
void cli_resume(void);

/**
 * @brief Close the CLI.
 */
void cli_close(void);

#endif
