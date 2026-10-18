/**
 * bsp/nucleo_f446re/src/stm32f4xx_it.c
 * * Architecture Layer: Board support / Interrupt handlers
 * Responsibility: Cortex-M exception handlers and peripheral IRQ handlers.
 *
 * SVC_Handler, PendSV_Handler and SysTick_Handler are implemented by the FreeRTOS port
 * (see the #defines at the end of FreeRTOSConfig.h). TIM6_DAC_IRQHandler is in hal_timebase_tim6.c.
 */
#include "board.h"

/* Faults: stop in a recognisable place. Milestone M8 adds register dump + watchdog reset. */
void NMI_Handler(void) {
    for (;;) {
    }
}

void HardFault_Handler(void) {
    for (;;) {
    }
}

void MemManage_Handler(void) {
    for (;;) {
    }
}

void BusFault_Handler(void) {
    for (;;) {
    }
}

void UsageFault_Handler(void) {
    for (;;) {
    }
}

void DebugMon_Handler(void) {}

/* User button B1 (PC13): the HAL clears the pending bit and calls HAL_GPIO_EXTI_Callback(). */
void EXTI15_10_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(BUTTON_PIN);
}
