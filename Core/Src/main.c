/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
  
#include "stm32f0xx_hal.h"
#include "system.h"
#include "bootloader.h"


int main(void)
{
	// Initialize system peripherals
	system_init();
	HAL_Delay(500);

	// TODO: Implement bootloader entry check.
	// For example, configure a specific pin as an input — if the pin is pressed during reset,
	// the system should remain in the bootloader mode; otherwise, it should jump directly to the application (using bootloader_exit() function).
	// Alternatively, a reboot-persistent register or flag can be used to trigger bootloader entry
	// without requiring an external pin.

	// Bootloader entry
	bootloader_entry();
	
	return 0;
}
