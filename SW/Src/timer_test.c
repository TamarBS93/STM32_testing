#include "timer_test.h"

/*
 * @brief Performs a test on the TIMER using the command protocol.
 * @param command: A pointer to the test_command_t struct.
 * @retval result_t: The result of the test (TEST_PASS or TEST_FAIL).
 */
Result timer_testing(test_command_t* command){

	uint16_t start_val ,end_val;

	if (command == NULL) {
//        printf("Received NULL command pointer. Skipping.\n\r"); // Debug printf
        return TEST_ERR;
	}

	// Start Timer
	HAL_TIM_Base_Start_IT(&htim7);

	for(uint8_t i=0 ; i< command->iterations ; i++){

	    if (xSemaphoreTake(TimSemHandle, pdMS_TO_TICKS(200)) != pdPASS) {
//			printf("Fail on iteration %u.\n\r",i+1); // Debug printf
	         vPortFree(command);
	         return TEST_FAIL;
	    }

//		printf("success on iteration %u.\n\r", i + 1); // Debug printf
        osDelay(10); // Small delay between iterations to prevent overwhelming the UUT or the system
	}// end of iterations

    // Stop Timer after the test is complete
	HAL_TIM_Base_Stop_IT(&htim7);

    return TEST_PASS;
}

