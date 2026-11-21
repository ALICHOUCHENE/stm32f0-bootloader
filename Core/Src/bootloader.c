/**
 ******************************************************************************
 * @file    bootloader.c
 * @brief   This file contains the declaration of the bootloader functions.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 ALI CHOUCHENE.
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
#include "cli.h"
#include "serial.h"
#include "system.h"
#include "xmodem.h"
#include "flash.h"
#include "string.h"
#include "cmsis_gcc.h"
#include "stm32f0xx.h"
#include "bootloader_signature.h"

#include "crypto_dsa.h"

#define XMODEM_MAX_PADDING  				(XMODEM_DATA_CHUNK_SIZE - 1)	// XMODEM maximum padding bytes at the end of the file

typedef struct {
	uint32_t __attribute__((aligned(4))) buffer[FLASH_PAGE_SIZE_WORDS];
	size_t len;
	uint8_t page_offset;
} flash_page_t;

typedef struct {
	const serial_interface_t* serial_interface;			// Pointer to the serial interface
	uint8_t len;										// Current length of valid data in the buffer
	uint8_t page_count;									// Number of pages written
	uint32_t total_length;								// Total number of bytes received in bytes (binary length)
	flash_page_t flash_page;							// Flash memory page data to write
} bootloader_ctx_t;

static bootloader_ctx_t _bootloader_ctx;


// Forward declaration
static bool _bootloader_move_binary_to_application_area(uint8_t application_start_page_id, uint8_t staging_start_page_id, uint8_t page_count);

#ifdef SECURITY_ENABLED
static authentication_status_t _bootloader_verify_dsa_signature(bootloader_ctx_t* ctx);
#endif

/******************************************************************************
 *                              CLI messages
 ******************************************************************************/
const char _bootloader_welcome_msg[] = "================================================\r\n"
									  "   		STM32 Bootloader v1.1.0    \r\n"
									  "================================================\r\n";

const char _bootloader_menu_msg[] = "=================== Bootloader Command Menu ===================\r\n"
								   "Available commands:\r\n"
								   "  info     : Display system information\r\n"
								   "  help     : Display bootloader commands\r\n"
								   "  upload   : Upload a new application binary to flash using XMODEM\r\n"
								   "  jump     : Jump to the user application\r\n";

const char _bootloader_system_info_msg[] ="  Arch   \t: ARM\r\n"
										 "  Core   \t: Cortex-M0\r\n"
										 "  Device \t: STM32F030xC\r\n"
									     "  Flash  \t: 256 KB\r\n";
const char _jump_to_application[] = "Jump to application...\r\n";
const char _bootloader_start_binary_upload[] = "Starting binary upload via XMODEM...\r\n";
const char _bootloader_no_valid_app[] = "No valid application \r\n";

#ifdef SECURITY_ENABLED
const char _bootloader_auth_success[] = "\r\nSigned binary authenticated!\r\n";
const char _bootloader_auth_footer_not_found[] = "\r\nAuthentication failed: Footer not found \r\n";
const char _bootloader_auth_file_length_mismatch[] = "\r\nAuthentication failed: File length mismatch\r\n";
const char _bootloader_auth_invalid_signature[] = "\r\nAuthentication failed: Invalid signature \r\n";
#endif

/******************************************************************************
 *                              Serial interface
 ******************************************************************************/

#ifdef SECURITY_ENABLED
// Print authentication result
static void _bootloader_cli_print_auth_status(authentication_status_t status)
{
	switch(status) {
	case authentication_status_success:
		cli_print(_bootloader_auth_success);
		break;

	case authentication_status_footer_not_found:
		cli_print(_bootloader_auth_footer_not_found);
		break;

	case authentication_status_file_length_mismatch:
		cli_print(_bootloader_auth_file_length_mismatch);
		break;

	case authentication_status_invalid_signature:
		cli_print(_bootloader_auth_invalid_signature);
		break;

	default:
		break;
	}
}
#endif

/******************************************************************************
 *                              Xmodem facilities
 ******************************************************************************/

