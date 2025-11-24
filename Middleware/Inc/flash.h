/**
 ******************************************************************************
 * @file    flash.h
 * @brief   This file contains the declaration of the flash interface functions.
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


#ifndef _FLASH_H
#define _FLASH_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define FLASH_TOTAL_SIZE_BYTES    (256 * 1024U)      								/* Total Flash size: 256 KB. */
#define FLASH_PAGE_SIZE_BYTES     (2 * 1024U)        								/* Page size: 2 KB. */
#define FLASH_PAGE_SIZE_WORDS     (FLASH_PAGE_SIZE_BYTES / 4U) 						/* Page size in 32-bit words. */
#define FLASH_TOTAL_PAGE_COUNT    (FLASH_TOTAL_SIZE_BYTES / FLASH_PAGE_SIZE_BYTES)	/* Total number of pages. */
#define FLASH_MEMORY_BASE_ADDR    0x08000000UL       								/* Flash memory base address. */

/**
 * @brief Writes data to a specific flash memory page.
 *
 * This function writes the specified data buffer to the given flash page.
 * The data length must not exceed the page size.
 *
 * @param page_number Page index to write (0 to PAGE_COUNT - 1).
 * @param data Pointer to the buffer containing 32-bit words to be written.
 * @param len Number of 32-bit words to write (must not exceed PAGE_SIZE_WORD).
 * @return `true` if the write operation was successful, `false` otherwise.
 */
bool flash_interface_write_page(uint8_t page_number, uint32_t* data, size_t len);

#endif
