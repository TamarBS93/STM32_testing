
/*

 */

/**
 * @file adcs.c
 * @brief ADC peripheral verification logic using DAC loopback.
 * * Hardware Connection Requirement:
 * ADC                               DAC
 * PC0 [ADC1_IN10] (CN9) <---------- PA4 [DAC_OUT1] (CN7)
 */

#include "adcs.h"

/**
 * @brief Performs a hardware verification test on the ADC peripheral.
 * * This test uses the DAC to generate a specific voltage defined in the
 * command's bit pattern, then reads it back via the ADC to verify accuracy.
 * * @param command Pointer to the test_command_t structure containing test parameters.
 * @return Result TEST_PASS if all iterations are within tolerance, TEST_FAIL or TEST_ERR otherwise.
 */
Result adc_testing(test_command_t* command) {
    uint32_t adc_value;
    int32_t difference;
    HAL_StatusTypeDef status;
    uint32_t expected_adc_result;
    uint32_t adc_tolerance;

    // Validate command pointer
    if (command == NULL) {
        return TEST_ERR;
    }

    // Start DAC Peripheral
    status = HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
    if (status != HAL_OK) {
        return TEST_FAIL;
    }

    for (uint8_t i = 0; i < command->iterations; i++) {
        /* * Use pattern data for expected value. If iterations exceed pattern length,
         * the last available pattern byte continues to be used.
         */
        if (i < command->bit_pattern_length) {
            expected_adc_result = command->bit_pattern[i];
            adc_tolerance = (uint32_t)(expected_adc_result * TOLERANCE_PERCENT);
        }

        // Set voltage level via DAC and allow signal to settle
        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_8B_R, expected_adc_result);
        HAL_Delay(1);

        // Start ADC conversion in Interrupt mode
        status = HAL_ADC_Start_IT(&hadc1);
        if (status != HAL_OK) {
            HAL_ADC_Stop(&hadc1);
            return TEST_FAIL;
        }

        // Wait for ADC conversion completion signaled by ISR semaphore
        if (xSemaphoreTake(AdcSemHandle, HAL_MAX_DELAY) == pdPASS) {
            adc_value = HAL_ADC_GetValue(&hadc1);
        } else {
            HAL_ADC_Stop(&hadc1);
            return TEST_FAIL;
        }

        // Calculate absolute difference between generated and measured values
        difference = (int32_t)adc_value - (int32_t)expected_adc_result;
        if (difference < 0) {
            difference = -difference;
        }

        // Validate result against allowed tolerance
        if (difference > adc_tolerance) {
            HAL_ADC_Stop(&hadc1);
            return TEST_FAIL;
        }

        // Stop ADC to reset for next iteration
        status = HAL_ADC_Stop(&hadc1);
        if (status != HAL_OK) {
            return TEST_FAIL;
        }
    }

    return TEST_PASS;
}

/**
 * @brief ADC Conversion Complete Callback.
 * @param hadc Pointer to the ADC handle triggering the interrupt.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hadc->Instance == ADC1) {
        xSemaphoreGiveFromISR(AdcSemHandle, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
