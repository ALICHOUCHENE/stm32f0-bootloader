/**
 ******************************************************************************
 * @file    bootloader.c
 * @brief   This file contains the implementation of the bootloader functions.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 IZITRON.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdbool.h>
#include "bootloader.h"
#include "serial.h"
#include "system.h"
#include "xmodem.h"
#include "flash.h"
#include "string.h"
#include "cmsis_gcc.h"
#include "stm32f0xx.h"

#define BOOTLOADER_MAX_CLI_BUFFER_SIZE		255		// Maximum CLI buffer size
#define BOOTLOADER_MAX_COMMAND_LENGTH 		64		// Maximum command length

typedef struct {
	uint32_t __attribute__((aligned(4))) buffer[FLASH_PAGE_SIZE_WORDS];
	size_t len;
	uint8_t page_offset;
} flash_page_t;

typedef struct {
	const serial_interface_t* serial_interface;			// Pointer to the serial interface
	uint8_t buffer[BOOTLOADER_MAX_CLI_BUFFER_SIZE];		// Buffer to store received CLI or data input
	uint8_t len;										// Current length of valid data in the buffer
	flash_page_t flash_page;							// Flash memory page data to write
} bootloader_ctx_t;

static bootloader_ctx_t _bootloader_ctx;

/******************************************************************************
 *                              CLI messages
 ******************************************************************************/
const char bootloader_welcome_msg[] = "================================================\r\n"
									  "   		STM32 Bootloader v1.0.0    \r\n"
									  "================================================\r\n";

const char bootloader_menu_msg[] = "=================== Bootloader Command Menu ===================\r\n"
								   "Available commands:\r\n"
								   "  info     : Display system information\r\n"
								   "  help     : Display bootloader commands\r\n"
								   "  upload   : Upload a new application binary to flash using XMODEM\r\n"
								   "  jump     : Jump to the user application\r\n";

const char bootloader_system_info_msg[] ="  Arch   \t: ARM\r\n"
										 "  Core   \t: Cortex-M0\r\n"
										 "  Device \t: STM32F030xC\r\n"
									     "  Flash  \t: 256 KB\r\n";
const char _unknown[] = "unknown\r\n";
const char _jump_to_application[] = "Jump to application...\r\n";
const char _bootloader_start_binary_upload[] = "Starting binary upload via XMODEM...\r\n";


/******************************************************************************
 *                              Serial interface
 ******************************************************************************/

static void serial_user_cb(void)
{
	size_t len = 0;

	// Data received over the serial interface
	_bootloader_ctx.serial_interface->read(_bootloader_ctx.buffer + _bootloader_ctx.len, &len);
	_bootloader_ctx.len += len;

	// Check serial input length
	if (_bootloader_ctx.len >= BOOTLOADER_MAX_CLI_BUFFER_SIZE) {
		_bootloader_ctx.len = 0;
	}
}

static void _bootloader_cli_print(const char* msg, uint16_t len)
{
	_bootloader_ctx.serial_interface->write((const uint8_t*)msg, len);
}

/******************************************************************************
 *                              Xmodem facilities
 ******************************************************************************/

static void _xmodem_cleanup(void)
{
	// Clean the buffer
	memset(_bootloader_ctx.buffer, 0, BOOTLOADER_MAX_CLI_BUFFER_SIZE);
	_bootloader_ctx.len = 0;

	// Reopen the serial interface
	_bootloader_ctx.serial_interface = serial_interface_init();
	_bootloader_ctx.serial_interface->open((serial_user_callback_t)serial_user_cb);
	HAL_Delay(100);
}

