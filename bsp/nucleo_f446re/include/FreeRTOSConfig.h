/**
 * bsp/nucleo_f446re/include/FreeRTOSConfig.h
 * * Architecture Layer: Board support / RTOS configuration
 * Responsibility: FreeRTOS kernel configuration for the STM32F446RE (Cortex-M4F, 4 NVIC priority bits).
 *
 * Key choices:
 *   - static allocation only: no heap at all (configSUPPORT_DYNAMIC_ALLOCATION = 0);
 *   - 1 kHz tick on SysTick (the HAL uses TIM6 as its own timebase);
 *   - interrupts with NVIC priority 0..4 are never masked by the kernel and must NOT call any
 *     FreeRTOS API; priorities 5..15 may call the ...FromISR functions.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#if defined(__GNUC__) && !defined(__ASSEMBLER__)
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t SystemCoreClock;
void vAssertCalled(const char* file, int line);
#ifdef __cplusplus
}
#endif
#endif

/* --- Scheduler ----------------------------------------------------------------------------------- */
#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE 0
#define configUSE_TIME_SLICING 1
#define configIDLE_SHOULD_YIELD 1
#define configCPU_CLOCK_HZ (SystemCoreClock)
#define configTICK_RATE_HZ ((TickType_t)1000)
#define configMAX_PRIORITIES 7
#define configMINIMAL_STACK_SIZE ((uint16_t)128)
#define configMAX_TASK_NAME_LEN 16
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#define configSTACK_DEPTH_TYPE uint32_t

/* --- Memory: static allocation only --------------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configKERNEL_PROVIDED_STATIC_MEMORY 0

/* --- Features ------------------------------------------------------------------------------------- */
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_TASK_NOTIFICATIONS 1
#define configUSE_QUEUE_SETS 0
#define configQUEUE_REGISTRY_SIZE 0
#define configUSE_TIMERS 0
#define configUSE_CO_ROUTINES 0
#define configUSE_NEWLIB_REENTRANT 0
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configGENERATE_RUN_TIME_STATS 0

/* --- Hooks and checks ----------------------------------------------------------------------------- */
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configASSERT(x)                                                                                                \
    if ((x) == 0) {                                                                                                    \
        vAssertCalled(__FILE__, __LINE__);                                                                             \
    }

/* --- Optional API --------------------------------------------------------------------------------- */
#define INCLUDE_vTaskPrioritySet 0
#define INCLUDE_uxTaskPriorityGet 0
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1 /* portMAX_DELAY blocks forever */
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1

/* --- Cortex-M interrupt priorities ------------------------------------------------------------- */
#define configPRIO_BITS 4 /* __NVIC_PRIO_BITS on STM32F4 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Map the port's handlers onto the CMSIS vector table names used by the startup file. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
