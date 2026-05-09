/**
  ******************************************************************************
  * @file    bootloader.h
  * @brief   This file contains the headers of the bootloader functions.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#define APPLICATION_BASE_ADDRESS	0x0800A000								/* Base address of the user application. */
#define SRAM_BASE_ADDRESS			0x20000000								/* Base address of SRAM. */
#define VECTOR_TABLE_SIZE			0xC0									/* Size of the vector table. */
#define SRAM_START_ADDRESS			SRAM_BASE_ADDRESS + VECTOR_TABLE_SIZE	/* Start of usable SRAM. */
#define SRAM_TOP_ADDRESS			SRAM_BASE_ADDRESS + 32 * 1024			/* Top of SRAM. */

/* Flash page index of application base. */
#define APPLICATION_BASE_PAGE		(APPLICATION_BASE_ADDRESS - FLASH_MEMORY_BASE_ADDR) / (FLASH_PAGE_SIZE_BYTES)

/* Define binary staging area*/
#define STAGING_AREA_BASE_ADDRESS	APPLICATION_BASE_ADDRESS + FLASH_PAGE_SIZE_BYTES
#define STAGING_AREA_BASE_PAGE		(STAGING_AREA_BASE_ADDRESS - FLASH_MEMORY_BASE_ADDR) / (FLASH_PAGE_SIZE_BYTES)

/**
 * @brief Enter bootloader mode and execute bootloader commands.
 *
 * This function switches the system into bootloader mode and
 * starts executing the bootloader command interface.
 */
void bootloader_entry(void);

/**
 * @brief Exit bootloader mode and jump to the main application.
 *
 * This function terminates the bootloader operation and
 * transfers control to the main application firmware.
 */
void bootloader_exit(void);

#endif
