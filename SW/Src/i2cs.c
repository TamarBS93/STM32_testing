#include "i2cs.h"
/*
 *
 * I2C4 (master)                I2C1 (slave)
 * PF14 SCL (CN10) <----------> PB8 SCL (CN7)
 * PF15 SDA (CN10) <----------> PB9 SDA (CN7)
 *
 */

#define I2C_SENDER 		(&hi2c4)   // Master
#define I2C_RECEIVER 	(&hi2c1)   // Slave
#define I2C_SLAVE_ADDR  (120 << 1) // left-shifted 7-bit address

/*
 * @brief Performs a test on the I2C peripheral using the command protocol.
 * @param command: A pointer to the test_command_t struct.
 * @retval result_t: The result of the test (TEST_PASS or TEST_FAIL).
 */
Result i2c_testing(test_command_t* command){

	uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	uint8_t echo_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

	HAL_StatusTypeDef status;

	if (command == NULL) {
//        printf("I2C_TEST: Received NULL command pointer. Skipping.\n\r"); // Debug printf
        return TEST_ERR;
	}

    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

	for(uint8_t i=0 ; i< command->iterations ; i++){
//	    printf("I2C_TEST: Iteration %u/%u -\n\r", i + 1, command->iterations); // Debug printf
	    memset(rx_buffer, 0, command->bit_pattern_length);

	    // --- 1. START RECEIVE DMA FIRST (SLAVE) ---
	    status = HAL_I2C_Slave_Receive_DMA(I2C_RECEIVER, echo_buffer, command->bit_pattern_length);
	    if (status != HAL_OK) {
	        printf("Failed to start slave receive DMA: %d\n\r", status); // Debug printf
	        return TEST_FAIL;
	    }

	    // --- 2. TRANSMIT a block of data via DMA (MASTER) ---
	    status = HAL_I2C_Master_Transmit_DMA(I2C_SENDER, I2C_SLAVE_ADDR, tx_buffer, command->bit_pattern_length);
	    if (status != HAL_OK) {
	        printf("Failed to send DMA on I2C sender: %d\n\r", status); // Debug printf
	        i2c_reset(I2C_SENDER); // Reset the Master on error
	        i2c_reset(I2C_RECEIVER); // Reset the Slave as a precaution
	        return TEST_FAIL;
	    }

	    // --- 3. WAIT FOR BOTH TX DMA COMPLETION ---
	    if (xSemaphoreTake(I2cTxHandle, TIMEOUT) != pdPASS) {
	         printf("Master TX timeout\n\r"); // Debug printf
	         i2c_reset(I2C_SENDER); // Reset the Master on timeout
	         i2c_reset(I2C_RECEIVER); // Reset the Slave as a precaution
	         return TEST_FAIL;
	    }
        else
        {
        	HAL_Delay(1);

        	status = HAL_I2C_Slave_Transmit_IT(I2C_RECEIVER, echo_buffer, command->bit_pattern_length);
			 if (status != HAL_OK){
				 printf("Failed to echo send on I2C receiver: %d\n\r", status);
				 i2c_reset(I2C_SENDER); // Reset the Master on timeout
				 i2c_reset(I2C_RECEIVER); // Reset the Slave as a precaution
				 return TEST_FAIL;
			 }
        	// Arm sender receive before receiver transmits back
			 status = HAL_I2C_Master_Receive_IT(I2C_SENDER, I2C_SLAVE_ADDR, rx_buffer, command->bit_pattern_length);
			if (status != HAL_OK) {
				printf("Sender Failed to start receive back: %d\n\r", status);
				return TEST_FAIL;
			}

        }
	    //  WAIT FOR BOTH RX DMA COMPLETION
	    if (xSemaphoreTake(I2cRxHandle, TIMEOUT) != pdPASS) {
	         printf("Slave RX timeout\n\r"); // Debug printf
			 i2c_reset(I2C_SENDER); // Reset the Master on timeout
	         i2c_reset(I2C_RECEIVER); // Reset the Slave as a precaution
	         return TEST_FAIL;
	    }

	    // --- 4. COMPARE SENT vs. RECEIVED data ---
	    if (command->bit_pattern_length > 100) {
	        uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
	        uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
	        if (sent_crc != received_crc) {
//	            printf("I2C_TEST: CRC mismatch on iteration %u.\n\r", i + 1); // Debug printf
	            return TEST_FAIL;
	        }
	    } else {
	        int comp = memcmp(tx_buffer, rx_buffer, command->bit_pattern_length);
	        if (comp != 0) {
	            printf("Data mismatch on iteration %u.\n\r", i + 1); // Debug printf
	            return TEST_FAIL;
	        }
	    }
//	    printf("Data Match on iteration %u.\n\r", i + 1); // Debug printf

        osDelay(10);
	}
    return TEST_PASS;
}


void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hi2c->Instance == I2C_SENDER->Instance) // Check the instance of your sender UART
    {
//        printf("Master TX callback fired\n\r"); // Debug printf
        xSemaphoreGiveFromISR(I2cTxHandle, &xHigherPriorityTaskWoken);
    }
    else
    {
    	UNUSED(hi2c);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hi2c->Instance == I2C_SENDER->Instance) // Check the instance of your sender UART
    {
//        printf("Master RX callback fired\n\r"); // Debug printf
        xSemaphoreGiveFromISR(I2cRxHandle, &xHigherPriorityTaskWoken);
    }
    else
    {
    	UNUSED(hi2c);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
// reset the I2C peripheral
void i2c_reset(I2C_HandleTypeDef *hi2c) {
    if (HAL_I2C_DeInit(hi2c) != HAL_OK) {
        // Log a fatal error, the peripheral is in an unrecoverable state
        Error_Handler();
//        printf("Failed to de-initialize I2C peripheral!\n\r"); // Debug printf
    }
    if (HAL_I2C_Init(hi2c) != HAL_OK) {
        // Log a fatal error
        Error_Handler();
//        printf("Failed to re-initialize I2C peripheral!\n\r"); // Debug printf
    }
}

