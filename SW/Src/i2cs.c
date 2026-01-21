/**
 * @file i2cs.c
 * @brief I2C peripheral verification logic using Master-Slave loopback.
 * * Hardware Connection Requirement:
 * I2C4 (master)                I2C1 (slave)
 * PF14 SCL (CN10) <----------> PB8 SCL (CN7)
 * PF15 SDA (CN10) <----------> PB9 SDA (CN7) */

#include "i2cs.h"

#define I2C_SENDER      (&hi2c4)   // Master instance
#define I2C_RECEIVER    (&hi2c1)   // Slave instance
#define I2C_SLAVE_ADDR  (120 << 1) // 7-bit address left-shifted for HAL compatibility

/**
 * @brief Performs a hardware verification test on the I2C peripherals.
 * * This test transmits a bit pattern from the Master to the Slave using DMA,
 * echoes the data back, and verifies integrity using memcmp or CRC.
 * * @param command Pointer to the test_command_t structure.
 * @return Result TEST_PASS on success, TEST_FAIL on mismatch, or TEST_ERR on invalid input.
 */
Result i2c_testing(test_command_t* command) {

    // Local buffers for transfer validation
    uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
    uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
    uint8_t echo_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
    HAL_StatusTypeDef status;

    if (command == NULL) {
        return TEST_ERR;
    }

    // Initialize the transmit buffer with the command pattern
    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

    for (uint8_t i = 0; i < command->iterations; i++) {
        memset(rx_buffer, 0, command->bit_pattern_length);

        // --- 1. Prepare Slave for Reception (DMA Mode) ---
        status = HAL_I2C_Slave_Receive_DMA(I2C_RECEIVER, echo_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            return TEST_FAIL;
        }

        // --- 2. Master Transmits Pattern (DMA Mode) ---
        status = HAL_I2C_Master_Transmit_DMA(I2C_SENDER, I2C_SLAVE_ADDR, tx_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            i2c_reset(I2C_SENDER);
            i2c_reset(I2C_RECEIVER);
            return TEST_FAIL;
        }

        // Wait for Master Transmission to complete
        if (xSemaphoreTake(I2cTxHandle, TIMEOUT) != pdPASS) {
            i2c_reset(I2C_SENDER);
            i2c_reset(I2C_RECEIVER);
            return TEST_FAIL;
        }

        // Small delay to ensure Slave DMA processing is finalized
        HAL_Delay(1);

        // --- 3. Echo Phase: Slave Transmits back to Master (Interrupt Mode) ---
        status = HAL_I2C_Slave_Transmit_IT(I2C_RECEIVER, echo_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            i2c_reset(I2C_SENDER);
            i2c_reset(I2C_RECEIVER);
            return TEST_FAIL;
        }

        // Master receives the echoed data
        status = HAL_I2C_Master_Receive_IT(I2C_SENDER, I2C_SLAVE_ADDR, rx_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            return TEST_FAIL;
        }

        // Wait for the Echo reception to complete
        if (xSemaphoreTake(I2cRxHandle, TIMEOUT) != pdPASS) {
            i2c_reset(I2C_SENDER);
            i2c_reset(I2C_RECEIVER);
            return TEST_FAIL;
        }

        // --- 4. Data Integrity Validation ---
        if (command->bit_pattern_length > 100) {
            // CRC check for large data blocks (>100 bytes) per project spec
            uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
            uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
            if (sent_crc != received_crc) {
                return TEST_FAIL;
            }
        } else {
            // Direct memory comparison for smaller blocks
            if (memcmp(tx_buffer, rx_buffer, command->bit_pattern_length) != 0) {
                return TEST_FAIL;
            }
        }

        osDelay(1); // Inter-iteration pacing
    }
    return TEST_PASS;
}

/**
 * @brief Master Transmission Complete Callback.
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hi2c->Instance == I2C_SENDER->Instance) {
        xSemaphoreGiveFromISR(I2cTxHandle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Master Reception Complete Callback.
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hi2c->Instance == I2C_SENDER->Instance) {
        xSemaphoreGiveFromISR(I2cRxHandle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Re-initializes I2C peripheral to recover from error states.
 */
void i2c_reset(I2C_HandleTypeDef *hi2c) {
    HAL_I2C_DeInit(hi2c);
    HAL_I2C_Init(hi2c);
}
