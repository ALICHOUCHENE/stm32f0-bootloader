/**
  ******************************************************************************
  * @file    system.h
  * @brief   This file contains the headers of the system functions.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SYSTEM_H
#define __SYSTEM_H

/**
 * @brief Initialize core system components.
 *
 * This function performs all mandatory setup required to bring the system
 * into a valid operational state. It configures the clock tree, memory system,
 * interrupt controller, and base hardware peripherals needed for the firmware
 * to operate correctly.
 *
 * Typical operations include:
 * - Configuring system clocks and PLLs
 * - Enabling essential peripheral power domains
 * - Initializing interrupt vectors
 *
 * @note Must be called once during system startup before any driver or
 *       application initialization functions.
 */
void system_init(void);

/**
 * @brief Deinitialize and shut down system components.
 *
 * This function safely disables or resets active system modules, preparing
 * the platform for firmware handover. It ensures all
 * peripherals and interrupts are properly stopped, and that the system returns
 * to a defined and consistent state.
 *
 * Typical operations include:
 * - Disabling clocks and peripherals
 * - Resetting interrupt configurations
 * - Powering down or isolating subsystems
 */
void system_deinit(void);

/**
 * @brief Handle critical system errors and unrecoverable faults.
 *
 * This function is invoked when the firmware detects a fatal system condition
 * or an unrecoverable hardware exception. It provides a unified mechanism
 * for fault logging, error indication, and controlled recovery actions.
 *
 * @note This function must not return. It should either reset the system
 *          or enter a fail-safe infinite loop depending on implementation policy.
 */
void system_error_handler(void);

#endif
