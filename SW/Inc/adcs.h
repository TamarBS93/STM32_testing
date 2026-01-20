#ifndef ADCS_P_H_
#define ADCS_P_H_

#include <stdint.h>
#include "cmsis_os.h"

#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones

#include "project_header.h"

extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac;

extern osSemaphoreId_t AdcSemHandle;

#define TOLERANCE_PERCENT 0.1f

Result adc_testing(test_command_t*);

#endif /* ADCS_P_H_ */
