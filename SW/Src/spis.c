// spis.c
#include "spis.h"
#include "cmsis_os.h"
#include "stm32f7xx_hal.h" // או ה־HAL המתאים למערכת שלך
#include <string.h>
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi4;

///*
// * SPI mapping in your project:
// * SPI_SENDER  -> hspi1 (Master)
// * SPI_RECEIVER-> hspi4 (Slave)

//	* SPI1 Master                 SPI4 Slave
//	* PA5 SCK   (CN7)  <--------> PE2 SCK  (CN9)
//	* PA6 MISO  (CN7)  ---------> PE5 MISO (CN9)
//	* PB5 MOSI  (CN7)  <--------- PE6 MOSI (CN9)
// */

// Buffers (global)
uint8_t echo_rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
uint8_t echo_tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

// Forward declarations
static HAL_StatusTypeDef safe_start_receive_dma(SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len);
static HAL_StatusTypeDef safe_start_transmit_dma(SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len);
static HAL_StatusTypeDef safe_start_transmitreceive_dma(SPI_HandleTypeDef *hspi, uint8_t *tx, uint8_t *rx, uint16_t len);

/**
 * Try to start a Receive DMA; if HAL_BUSY try a few times with small delay.
 */
static HAL_StatusTypeDef safe_start_receive_dma(SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st;
    for (int i = 0; i < RETRY_COUNT; ++i)
    {
        st = HAL_SPI_Receive_DMA(hspi, buf, len);
        if (st == HAL_OK) return HAL_OK;
        if (st == HAL_BUSY) {
            osDelay(RETRY_DELAY_MS);
            continue;
        }
        return st;
    }
    return HAL_BUSY;
}

static HAL_StatusTypeDef safe_start_transmit_dma(SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st;
    for (int i = 0; i < RETRY_COUNT; ++i)
    {
        st = HAL_SPI_Transmit_DMA(hspi, buf, len);
        if (st == HAL_OK) return HAL_OK;
        if (st == HAL_BUSY) {
            osDelay(RETRY_DELAY_MS);
            continue;
        }
        return st;
    }
    return HAL_BUSY;
}

static HAL_StatusTypeDef safe_start_transmitreceive_dma(SPI_HandleTypeDef *hspi, uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_StatusTypeDef st;
    for (int i = 0; i < RETRY_COUNT; ++i)
    {
        st = HAL_SPI_TransmitReceive_DMA(hspi, tx, rx, len);
        if (st == HAL_OK) return HAL_OK;
        if (st == HAL_BUSY) {
            osDelay(RETRY_DELAY_MS);
            continue;
        }
        return st;
    }
    return HAL_BUSY;
}

/*
 * Main test function (תיקנתי וניקיתי מעט כדי שיהיה יותר יציב)
 */
Result spi_testing(test_command_t* command)
{
    static uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
    static uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

    HAL_StatusTypeDef status;

    if (command == NULL) {
        printf("SPI_TEST: Received NULL command pointer. Skipping.\n");
        return TEST_ERR;
    }

    if (command->bit_pattern_length == 0 || command->bit_pattern_length > MAX_BIT_PATTERN_LENGTH) {
        printf("SPI_TEST: invalid pattern length %u\n", command->bit_pattern_length);
        return TEST_ERR;
    }
//    HAL_SPI_DMA
    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

    for (uint8_t iter = 0; iter < command->iterations; ++iter)
    {
        printf("SPI_TEST: Iteration %u/%u\n\r", iter + 1, command->iterations);
        memset(rx_buffer, 0, command->bit_pattern_length);

        // Reset and prepare for new iteration
        reset_test();

        // Ensure slave listening buffer is cleared and prepared
        // Start Slave to receive (prepare to capture master's frame)
        status = safe_start_receive_dma(SPI_RECEIVER, echo_rx_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            printf("SPI_TEST: Failed to start slave receive (pre-master): %d\n\r", status);
            reset_test();
            return TEST_FAIL;
        }
        osDelay(1);
        // Assert CS (active low)
        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

        // Start Master transmit+receive (master exchanges simultaneously)
        status = safe_start_transmitreceive_dma(SPI_SENDER, tx_buffer, rx_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            printf("SPI_TEST: Failed to start master TxRx: %d\n\r", status);
            HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
            reset_test();
            return TEST_FAIL;
        }

        // Wait for master TxRx complete (gives SpiTxHandle in callback)
        if (xSemaphoreTake(SpiTxHandle, TIMEOUT) != pdPASS) {
            printf("SPI_TEST: Master TX timeout\n\r");
            HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
            reset_test();
            return TEST_FAIL;
        }

        // Wait for slave to have captured master's frame (slave Rx callback or TxRx)
        if (xSemaphoreTake(SpiSlaveRxHandle, TIMEOUT) != pdPASS) {
            printf("SPI_TEST: Slave RX timeout (after master send)\n\r");
            HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
            reset_test();
            return TEST_FAIL;
        }

        // Deassert CS to end the first transaction
        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

        // Copy the captured data into echo_tx_buffer (performed in callback too, but safe here)
        // make sure caches coherent before using echo_tx_buffer as DMA source
        SCB_CleanDCache_by_Addr((uint32_t*)echo_rx_buffer, command->bit_pattern_length);
        memcpy(echo_tx_buffer, echo_rx_buffer, command->bit_pattern_length);

        // --- Echo phase: Master receives echoed data while Slave transmits echo ---
        osDelay(1);
        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

        // Invalidate master's rx buffer cache if needed
        SCB_InvalidateDCache_by_Addr((uint32_t*)rx_buffer, command->bit_pattern_length);

        clear_flags(SPI_RECEIVER);
//        HAL_SPI_Abort(SPI_RECEIVER);

        // Prepare Master to receive (use Receive DMA)
        status = safe_start_receive_dma(SPI_SENDER, rx_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            printf("SPI_TEST: Failed to start master Receive (echo): %d\n\r", status);
            HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
            reset_test();
            return TEST_FAIL;
        }

        // Start Slave transmit of the echo buffer
        status = safe_start_transmit_dma(SPI_RECEIVER, echo_tx_buffer, command->bit_pattern_length);
        if (status != HAL_OK) {
            printf("SPI_TEST: Failed to start slave Transmit (echo): %d\n\r", status);
            HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
            reset_test();
            return TEST_FAIL;
        }

        // Wait for master Rx complete
        if (xSemaphoreTake(SpiRxHandle, TIMEOUT) != pdPASS) {
            printf("SPI_TEST: Master RX timeout (echo)\n\r");
            HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
            reset_test();
            return TEST_FAIL;
        }

        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

        // Invalidate DCache for master's rx buffer before comparing
        SCB_InvalidateDCache_by_Addr((uint32_t*)rx_buffer, command->bit_pattern_length);

        // Compare
        if (command->bit_pattern_length > 100) {
            uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
            uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
            if (sent_crc != received_crc) {
                printf("SPI_TEST: CRC mismatch on iteration %u.\n", iter + 1);
                reset_test();
                return TEST_FAIL;
            }
        } else {
            if (memcmp(tx_buffer, rx_buffer, command->bit_pattern_length) != 0) {
                printf("SPI_TEST: Data mismatch on iteration %u.\n", iter + 1);
                printf("Sent: %.*s\n", command->bit_pattern_length, tx_buffer);
                printf("Recv: %.*s\n", command->bit_pattern_length, rx_buffer);
                reset_test();
                return TEST_FAIL;
            }
        }

        printf("SPI_TEST: Data Match on iteration %u.\n", iter + 1);
        osDelay(10);
    }

    return TEST_PASS;
}

