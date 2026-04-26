/**
 ******************************************************************************
 * @file    serial.h
 * @brief   This file contains the declaration of the serial interface functions.
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

#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>
#include <stdio.h>

// User callback
typedef void  (*serial_user_callback_t)(void);

// Define function pointer types (optional but clean)
typedef void  (*serial_open_t)(serial_user_callback_t);
typedef void  (*serial_close_t)(void);
typedef void  (*serial_write_t)(const uint8_t *data, size_t len);
typedef void  (*serial_read_t)(uint8_t *buffer, size_t* len);

/**
 * @typedef serial_interface_t
 * @brief Structure defining the serial interface function pointers.
 *
 * This structure groups together function pointers for serial operations.
 * Each member corresponds to a specific serial port operation.
 */
typedef struct {
    serial_open_t  open;   /**< Function pointer to open the serial interface. */
    serial_close_t close;  /**< Function pointer to close the serial interface. */
    serial_write_t write;  /**< Function pointer to write data to the serial interface. */
    serial_read_t  read;   /**< Function pointer to read data from the serial interface. */
} serial_interface_t;

/**
 * @brief Initializes and returns the serial interface implementation.
 *
 * This function initializes the available serial interface implementation and returns
 * a pointer to a populated ::serial_interface_t structure. The returned structure
 * provides access to the hardware- or platform-specific serial functions.
 *
 * @return Pointer to a constant ::serial_interface_t structure containing
 *         initialized function pointers for serial operations.
 */
const serial_interface_t* serial_interface_init(void);

#endif
