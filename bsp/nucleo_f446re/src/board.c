/**
 * bsp/nucleo_f446re/src/board.c
 * * Architecture Layer: Board support / NUCLEO-F446RE
 * Responsibility: System clock, GPIO and console UART initialisation (what STM32CubeMX would
 *                 generate in main.c, written by hand so the project builds from the command line).
 */
#include "board.h"

UART_HandleTypeDef huart2;

/*
 * Clock tree:
 *   HSI 16 MHz -> /M=8 -> 2 MHz -> xN=180 -> VCO 360 MHz -> /P=2 -> SYSCLK 180 MHz
 *                                                          -> /Q=8 -> 45 MHz (PLL48CLK, unused)
 *   AHB  /1 -> HCLK  180 MHz
 *   APB1 /4 -> PCLK1  45 MHz (timers x2 = 90 MHz)
 *   APB2 /2 -> PCLK2  90 MHz
 * 180 MHz requires voltage scale 1, over-drive and 5 flash wait states.
 */
HAL_StatusTypeDef board_clock_init(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 180;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 8;
    osc.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        return HAL_ERROR;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    return HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5);
}

void board_gpio_init(void) {
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* LD2 */
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
    gpio.Pin = LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &gpio);

    /* B1: the line goes low when pressed */
    gpio.Pin = BUTTON_PIN;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BUTTON_GPIO_PORT, &gpio);

    HAL_NVIC_SetPriority(BUTTON_EXTI_IRQn, BUTTON_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(BUTTON_EXTI_IRQn);
}

HAL_StatusTypeDef board_uart_init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = CONSOLE_BAUDRATE;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    return HAL_UART_Init(&huart2); /* pins and clocks: HAL_UART_MspInit() in hal_msp.c */
}

void board_led_toggle(void) {
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
}

void Error_Handler(void) {
    __disable_irq();
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
    for (;;) {
    }
}
