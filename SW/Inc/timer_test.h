#ifndef TIMERS_H_
#define TIMERS_H_

#include <stdint.h>
#include "cmsis_os.h"

#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones

#include "project_header.h"

#define TIMEOUT 	1000

extern TIM_HandleTypeDef htim7;
extern osSemaphoreId_t TimSemHandle;

Result timer_testing(test_command_t*);

#endif /* TIMERS_H_ */
