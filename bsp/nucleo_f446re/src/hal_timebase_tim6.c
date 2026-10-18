/**
 * bsp/nucleo_f446re/src/hal_timebase_tim6.c
 * * Architecture Layer: Board support / HAL timebase
 * Responsibility: Overrides the weak HAL_InitTick() so the HAL 1 ms tick (HAL_Delay, HAL timeouts)
 *                 comes from TIM6 instead of SysTick. SysTick belongs to the FreeRTOS scheduler.
 *                 Adapted from Src/stm32f4xx_hal_timebase_tim_template.c of the STM32F4 HAL.
 */
#include "board.h"

static TIM_HandleTypeDef htim6;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority) {
    RCC_ClkInitTypeDef clkconfig;
    uint32_t flashLatency;
    uint32_t timClock;

    if (TickPriority >= (1UL << __NVIC_PRIO_BITS)) {
        return HAL_ERROR;
    }

    __HAL_RCC_TIM6_CLK_ENABLE();

    /* TIM6 sits on APB1: its clock is PCLK1, doubled when the APB1 prescaler is not 1. */
    HAL_RCC_GetClockConfig(&clkconfig, &flashLatency);
    timClock = (clkconfig.APB1CLKDivider == RCC_HCLK_DIV1) ? HAL_RCC_GetPCLK1Freq() : 2U * HAL_RCC_GetPCLK1Freq();

    /* 1 MHz counter, update event every 1000 counts = 1 kHz */
    htim6.Instance = TIM6;
    htim6.Init.Prescaler = (timClock / 1000000U) - 1U;
    htim6.Init.Period = (1000000U / 1000U) - 1U;
    htim6.Init.ClockDivision = 0U;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
        return HAL_ERROR;
    }

    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    uwTickPrio = TickPriority;
    return HAL_OK;
}

void HAL_SuspendTick(void) {
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void) {
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM6) {
        HAL_IncTick();
    }
}

void TIM6_DAC_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim6);
}
