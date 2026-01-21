/**
 * @file timer_test.c
 * @brief TIMER peripheral verification logic using Interrupt/Semaphore heartbeat.
 * * Design Decision:
 * This test verifies the hardware's ability to generate periodic interrupts.
 * The Timer is configured to fire at a specific frequency (e.g., every 100ms).
 * The test passes if the CPU receives the expected number of interrupts
 * within the allowed time window.
 */

#include "timer_test.h"

/**
 * @brief Performs a hardware verification test on the TIMER peripheral.
 * * @param command Pointer to the test_command_t structure.
 * @return Result TEST_PASS if the timer pulses are received correctly,
 * TEST_FAIL if a timeout occurs, TEST_ERR for null input.
 */
Result timer_testing(test_command_t* command) {

    if (command == NULL) {
        return TEST_ERR;
    }

    // Start Timer in Interrupt mode
    if (HAL_TIM_Base_Start_IT(&htim7) != HAL_OK) {
        return TEST_FAIL;
    }

    for (uint8_t i = 0; i < command->iterations; i++) {
        /*
         * Wait for the Timer Callback to give the semaphore.
         * The timeout (200ms) acts as a "Watchdog". If the timer hardware
         * fails to pulse, the test fails.
         */
        if (xSemaphoreTake(TimSemHandle, pdMS_TO_TICKS(200)) != pdPASS) {
            HAL_TIM_Base_Stop_IT(&htim7);
            return TEST_FAIL;
        }

        // Pacing delay between pulse verifications
        osDelay(1);
    }

    // Stop Timer after the verification iterations are complete
    HAL_TIM_Base_Stop_IT(&htim7);

    return TEST_PASS;
}