static void _xmodem_user_cb(xmodem_result_t result, uint8_t* buffer, uint16_t len)
{
	bool flash_success = false;

	switch (result) {
	case xmodem_result_rx_success:
		// Copy packet received to flash page buffer
		memcpy(_bootloader_ctx.flash_page.buffer + _bootloader_ctx.flash_page.len, (uint32_t*)buffer, len);
		_bootloader_ctx.flash_page.len += len/4;

		// Check if flash page buffer is full, flash it
		if (_bootloader_ctx.flash_page.len == FLASH_PAGE_SIZE_WORDS) {
			flash_success = flash_interface_write_page(APPLICATION_BASE_PAGE + _bootloader_ctx.flash_page.page_offset,
									   	   	   	   	   _bootloader_ctx.flash_page.buffer,
													   _bootloader_ctx.flash_page.len);
			// Abort the transfer if the page flash fails
			if (!flash_success) {
				xmodem_stop();
			}

			// Increment page offset
			_bootloader_ctx.flash_page.page_offset ++;
			memset(_bootloader_ctx.flash_page.buffer, 0, _bootloader_ctx.flash_page.len * 4);
			_bootloader_ctx.flash_page.len = 0;
		}
		break;

	case xmodem_result_completed:

		// Write the reaming data in the flash page buffer
		flash_interface_write_page(APPLICATION_BASE_PAGE + _bootloader_ctx.flash_page.page_offset,
								   _bootloader_ctx.flash_page.buffer,
				                   _bootloader_ctx.flash_page.len);

		_xmodem_cleanup();
		break;

	case xmodem_result_aborted:
		_xmodem_cleanup();
		break;

	default:
		break;
	}

}

/******************************************************************************
 *                              Bootloader actions
 ******************************************************************************/

static void _bootloader_jump_to_application(uint32_t application_address)
{
	// Define jump function
	typedef void (*pFunction)(void);

	uint32_t app_stack     	= *(volatile uint32_t*)(application_address);
	uint32_t app_reset_handler = *(volatile uint32_t*)(application_address + 4);
	pFunction appEntry    = (pFunction)app_reset_handler;

	if (app_stack < SRAM_START_ADDRESS || app_stack > SRAM_TOP_ADDRESS) {
	    // Invalid stack pointer — application image is likely corrupted
	    return;
	}

	// Close the serial interface
	_bootloader_ctx.serial_interface->close();

	// Disable the interrupts
	__disable_irq();

	// Copy the vector table to SRAM
	memcpy((void*)SRAM_BASE_ADDRESS, (void*)application_address, VECTOR_TABLE_SIZE);

	// Re-map the SRAM to address 0x0
	SYSCFG->CFGR1 &= ~SYSCFG_CFGR1_MEM_MODE;
	SYSCFG->CFGR1 |=  SYSCFG_CFGR1_MEM_MODE_0 | SYSCFG_CFGR1_MEM_MODE_1;  // 0b11 = SRAM

	// De-init the system
	system_deinit();

	// Synchronize pipelines before jump
	__DSB();
	__ISB();

	// Set the MSP
	__set_MSP(app_stack);

	// Clear all pending interrupts
	NVIC->ICPR[0] = 0xFFFFFFFF;

	// Enable the interrupts
	__enable_irq();

	// Jump
	appEntry();

	// Loop forever; never reach here !
	while(1);
}

static void _bootloader_execute_action_upload(void)
{
	// Print CLI message
	_bootloader_cli_print(_bootloader_start_binary_upload, sizeof(_bootloader_start_binary_upload));
	HAL_Delay(500);

	// Close CLI interface, to be used by the Xmdoem
	_bootloader_ctx.serial_interface->close();

	// Start Xmodem transfer
	xmodem_start((xmodem_user_callback_t)_xmodem_user_cb);
}

