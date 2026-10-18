/**
 * bsp/nucleo_f446re/include/board.h
 * * Architecture Layer: Board support / NUCLEO-F446RE
 * Responsibility: Clock tree, pin map and peripheral handles of the board.
 *
 *   LD2 (green LED)   PA5    push-pull output
 *   B1  (user button) PC13   EXTI falling edge, external pull-up on the board
 *   USART2 TX / RX    PA2 / PA3 (AF7) -> ST-LINK virtual COM port, 115200 8N1
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_NAME "nucleo-f446re"

#define LED_GPIO_PORT GPIOA
#define LED_PIN GPIO_PIN_5
#define BUTTON_GPIO_PORT GPIOC
#define BUTTON_PIN GPIO_PIN_13
#define BUTTON_EXTI_IRQn EXTI15_10_IRQn

#define CONSOLE_BAUDRATE 115200U

/* NVIC priorities (0 = highest). Anything that calls a FreeRTOS ...FromISR API must be
 * numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5). */
#define BUTTON_IRQ_PRIORITY 6U

extern UART_HandleTypeDef huart2;

/* 180 MHz from HSI (16 MHz) through the PLL, with over-drive. Returns HAL_OK on success. */
HAL_StatusTypeDef board_clock_init(void);
void board_gpio_init(void);
HAL_StatusTypeDef board_uart_init(void);
void board_led_toggle(void);

/* Unrecoverable configuration error: interrupts off, LED on, wait for the debugger. */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