static void _xmodem_cleanup(void)
{
	// Resume CLI
	cli_resume();
	HAL_Delay(100);
}

static void _xmodem_user_cb(xmodem_result_t result, uint8_t* buffer, uint16_t len)
{
	bool flash_success = false;

	// Increment the number of bytes received
	_bootloader_ctx.total_length += len;

	switch (result) {
	case xmodem_result_rx_success:
		// Copy packet received to flash page buffer
		memcpy(_bootloader_ctx.flash_page.buffer + _bootloader_ctx.flash_page.len, (uint32_t*)buffer, len);
		_bootloader_ctx.flash_page.len += len/4;

		// Check if flash page buffer is full, flash it
		if (_bootloader_ctx.flash_page.len == FLASH_PAGE_SIZE_WORDS) {
			// Write the received bytes in the staging area
			flash_success = flash_interface_write_page(STAGING_AREA_BASE_PAGE + _bootloader_ctx.flash_page.page_offset,
									   	   	   	   	   _bootloader_ctx.flash_page.buffer,
													   _bootloader_ctx.flash_page.len);
			// Abort the transfer if the page flash fails
			if (!flash_success) {
				xmodem_stop();
			}

			// Increment page offset
			_bootloader_ctx.flash_page.page_offset ++;

			// A new page is written increment the total number of pages
			_bootloader_ctx.page_count ++;

			// Prepare buffer for the next page
			memset(_bootloader_ctx.flash_page.buffer, 0, _bootloader_ctx.flash_page.len * 4);
			_bootloader_ctx.flash_page.len = 0;
		}
		break;

	case xmodem_result_completed:

		// Write the remaining data in the flash page buffer
		flash_interface_write_page(STAGING_AREA_BASE_PAGE + _bootloader_ctx.flash_page.page_offset,
								   _bootloader_ctx.flash_page.buffer,
				                   _bootloader_ctx.flash_page.len);

		// A new page is written increment the total number of pages
		_bootloader_ctx.page_count ++;

#ifdef SECURITY_ENABLED
		// Check signature
		authentication_status_t status;
		status = _bootloader_verify_dsa_signature(&_bootloader_ctx);

		// Authenticated binary move it from staging area to application area.
		if (status == authentication_status_success) {
			_bootloader_move_binary_to_application_area(APPLICATION_BASE_PAGE, STAGING_AREA_BASE_PAGE, _bootloader_ctx.page_count);
		}
#else
		// Just move the application binary from the staging area to application area
		_bootloader_move_binary_to_application_area(APPLICATION_BASE_PAGE, STAGING_AREA_BASE_PAGE, _bootloader_ctx.page_count);
#endif

		// We are done close the xmodem facility
		_xmodem_cleanup();
#ifdef SECURITY_ENABLED
		// Display authentication status
		_bootloader_cli_print_auth_status(status);
#endif
		break;

	case xmodem_result_aborted:
		// We are done close the xmodem facility
		_xmodem_cleanup();
		break;

	default:
		break;
	}

}

/******************************************************************************
 *                              Bootloader actions
 ******************************************************************************/
// This function moves the binary from the staging area to the application area
static bool _bootloader_move_binary_to_application_area(uint8_t application_start_page_id, uint8_t staging_start_page_id, uint8_t page_count)
{
	// Move the user binary from staging to application area
	for (int ii = 0; ii < page_count; ii ++) {
		if (!flash_interface_memory_move(application_start_page_id + ii, staging_start_page_id + ii)) {
			return false;
		}
	}

	// All data were moved form the staging area to the application area
	return true;
}

