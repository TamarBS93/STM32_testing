#ifndef UARTS_H_
#define UARTS_H_

#include <stdint.h>
#include "cmsis_os.h"

#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones
#include "stm32f7xx_hal_uart.h" // Specifically for UART_HandleTypeDef and HAL_UART functions

#include "project_header.h"

#define TIMEOUT 	1000 	// ticks (30  millis).

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;

extern osSemaphoreId_t UartTxHandle;
extern osSemaphoreId_t UartRxHandle;

Result uart_testing(test_command_t*);

#endif /* UARTS_H_ */
