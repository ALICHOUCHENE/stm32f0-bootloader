/**
 ******************************************************************************
 * @file    flash.c
 * @brief   This file contains the implementation of the flash interface functions.
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

#include "flash.h"
#include "stm32f0xx.h"
#include "stm32f0xx_hal_flash.h"

/******************************************************************************/
/*                          Local Function Prototypes                         */
/*  Description : Declarations of static (file-scope) functions used only     */
/*                within this source file. These functions are not visible    */
/*                outside this module.                                        */
/******************************************************************************/

static bool _flash_inetrface_erase_page(uint32_t paddress)
{
	FLASH_EraseInitTypeDef pEraseInit;
	uint32_t PageError;

	// Set page erase parameters
	pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
	pEraseInit.PageAddress = paddress;
	pEraseInit.NbPages = 1;

	// Erase page
	return (HAL_FLASHEx_Erase(&pEraseInit, &PageError) == HAL_OK);
}

static bool _flash_interface_write_to_page(uint32_t paddress, uint32_t* data, size_t len)
{
	// Write word starting from the page address
	for (int i = 0; i <= len - 1; i++) {
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, paddress, *data) != HAL_OK) {
			return false;
		}

		// Increment address and data
		paddress += 4;
		data ++;
	}

	// Done
	return true;
}

/******************************************************************************/
/*                              Public API Functions                          */
/*  Description : Definitions of externally visible functions declared in the */
/*                corresponding header file (.h). These functions provide the */
/*                module's interface for use by other components.             */
/******************************************************************************/

bool flash_interface_write_page(uint8_t page, uint32_t* data, size_t len)
{
	uint32_t paddress;

	// Check page number
	if ((page > FLASH_TOTAL_PAGE_COUNT) || (page < 0)) {
		return false;
	}

	// Check buffer size to write
	if (len > (FLASH_PAGE_SIZE_WORDS)) {
		return false;
	}

	// Unlock the flash
	if (HAL_FLASH_Unlock() != HAL_OK) {
		return false;
	}

	// Page start address
	paddress = FLASH_MEMORY_BASE_ADDR + page * FLASH_PAGE_SIZE_BYTES;

	// Erase Page
	if (!_flash_inetrface_erase_page(paddress)) {
		goto _error;
	}

	// Write to page
	if (!_flash_interface_write_to_page(paddress, data, len)) {
		goto _error;
	}

	// Lock the flash
	HAL_FLASH_Lock();

	return true;

_error:
	// Error occurred lock the flash and exit
	HAL_FLASH_Lock();
	return false;
}

bool flash_interface_memory_move(uint8_t dst_page, uint8_t src_page)
{
	uint32_t dst_page_address;
	uint32_t src_page_address;

	// Check page number
	if ((dst_page > FLASH_TOTAL_PAGE_COUNT) || (dst_page < 0)) {
		return false;
	}

	// Check page number
	if ((src_page > FLASH_TOTAL_PAGE_COUNT) || (src_page < 0)) {
		return false;
	}

	// Compute destination and source page addressees
	dst_page_address = FLASH_MEMORY_BASE_ADDR + dst_page * FLASH_PAGE_SIZE_BYTES;
	src_page_address = FLASH_MEMORY_BASE_ADDR + src_page * FLASH_PAGE_SIZE_BYTES;

	// Unlock the flash
	if (HAL_FLASH_Unlock() != HAL_OK) {
		return false;
	}

	// Erase the destination page
	if (!_flash_inetrface_erase_page(dst_page_address)) {
		// Fail to erase page
		goto _error;
	}

	// Move data from source to target page
	if (!_flash_interface_write_to_page(dst_page_address, (uint32_t *)src_page_address, FLASH_PAGE_SIZE_WORDS)) {
		// Fail to write data to page
		goto _error;
	}

	// Lock the flash
	HAL_FLASH_Lock();

	// All good
	return true;

_error:
	// Error occurred lock the flash and exit
	HAL_FLASH_Lock();
	return false;
}

bool flash_interface_erase_page(uint8_t page)
{
	uint32_t page_address;

	// Check page number
	if ((page > FLASH_TOTAL_PAGE_COUNT) || (page < 0)) {
		return false;
	}

	// Compute destination and source page addressees
	page_address = FLASH_MEMORY_BASE_ADDR + page * FLASH_PAGE_SIZE_BYTES;

	// Unlock the flash
	if (HAL_FLASH_Unlock() != HAL_OK) {
		return false;
	}

	// Erase the page
	if (!_flash_inetrface_erase_page(page_address)) {
		// Fail to erase page
		goto _error;
	}

	// Lock the flash
	HAL_FLASH_Lock();

	// All good
	return true;

	_error:
	// Error occurred lock the flash and exit
	HAL_FLASH_Lock();
	return false;
}

