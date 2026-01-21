#ifndef SPIS_H_
#define SPIS_H_

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones

#include "project_header.h"

#define TIMEOUT 	1000 	// ticks (60  millis).

#define RETRY_DELAY_MS 5
#define RETRY_COUNT 5
/*
 * SPI mapping in your project:
 * SPI_SENDER  -> hspi1 (Master)
 * SPI_RECEIVER-> hspi4 (Slave)
 */
#define SPI_SENDER     (&hspi1)
#define SPI_RECEIVER   (&hspi4)

#define CS_Pin          GPIO_PIN_0
#define CS_GPIO_Port    GPIOG
//
//extern uint8_t spi_tx_buffer[MAX_BIT_PATTERN_LENGTH];
//extern uint8_t spi_rx_buffer[MAX_BIT_PATTERN_LENGTH];

extern osSemaphoreId_t SpiTxHandle;
extern osSemaphoreId_t SpiRxHandle;
extern osSemaphoreId_t SpiSlaveRxHandle;

Result spi_testing(test_command_t*);
void clear_flags(SPI_HandleTypeDef *hspi);
void reset_test();

#endif /* SPIS_H_ */
