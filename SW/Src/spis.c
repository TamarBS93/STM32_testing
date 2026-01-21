/**
 * @file spis.c
 * @brief SPI peripheral verification logic using Master-Slave DMA loopback.
 * * Hardware Connections:
 * SPI1 [Master] (PA5, PA6, PB5) <-> SPI4 [Slave] (PE2, PE5, PE6)
 * SPI1 (Master)             SPI4 (Slave)
 * PA5 SCK (CN7)  <--------> PE2 SCK (CN9)
 * PA6 MISO (CN7) ---------> PE5 MISO (CN9)
 * PB5 MOSI (CN7) <--------- PE6 MOSI (CN9)
 */

#include "spis.h"

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi4;

/**
 * @brief Macro for 32-byte alignment to match Cortex-M7 Cache line size.
 */
#define ALIGN_32 __attribute__((aligned(32)))

/**
 * @brief Macro to round length up to the nearest 32-byte boundary for cache maintenance.
 */
#define CACHE_ROUND(x) (((x) + 31) & ~31)

/* * DMA Buffers
 * Static allocation ensures persistence during transfer.
 * Alignment ensures cache operations do not corrupt adjacent variables.
 */
static uint8_t echo_rx_buffer[MAX_BIT_PATTERN_LENGTH] ALIGN_32;
static uint8_t echo_tx_buffer[MAX_BIT_PATTERN_LENGTH] ALIGN_32;
static uint8_t master_tx[MAX_BIT_PATTERN_LENGTH] ALIGN_32;
static uint8_t master_rx[MAX_BIT_PATTERN_LENGTH] ALIGN_32;

/**
 * @brief Performs hardware verification on SPI peripherals.
 * * This test uses a two-phase DMA approach:
 * Phase 1: Master transmits a pattern to the Slave.
 * Phase 2: Slave echoes the pattern back to the Master.
 * * @param command Pointer to test parameters (ID, iterations, pattern).
 * @return Result TEST_PASS on successful echo, TEST_FAIL on mismatch/timeout.
 */
Result spi_testing(test_command_t* command)
{
    if (command == NULL || command->bit_pattern_length > MAX_BIT_PATTERN_LENGTH) return TEST_ERR;

    uint16_t len = command->bit_pattern_length;
    uint32_t clean_len = CACHE_ROUND(len);

    // Prepare initial pattern
    memcpy(master_tx, command->bit_pattern, len);

    // Clean cache to push master_tx from CPU L1 to RAM where DMA can access it
    SCB_CleanDCache_by_Addr((uint32_t*)master_tx, clean_len);

    for (uint8_t iter = 0; iter < command->iterations; ++iter)
    {
        reset_test();
        memset(master_rx, 0, len);
        memset(echo_rx_buffer, 0, len);

        // Ensure memory regions are ready for DMA by clearing stale cache lines
        SCB_CleanInvalidateDCache_by_Addr((uint32_t*)echo_rx_buffer, clean_len);
        SCB_CleanInvalidateDCache_by_Addr((uint32_t*)master_rx, clean_len);

        /* --- PHASE 1: Master -> Slave --- */
        HAL_SPI_Receive_DMA(SPI_RECEIVER, echo_rx_buffer, len);
        osDelay(2); // Wait for DMA setup

        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
        HAL_SPI_Transmit(SPI_SENDER, master_tx, len, TIMEOUT);
        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

        if (xSemaphoreTake(SpiSlaveRxHandle, TIMEOUT) != pdPASS) {
            return TEST_FAIL;
        }

        // --- Cache Sync: Force CPU to read RAM instead of potentially zeroed Cache ---
        SCB_InvalidateDCache_by_Addr((uint32_t*)echo_rx_buffer, clean_len);

        memcpy(echo_tx_buffer, echo_rx_buffer, len);

        // Clean cache to push echo_tx_buffer to RAM for the next DMA transfer
        SCB_CleanDCache_by_Addr((uint32_t*)echo_tx_buffer, clean_len);

        /* --- PHASE 2: Slave -> Master (Echo) --- */
        HAL_SPI_TransmitReceive_DMA(SPI_RECEIVER, echo_tx_buffer, echo_rx_buffer, len);
        osDelay(2);

        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
        HAL_SPI_Receive(SPI_SENDER, master_rx, len, TIMEOUT);
        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

        // Final Cache Sync for Master Reception
        SCB_InvalidateDCache_by_Addr((uint32_t*)master_rx, clean_len);

        if (xSemaphoreTake(SpiRxHandle, TIMEOUT) != pdPASS) {
            return TEST_FAIL;
        }

        // Final data validation
        if (memcmp(master_tx, master_rx, len) != 0) {
            return TEST_FAIL;
        }
    }
    return TEST_PASS;
}

/**
 * @brief Resets SPI peripherals and clears semaphores to ensure a clean state.
 */
void reset_test(void)
{
    HAL_SPI_Abort(SPI_SENDER);
    HAL_SPI_Abort(SPI_RECEIVER);

    __HAL_SPI_DISABLE(SPI_RECEIVER);
    __HAL_SPI_DISABLE(SPI_SENDER);

    __HAL_SPI_CLEAR_OVRFLAG(SPI_RECEIVER);
    __HAL_SPI_CLEAR_FREFLAG(SPI_RECEIVER);

    HAL_SPIEx_FlushRxFifo(SPI_RECEIVER);
    HAL_SPIEx_FlushRxFifo(SPI_SENDER);

    __HAL_SPI_ENABLE(SPI_RECEIVER);
    __HAL_SPI_ENABLE(SPI_SENDER);

    // Drain semaphores
    while (xSemaphoreTake(SpiSlaveRxHandle, 0) == pdTRUE);
    while (xSemaphoreTake(SpiRxHandle, 0) == pdTRUE);
}

/* ---- Callbacks ---- */

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hspi->Instance == SPI_RECEIVER->Instance)
    {
        xSemaphoreGiveFromISR(SpiSlaveRxHandle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hspi->Instance == SPI_RECEIVER->Instance)
    {
        xSemaphoreGiveFromISR(SpiRxHandle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI_RECEIVER->Instance)
    {
        if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_OVR))
        {
            __HAL_SPI_CLEAR_OVRFLAG(hspi);
            HAL_SPIEx_FlushRxFifo(hspi);
        }
    }
}
