/**
 ******************************************************************************
 * @file    serial.c
 * @brief   This file contains the implementation of the serial interface.
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
#include "string.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_uart.h"
#include "system.h"
#include "serial.h"

#define SERIAL_MAX_RX_BUFFER_SIZE 256       // Maximum size of the serial receive buffer

typedef struct {
	bool open_done;								// Flag indicating whether the serial interface is initialized
	uint8_t buffer[SERIAL_MAX_RX_BUFFER_SIZE];	// Buffer to store received serial data
	uint8_t len;								// Number of valid bytes currently in the buffer
	serial_user_callback_t user_cb;				// User callback function for handling serial events
} serial_ctx_t;

static serial_ctx_t _serial_ctx;

UART_HandleTypeDef huart1;					// UART1 hardware handler


/******************************************************************************/
/*                          Local Function Prototypes                         */
/*  Description : Declarations of static (file-scope) functions used only     */
/*                within this source file. These functions are not visible    */
/*                outside this module.                                        */
/******************************************************************************/

static void _open(serial_user_callback_t user_cb)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Clear context
	memset(&_serial_ctx, 0, sizeof(serial_ctx_t));

	// Peripheral clock enable
	__HAL_RCC_USART1_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	// USART1 GPIO Configuration PA9 (USART1_TX) and PA10 (USART1_RX)
	GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF1_USART1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Initialize USART1
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		system_error_handler();
	}

	// USART1 interrupt Init
	HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);

	// Enable RXNE interrupt
	USART1->CR1 |= USART_CR1_RXNEIE;

	// Enable USART interrupt in NVIC
	HAL_NVIC_EnableIRQ(USART1_IRQn);

	// Open done
	_serial_ctx.open_done = true;

	// Store user callback
	_serial_ctx.user_cb = user_cb;
}

static void _close(void)
{
	// Peripheral clock disable
	__HAL_RCC_USART1_CLK_DISABLE();

	// Liberate GPIOs
	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

	// USART1 interrupt DeInit
	HAL_NVIC_DisableIRQ(USART1_IRQn);

	// Serial interface closed
	_serial_ctx.open_done = false;
}

static void _write(const uint8_t *data, size_t len)
{
	// Check if the serial interface is opened
	if (!_serial_ctx.open_done) {
		// Not opened, can't transmit
		return;
	}

	// Transmit
	HAL_UART_Transmit(&huart1, data, len, 1000);
}

static void _read(uint8_t *buffer, size_t* len)
{
	// Check if the interface is opened and buffer not empty
	if ((!_serial_ctx.open_done) || (!_serial_ctx.len)) {
		// Not opened or nothing to read
		return;
	}

	// Return available data in the buffer
	memcpy(buffer, _serial_ctx.buffer, _serial_ctx.len);
	*len = _serial_ctx.len;

	// Clear buffer
	_serial_ctx.len = 0;
}

// This function handles USART1 global interrupt.
__attribute__((used, externally_visible)) void USART1_IRQHandler(void)
{
	// Check if data received
	if (USART1->ISR & USART_ISR_RXNE) {
		// Read data, clears RXNE
		_serial_ctx.buffer[_serial_ctx.len] = USART1->RDR;
		_serial_ctx.len ++;
	}

	// Trigger user callback
	if (_serial_ctx.user_cb) {
		_serial_ctx.user_cb();
	}
}


/******************************************************************************/
/*                              Public API Functions                          */
/*  Description : Definitions of externally visible functions declared in the */
/*                corresponding header file (.h). These functions provide the */
/*                module's interface for use by other components.             */
/******************************************************************************/

const serial_interface_t serial_interface = {
	.open  = _open,
	.close = _close,
	.read  = _read,
	.write = _write,
};

const serial_interface_t* serial_interface_init(void)
{
	return &serial_interface;
}
