#include "adcs.h"

/*
 * ADC                          DAC
 * PC0 [IN10] (CN9) <---------- PA4 (CN7)
 */

/*
 * @brief Performs a test on the ADC peripheral using the command protocol.
 * @param command: A pointer to the test_command_t struct.
 * @retval result_t: The result of the test (TEST_PASS or TEST_FAIL).
 */
Result adc_testing(test_command_t* command){

	uint32_t adc_value;
    int32_t difference;
    HAL_StatusTypeDef status;

    // Check for valid command and bit pattern length
	if (command == NULL) {
//        printf("ADC_TEST: Received NULL command pointer. Skipping.\n\r"); // Debug printf
        return TEST_ERR;
	}
	uint32_t expected_adc_result = command->bit_pattern[0];
	uint32_t adc_tolerance = (uint32_t)(expected_adc_result * TOLERANCE_PERCENT);

    status = HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
    if (status != HAL_OK) {
//        printf("Error: Failed to start DAC conversion. Status: %d\n\r", status); // Debug printf
        return TEST_FAIL;
    }

	for(uint8_t i=0 ; i< command->iterations ; i++){

		if(i < command->bit_pattern_length){
			// Extract the 8-bit expected ADC value from the command's bit pattern
		    expected_adc_result = command->bit_pattern[i];
		    // Define a tolerance based on the expected result.
		    adc_tolerance = (uint8_t)(expected_adc_result * TOLERANCE_PERCENT);
		}

	    // Set value to DAC and run
	    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_8B_R, expected_adc_result);
	    HAL_Delay(1); // allow DAC to settle

	    // Start ADC conversion
	    status = HAL_ADC_Start_IT(&hadc1);
	    if (status != HAL_OK) {
//	        printf("Error: Failed to start ADC conversion. Status: %d\n\r", status); // Debug printf
	    	HAL_ADC_Stop(&hadc1);
	        return TEST_FAIL;
	    }

	    // waiting for the ADC conversion to complete and give a semaphore
	    if (xSemaphoreTake(AdcSemHandle, HAL_MAX_DELAY) == pdPASS){
		  // Get the converted value
		  adc_value = HAL_ADC_GetValue(&hadc1);
		} // end of ADC conversion
		else{
//	         printf("ADC semaphore acquire failed or timed out\n\r"); // Debug printf
	         HAL_ADC_Stop(&hadc1);
	         return TEST_FAIL;
		}

		// Compare the result with the expected value, within a tolerance
		difference = adc_value - expected_adc_result;
		difference = (difference < 0) ? -difference : difference; //absolute value of the difference

		if (difference > adc_tolerance)
		{
//			  printf("Test failed on iteration %u- Expected Value: %u, ADC value: %lu.\n\r",i+1, expected_adc_result, adc_value); // Debug printf
			  HAL_ADC_Stop(&hadc1);
			  return TEST_FAIL;
//		} else {
//				// Debug printf
//			  printf("ADC value is within tolerance for iteration %u\n\r", i+1);
//			  printf("Expected value=%d >> ADC value =%ld \n\r", expected_adc_result, adc_value);
		}
		// Stop the ADC conversion
		status = HAL_ADC_Stop(&hadc1);
		if (status != HAL_OK) {
//			printf("Warning: Failed to stop ADC conversion. Status: %d\n\r", status); // Debug printf
	         return TEST_FAIL;
		}
	} // end of iterations

	return TEST_PASS;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(AdcSemHandle, &xHigherPriorityTaskWoken);
//	printf("ADC complete callback fired and gave a semaphore\n\r"); // Debug printf
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
