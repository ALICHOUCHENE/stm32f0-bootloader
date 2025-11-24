/**
 ******************************************************************************
 * @file    flash.c
 * @brief   This file contains the implementation of the flash interface functions.
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

#include "flash.h"
#include "stm32f0xx.h"
#include "stm32f0xx_hal_flash.h"

/******************************************************************************/
/*                          Local Function Prototypes                         */
/*  Description : Declarations of static (file-scope) functions used only     */
/*                within this source file. These functions are not visible    */
/*                outside this module.                                        */
/******************************************************************************/

static bool _flash_interface_erase_page(uint32_t paddress)
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
	if (!_flash_interface_erase_page(paddress)) {
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