static void _bootloader_execute_action()
{
	volatile uint8_t usr_cmd_end = 0;
	char cmd[BOOTLOADER_MAX_COMMAND_LENGTH];
	bool complete = false;

	// Check serial buffer
	if (!_bootloader_ctx.len) {
		// Nothing to do
		return;
	}

	// Check if the user input is just an enter
	if ((_bootloader_ctx.buffer[0] == '\r')) {

		// Print the prompt
		_bootloader_cli_print("\n->", 3);

		// Shift the buffer
		memmove(_bootloader_ctx.buffer, _bootloader_ctx.buffer + 1, BOOTLOADER_MAX_CLI_BUFFER_SIZE - _bootloader_ctx.len);

		// Remove the consumed byte
		_bootloader_ctx.len -= 1; // \r consumed

		// Done
		return;
	}

	// Check if ther's \n after \r
	if ((_bootloader_ctx.buffer[0] == '\n')) {

		// Remove the \n and shift the buffer
		memmove(_bootloader_ctx.buffer, _bootloader_ctx.buffer + 1, BOOTLOADER_MAX_CLI_BUFFER_SIZE - _bootloader_ctx.len);

		// Remove the consumed byte
		_bootloader_ctx.len -= 1; // \n consumed
	}

	// Locate the end of the user command \r
	while(usr_cmd_end <= _bootloader_ctx.len - 1) {
		if (_bootloader_ctx.buffer[usr_cmd_end] == '\r') {
			complete = true;
			break;
		}
		usr_cmd_end ++;
	}

	// Check if the user input is complete
	if (!complete) {
		return;
	}

	// Remove the \r from the user input (cmd + \r)
	if ((usr_cmd_end > 1) && (_bootloader_ctx.buffer[usr_cmd_end - 1] == '\r')) {
		usr_cmd_end -= 1;
	}

	// Check user command length
	usr_cmd_end = usr_cmd_end > BOOTLOADER_MAX_COMMAND_LENGTH - 1 ? BOOTLOADER_MAX_COMMAND_LENGTH - 1 : usr_cmd_end;

	// Isolate the user command
	memcpy(cmd, _bootloader_ctx.buffer, usr_cmd_end);
	cmd[usr_cmd_end] = '\0';

	// Remove the consumed bytes
	_bootloader_ctx.len -= usr_cmd_end;

	// Shift the buffer
	memcpy(_bootloader_ctx.buffer, _bootloader_ctx.buffer + usr_cmd_end, _bootloader_ctx.len);

	// Print LF
	_bootloader_cli_print("\n", 1);

	// Execute command
	if (strstr(cmd, "info")) {
		// Print platform info
		_bootloader_cli_print(bootloader_system_info_msg, sizeof(bootloader_system_info_msg));
	} else if (strstr(cmd, "jump")) {
		// Jump to application
		_bootloader_cli_print(_jump_to_application, sizeof(_jump_to_application));
		HAL_Delay(100);
		_bootloader_jump_to_application(APPLICATION_BASE_ADDRESS);
	} else if (strstr(cmd, "help")) {
		// Display commands menu
		_bootloader_cli_print(bootloader_menu_msg, sizeof(bootloader_menu_msg));
	} else if (strstr(cmd, "upload")) {
		_bootloader_execute_action_upload();
	} else {
		// Unknown command
		_bootloader_cli_print(_unknown, sizeof(_unknown));
	}
}


/******************************************************************************/
/*                              Public API Functions                          */
/*  Description : Definitions of externally visible functions declared in the */
/*                corresponding header file (.h). These functions provide the */
/*                module's interface for use by other components.             */
/******************************************************************************/

void bootloader_entry(void)
{
	// Clear context
	memset(&_bootloader_ctx, 0, sizeof(bootloader_ctx_t));

	// Initialize and open the serial interface
	_bootloader_ctx.serial_interface = serial_interface_init();
	_bootloader_ctx.serial_interface->open((serial_user_callback_t)serial_user_cb);
	HAL_Delay(100);

	// Display bootloader welcome message and menu
	_bootloader_cli_print(bootloader_welcome_msg, sizeof(bootloader_welcome_msg));
	_bootloader_cli_print(bootloader_menu_msg, sizeof(bootloader_menu_msg));

	// Execute bootloader actions
	while(1) {
		//_bootloader_execute_action();
		_bootloader_execute_action();
	}
}

void bootloader_exit(void)
{
	// Jump to the user application
	_bootloader_jump_to_application(APPLICATION_BASE_ADDRESS);
}
