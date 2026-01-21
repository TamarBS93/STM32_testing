/**
 * @file uarts.c
 * @brief UART peripheral verification logic using a dual-UART loopback.
 * * Hardware Connection Requirement:
 * UART2                    UART4
 * PD6 RX (CN9) <---------- PA0 TX (CN10)
 * PD5 TX (CN9) ----------> PC11 RX (CN8)
 */

#include "uarts.h"

#define UART_SENDER         (&huart2)
#define UART_RECEIVER       (&huart4)

/**
 * @brief Performs a hardware verification test on the UART peripherals.
 * * This test transmits a bit pattern from UART2 to UART4 using DMA.
 * UART4 then echoes the data back to UART2. Integrity is verified
 * via memory comparison or CRC for large blocks.
 * * @param command Pointer to the test_command_t structure.
 * @return Result TEST_PASS on success, TEST_FAIL on mismatch, TEST_ERR for invalid input.
 */
Result uart_testing(test_command_t* command){

    uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
    uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
    uint8_t echo_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

    HAL_StatusTypeDef status;

    if (command == NULL) {
        return TEST_ERR;
    }

    // Prepare the transmission pattern
    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

    for(uint8_t i=0 ; i < command->iterations ; i++){
        memset(rx_buffer, 0, command->bit_pattern_length);

        // --- 1. Prepare Receiver to receive the pattern (DMA Mode) ---
        status = HAL_UART_Receive_DMA(UART_RECEIVER, echo_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            return TEST_FAIL;
        }

        // --- 2. Prepare Sender to receive the echoed data (Interrupt Mode) ---
        if (HAL_UART_Receive_IT(UART_SENDER, rx_buffer, command->bit_pattern_length) != HAL_OK) {
            HAL_UART_Abort(UART_RECEIVER);
            return TEST_FAIL;
        }

        // --- 3. Transmit pattern from Sender (DMA Mode) ---
        status = HAL_UART_Transmit_DMA(UART_SENDER, tx_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            HAL_UART_Abort(UART_RECEIVER);
            return TEST_FAIL;
        }

        // Wait for Receiver to finish collecting the pattern via DMA
        if (xSemaphoreTake(UartTxHandle, TIMEOUT) != pdPASS) {
             HAL_UART_Abort(UART_RECEIVER);
             HAL_UART_Abort(UART_SENDER);
             return TEST_FAIL;
        }

        // --- 4. Echo Phase: Receiver transmits collected data back ---
        if (HAL_UART_Transmit_IT(UART_RECEIVER, echo_buffer, command->bit_pattern_length) != HAL_OK){
             HAL_UART_Abort(UART_RECEIVER);
             HAL_UART_Abort(UART_SENDER);
             return TEST_FAIL;
        }

        // Wait for Sender to finish receiving the echoed data
        if (xSemaphoreTake(UartRxHandle, TIMEOUT) != pdPASS) {
            HAL_UART_Abort(UART_SENDER);
            HAL_UART_Abort(UART_RECEIVER);
            return TEST_FAIL;
        }

        // --- 5. Data Validation ---
        if (command->bit_pattern_length > 100) {
            // CRC comparison for efficiency on large data sets
            uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
            uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
            if (sent_crc != received_crc) {
                return TEST_FAIL;
            }
        }
        else {
            // Direct memory comparison for standard pattern lengths
            if (memcmp(tx_buffer, rx_buffer, command->bit_pattern_length) != 0) {
                return TEST_FAIL;
            }
        }

        osDelay(1); // Inter-iteration pacing
    }
    return TEST_PASS;
}

/**
 * @brief UART Reception Complete Callback.
 * Signals the appropriate semaphore based on which peripheral finished receiving.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart->Instance == UART_RECEIVER->Instance)
    {
        // Receiver finished receiving data from Master
        xSemaphoreGiveFromISR(UartTxHandle, &xHigherPriorityTaskWoken);
    }
    else if (huart->Instance == UART_SENDER->Instance)
    {
        // Sender finished receiving the echo
        xSemaphoreGiveFromISR(UartRxHandle, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief UART Transmission Complete Callback.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    // Transmitter callbacks are handled via the Rx side semaphores in this echo logic
    UNUSED(huart);
}