#ifdef SECURITY_ENABLED
static authentication_status_t _bootloader_verify_dsa_signature(bootloader_ctx_t* ctx)
{
	const uint8_t* base = (const uint8_t*)STAGING_AREA_BASE_ADDRESS;
	const uint32_t total = ctx->total_length;
	signed_file_footer_t footer;
	uint32_t end;

	// Received file too small
	if (total < sizeof(signed_file_footer_t)) {
		return authentication_status_footer_not_found;
	}

	// Loop to find the magic number somewhere in [total-127, total]
	for (uint8_t pad = 0; pad <= XMODEM_MAX_PADDING; pad++) {

		// candidate end of the real file
		end = total - pad;

		// Extract the footer
		memcpy(&footer, base + end - sizeof(signed_file_footer_t), sizeof(footer));

		// Check the magic number
		if (footer.magic != SIGNED_FILE_MAGIC_NUMBER) {
			continue;
		}

		// Check length
		if (footer.file_length != end - sizeof(signed_file_footer_t)) {
			return authentication_status_file_length_mismatch;
		}

		// Footer found at a self-consistent position
		return crypto_dsa_verify(public_key, base, footer.file_length, footer.signature)
				? authentication_status_success
				: authentication_status_invalid_signature;
	}

	return authentication_status_footer_not_found;
}
#endif

static void _bootloader_jump_to_application(void)
{
	typedef void (*pFunction)(void);	// Define jump function
	uint32_t app_stack, app_reset_handler;
	pFunction app_entry;

	// Fetch the stack pointer and the reset handler address for the staging area
	app_stack     		= *(volatile uint32_t*)(APPLICATION_BASE_ADDRESS);
	app_reset_handler 	= *(volatile uint32_t*)(APPLICATION_BASE_ADDRESS + 4);

	// Check binary integrity in the staging area
	if (app_stack < SRAM_START_ADDRESS || app_stack > SRAM_TOP_ADDRESS) {
		// Invalid stack pointer — application image is likely corrupted
		cli_print(_bootloader_no_valid_app);
		return;
	}

	// Display the application jump traces
	cli_print(_jump_to_application);

	// Close the CLI
	cli_close();

	// Disable the interrupts
	__disable_irq();

	// Copy the vector table to SRAM
	memcpy((void*)SRAM_BASE_ADDRESS, (void*)APPLICATION_BASE_ADDRESS, VECTOR_TABLE_SIZE);

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
	app_entry = (pFunction)app_reset_handler;
	app_entry();

	// Loop forever; never reach here !
	while(1);
}

static void _bootloader_upload(void)
{
	// Clear xmodem local variables
	_bootloader_ctx.page_count = 0;
	_bootloader_ctx.total_length = 0;
	_bootloader_ctx.len = 0;
	memset(&_bootloader_ctx.flash_page, 0, sizeof(flash_page_t));

	// Erase application base page. Fail safe in case of reset during the transfer
	flash_interface_erase_page(APPLICATION_BASE_PAGE);

	// Print CLI message
	cli_print(_bootloader_start_binary_upload);
	HAL_Delay(500);

	// Close the CLI
	cli_suspend();

	// Start Xmodem transfer
	xmodem_start((xmodem_user_callback_t)_xmodem_user_cb);
}

// Display commands menu
static void _bootloader_dsiplay_help(void)
{
	cli_print(_bootloader_menu_msg);
}

// Print platform info
static void _bootloader_dsiplay_info(void)
{
	cli_print(_bootloader_system_info_msg);
}

/******************************************************************************/
/*                              Define CLI commands table                     */
/******************************************************************************/

static cli_command_t table[] = {
		{"help", "Display bootloader commands", _bootloader_dsiplay_help},
		{"info", "Display system information", _bootloader_dsiplay_info},
		{"upload", "Upload a new application binary to flash using XMODEM", _bootloader_upload},
		{"jump", "Jump to the user application", _bootloader_jump_to_application},
};

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

	// Initialize the CLI
	cli_init(table, sizeof(table) / sizeof(cli_command_t));

	// Display bootloader welcome message and menu
	cli_print(_bootloader_welcome_msg);
	cli_print(_bootloader_menu_msg);

	// Run the CLI
	while(1) {
		cli_run();
	}
}

void bootloader_exit(void)
{
	// Jump to the user application
	_bootloader_jump_to_application();
}
