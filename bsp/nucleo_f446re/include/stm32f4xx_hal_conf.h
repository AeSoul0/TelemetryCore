/**
 * bsp/nucleo_f446re/include/stm32f4xx_hal_conf.h
 * * Architecture Layer: Board support / HAL configuration
 * Responsibility: Selects the HAL modules compiled into the firmware and the clock constants.
 *                 Derived from Inc/stm32f4xx_hal_conf_template.h of the STM32F4 HAL v1.8.5,
 *                 trimmed to the modules this board uses. Add a module here AND in
 *                 bsp/nucleo_f446re/CMakeLists.txt (HAL_MODULES) when a new peripheral is needed.
 */
#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ########################## Oscillator Values ############################# */
/* NUCLEO-64: 8 MHz from the ST-LINK MCO (unused: the system clock runs from HSI) */
#define HSE_VALUE 8000000U
#define HSE_STARTUP_TIMEOUT 100U
#define HSI_VALUE 16000000U
#define LSI_VALUE 32000U
#define LSE_VALUE 32768U
#define LSE_STARTUP_TIMEOUT 5000U
#define EXTERNAL_CLOCK_VALUE 12288000U

/* ########################### System Configuration ######################### */
#define VDD_VALUE 3300U
/* HAL tick on TIM6 at the lowest priority; SysTick is reserved for FreeRTOS. */
#define TICK_INT_PRIORITY 0x0FU
#define USE_RTOS 0U
#define PREFETCH_ENABLE 1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE 1U

#define USE_HAL_TIM_REGISTER_CALLBACKS 0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U
#define USE_SPI_CRC 0U

/* ################################ Includes ################################ */
/* Same order as the ST template: the UART/TIM handles reference DMA types. */
/* clang-format off */
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_exti.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_uart.h"
/* clang-format on */

/* ############################# Assert Selection ########################### */
#ifdef USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t*)__FILE__, __LINE__))
void assert_failed(uint8_t* file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_HAL_CONF_H */
