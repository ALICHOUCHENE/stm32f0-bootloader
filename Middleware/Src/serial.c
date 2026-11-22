
/**
 ******************************************************************************
 * @file    serial.c
 * @brief   This file contains the implementation of the serial interface.
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
#include "string.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_uart.h"
#include "system.h"
#include "serial.h"

#define SERIAL_MAX_RX_BUFFER_SIZE 	1024						     	// Maximum size of the serial receive buffer
#define SERIAL_RX_BUFFER_MASK     	(SERIAL_MAX_RX_BUFFER_SIZE - 1) 	// RX ring buffer indexing mask

typedef struct {
    uint8_t buffer[SERIAL_MAX_RX_BUFFER_SIZE];  // Received data storage
    volatile uint16_t head;                     // Write position
    volatile uint16_t tail;                     // Read position
} serial_rx_buffer_t;

typedef struct {
    bool open_done;                             // Flag indicating whether the serial interface is initialized
    serial_rx_buffer_t rx_buffer;               // Serial receive ring buffer
    serial_user_callback_t user_cb;             // User callback function for handling serial events
} serial_ctx_t;

static serial_ctx_t _serial_ctx;

UART_HandleTypeDef huart1;                      // UART1 hardware handler


/******************************************************************************/
/*                          Local Function Prototypes                         */
/*  Description : Declarations of static (file-scope) functions used only     */
/*                within this source file. These functions are not visible    */
/*                outside this module.                                        */
/******************************************************************************/

// Ring buffer management
static bool _serial_rx_buffer_is_empty(void)
{
    return (_serial_ctx.rx_buffer.head == _serial_ctx.rx_buffer.tail);
}

static bool _serial_rx_buffer_is_full(void)
{
    return (((_serial_ctx.rx_buffer.head + 1) & SERIAL_RX_BUFFER_MASK) == _serial_ctx.rx_buffer.tail);
}

static bool _serial_rx_buffer_push(uint8_t data)
{
    uint16_t head = _serial_ctx.rx_buffer.head;

    // Check if the RX buffer is full
    if (_serial_rx_buffer_is_full()) {
        return false;
    }

    // Push the data
    _serial_ctx.rx_buffer.buffer[head] = data;
    _serial_ctx.rx_buffer.head = (head + 1) & SERIAL_RX_BUFFER_MASK;

    return true;
}

static bool _serial_rx_buffer_pop(uint8_t *data)
{
    uint16_t tail = _serial_ctx.rx_buffer.tail;

    // Check if the RX buffer is empty
    if (!data || _serial_rx_buffer_is_empty()) {
        return false;
    }

    // Pop the data
    *data = _serial_ctx.rx_buffer.buffer[tail];
    _serial_ctx.rx_buffer.tail = (tail + 1) & SERIAL_RX_BUFFER_MASK;

    return true;
}

static void _serial_rx_buffer_init(void)
{
    _serial_ctx.rx_buffer.head = 0;
    _serial_ctx.rx_buffer.tail = 0;
}

// Serial interface management */
static void _open(serial_user_callback_t user_cb)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Clear context
    memset(&_serial_ctx, 0, sizeof(serial_ctx_t));

    // Initialize RX buffer
    _serial_rx_buffer_init();

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

    // Store user callback
    _serial_ctx.user_cb = user_cb;

    // Serial interface opened
    _serial_ctx.open_done = true;

    // Enable RXNE interrupt
    USART1->CR1 |= USART_CR1_RXNEIE;

    // Enable USART interrupt in NVIC
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void _close(void)
{
    // Disable USART interrupt
    HAL_NVIC_DisableIRQ(USART1_IRQn);

    // Disable RXNE interrupt
    USART1->CR1 &= ~USART_CR1_RXNEIE;

    // Peripheral clock disable
    __HAL_RCC_USART1_CLK_DISABLE();

    // Liberate GPIOs
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

    // Clear RX buffer
    _serial_rx_buffer_init();

    // Serial interface closed
    _serial_ctx.open_done = false;
    _serial_ctx.user_cb = NULL;
}

static void _write(const uint8_t *data, size_t len)
{
    // Check if the serial interface is opened
    if (!_serial_ctx.open_done) {
        // Not opened, can't transmit
        return;
    }

    // Transmit
    HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 1000);
}

static void _read(uint8_t *buffer, size_t* len)
{
    // Check input parameters
    if (!buffer || !len) {
        return;
    }

    *len = 0;

    // Check if the interface is opened
    if (!_serial_ctx.open_done) {
        // Not opened, can't read
        return;
    }

    // Read available data from the RX buffer
    while (_serial_rx_buffer_pop(&buffer[*len])) {
        (*len)++;
    }
}


// This function handles USART1 global interrupt.
__attribute__((used, externally_visible)) void USART1_IRQHandler(void)
{
    // Check if data received
    if (USART1->ISR & USART_ISR_RXNE) {

        // Read data, clears RXNE
        uint8_t data = USART1->RDR;

        // Push received data into the RX buffer
        _serial_rx_buffer_push(data);
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
