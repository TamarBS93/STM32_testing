#ifndef I2CS_H_
#define I2CS_H_

#include <stdint.h>
#include "cmsis_os.h"

#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones

#include "project_header.h"

#define TIMEOUT 	1000 	// ticks (30  millis).

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c4;

extern osSemaphoreId_t I2cTxHandle;
extern osSemaphoreId_t I2cRxHandle;

Result i2c_testing(test_command_t*);
void i2c_reset(I2C_HandleTypeDef *hi2c);

#endif /* I2CS_H_ */