/* ---- Callbacks ---- */

/* Rx complete callback (called from HAL ISR) */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hspi == SPI_RECEIVER)
    {
        // Slave Received data into echo_rx_buffer
        xSemaphoreGiveFromISR(SpiSlaveRxHandle, &xHigherPriorityTaskWoken);

        // Re-arm the Slave receive for next frame (non-blocking, safe retry if busy)
        // Note: We don't block here; if busy, safe_start_receive will attempt later when reset_test called
        HAL_SPI_Receive_DMA(SPI_RECEIVER, echo_rx_buffer, MAX_BIT_PATTERN_LENGTH);
    }
    else if (hspi == SPI_SENDER)
    {
        xSemaphoreGiveFromISR(SpiRxHandle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* TxRx complete callback */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hspi == SPI_RECEIVER)
    {
        // Slave finished a TxRx operation (if used)
        // Copy received into echo_tx buffer for echoing if needed
        // Use RxXferSize to know length (if available)
        uint16_t len = (uint16_t)hspi->RxXferSize;
        if (len > 0 && len <= MAX_BIT_PATTERN_LENGTH) {
            memcpy(echo_tx_buffer, echo_rx_buffer, len);
        }
        xSemaphoreGiveFromISR(SpiSlaveRxHandle, &xHigherPriorityTaskWoken);
    }
    else if (hspi == SPI_SENDER)
    {
        // Master finished TxRx (initial transmit)
        xSemaphoreGiveFromISR(SpiTxHandle, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Error callback */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == SPI_RECEIVER)
    {
        if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_OVR))
        {
            __HAL_SPI_CLEAR_OVRFLAG(hspi);
            HAL_SPIEx_FlushRxFifo(hspi);
        }
    }
    // optionally notify tasks or log
}

/* ---- Utilities ---- */

/* clear_flags: try to leave SPI in clean state (avoid unnecessary aborts) */
void clear_flags(SPI_HandleTypeDef *hspi)
{
    // Only abort if not ready
    if (hspi->State != HAL_SPI_STATE_READY) {
        HAL_SPI_Abort(hspi);
    }
    __HAL_SPI_CLEAR_OVRFLAG(hspi);
    HAL_SPIEx_FlushRxFifo(hspi);
}

/* reset_test: robust reset between iterations or on error */
void reset_test(void)
{
    // Abort if necessary
    if (SPI_SENDER->State != HAL_SPI_STATE_READY) {
        HAL_SPI_Abort(SPI_SENDER);
    }
    if (SPI_RECEIVER->State != HAL_SPI_STATE_READY) {
        HAL_SPI_Abort(SPI_RECEIVER);
    }

    // Drain semaphores (non-blocking)
    while (xSemaphoreTake(SpiTxHandle, 0) == pdTRUE) {}
    while (xSemaphoreTake(SpiRxHandle, 0) == pdTRUE) {}
    while (xSemaphoreTake(SpiSlaveRxHandle, 0) == pdTRUE) {}

    // Clear buffers
    memset(echo_rx_buffer, 0, sizeof(echo_rx_buffer));
    memset(echo_tx_buffer, 0, sizeof(echo_tx_buffer));

    // Clear flags/fifos
    __HAL_SPI_CLEAR_OVRFLAG(SPI_SENDER);
    __HAL_SPI_CLEAR_OVRFLAG(SPI_RECEIVER);
    HAL_SPIEx_FlushRxFifo(SPI_RECEIVER);

    // Re-arm slave for next incoming frame
    // Use maximum supported length to be ready; actual compare uses command length.
    //HAL_SPI_Receive_DMA(SPI_RECEIVER, echo_rx_buffer, MAX_BIT_PATTERN_LENGTH);
}


