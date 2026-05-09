/**
 ******************************************************************************
 * @file    system.c
 * @brief   This file contains the implementation of the system functions.
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
#include "stm32f0xx_hal.h"
#include <system.h>

/******************************************************************************/
/*                          Local Function Prototypes                         */
/*  Description : Declarations of static (file-scope) functions used only     */
/*                within this source file. These functions are not visible    */
/*                outside this module.                                        */
/******************************************************************************/

static void _system_clock_config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	// Initializes the RCC Oscillators
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
	RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		system_error_handler();
	}

	// Initializes the CPU, AHB and APB buses clocks
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
	{
		system_error_handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		system_error_handler();
	}
}

static void _system_gpio_clk_init(void)
{
	// GPIO Ports Clock Enable
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
}

/******************************************************************************/
/*                              Public API Functions                          */
/*  Description : Definitions of externally visible functions declared in the */
/*                corresponding header file (.h). These functions provide the */
/*                module's interface for use by other components.             */
/******************************************************************************/

void system_init(void)
{
	// Reset of all peripherals, Initializes the Flash interface and the Systick.
	HAL_Init();

	// Enable RCC sys-config clock
	__HAL_RCC_SYSCFG_CLK_ENABLE();

	// Enable RCC PWR clock
	__HAL_RCC_PWR_CLK_ENABLE();

	// Configure system clock, use the HSE
	_system_clock_config();

	// Enable GPIOs clock
	_system_gpio_clk_init();
}

void system_deinit(void)
{
	// De-init the peripherals
	HAL_DeInit();

	// Enable HSI (High-Speed Internal clock)
	RCC->CR |= RCC_CR_HSION;

	// Wait until HSI is ready
	while ((RCC->CR & RCC_CR_HSIRDY) == 0);

	// Select HSI as system clock
	RCC->CFGR &= ~RCC_CFGR_SW;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);

	// Turn off PLL, HSE, CSS
	RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);

	// Wait until PLL is off
	while ((RCC->CR & RCC_CR_PLLRDY) != 0);
}

void system_error_handler(void)
{
	__disable_irq();
	while (1);
}

/******************************************************************************/
/*           Cortex-M0 Processor Interruption and Exception Handlers          */
/******************************************************************************/

// This function handles NMI interrupt.
void NMI_Handler(void)
{
	__disable_irq();
	while (1);
}

// This function handles Hard fault interrupt.
void HardFault_Handler(void)
{
	__disable_irq();
	while (1);
}


// This function handles System service call via SWI instruction.
void SVC_Handler(void)
{
}

//This function handles Pendable request for system service.
void PendSV_Handler(void)
{
}

// This function handles System tick timer.
  void SysTick_Handler(void)
{
  HAL_IncTick();
}
