//#include "spis.h"
//#include "cmsis_os.h"
//#include "stm32f7xx_hal.h"
//#include <string.h>
//#include <stdio.h>
//
//extern SPI_HandleTypeDef hspi1;
//extern SPI_HandleTypeDef hspi4;
//
/////*
//// * SPI1 Master SPI_SENDER    SPI4 Slave SPI_RECEIVER
//// * PA5 SCK (CN7)  <--------> PE2 SCK (CN9)
//// * PA6 MISO (CN7) ---------> PE5 MISO (CN9)
//// * PB5 MOSI (CN7) <--------- PE6 MOSI (CN9)
//// */
//static uint8_t echo_rx_buffer[MAX_BIT_PATTERN_LENGTH] __attribute__((aligned(32)));
//static uint8_t echo_tx_buffer[MAX_BIT_PATTERN_LENGTH] __attribute__((aligned(32)));
//static uint8_t master_tx[MAX_BIT_PATTERN_LENGTH] __attribute__((aligned(32)));
//static uint8_t master_rx[MAX_BIT_PATTERN_LENGTH] __attribute__((aligned(32)));
///*
//* Main test function
//*/
//Result spi_testing(test_command_t* command)
//{
//	if (command == NULL || command->bit_pattern_length > MAX_BIT_PATTERN_LENGTH) return TEST_ERR;
//	uint16_t len = command->bit_pattern_length;
//
//	memcpy(master_tx, command->bit_pattern, len);
//
//	for (uint8_t iter = 0; iter < command->iterations; ++iter)
//	{
//		reset_test(); // Ensure flags are clear and DMA is aborted
//		memset(master_rx, 0, len);
//		memset(echo_rx_buffer, 0, len);
//		memset(echo_tx_buffer, 0, len);
//
//		// --- PHASE 1: Master -> Slave ---
//		// 1. Arm Slave first (Non-blocking)
//		HAL_SPI_Receive_DMA(SPI_RECEIVER, echo_rx_buffer, len);
//		osDelay(1); // Small breath to ensure Slave DMA is ready
//
//		// 2. Master Sends (Blocking is okay here for simplicity)
//		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
//		HAL_SPI_Transmit(SPI_SENDER, master_tx, len, TIMEOUT);
//		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
//
//		// 3. Wait for Slave to confirm receipt via ISR semaphore
//		if (xSemaphoreTake(SpiSlaveRxHandle, TIMEOUT) != pdPASS) {
//			printf("Iteration %d: Slave RX Timeout.\r\n", iter);
//			return TEST_FAIL;
//		}
//
//		// --- PHASE 2: Slave -> Master (Echo) ---
//		SCB_InvalidateDCache_by_Addr((uint32_t*)echo_rx_buffer, (len + 31) & ~31);
//		// 1. Prepare Slave to send the echo back
//		memcpy(echo_tx_buffer, echo_rx_buffer, len);
//
//		SCB_CleanDCache_by_Addr((uint32_t*)echo_tx_buffer, (len + 31) & ~31);
//		// We use TransmitReceive so the Slave can "swap" data
//		HAL_SPI_TransmitReceive_DMA(SPI_RECEIVER, echo_tx_buffer, echo_rx_buffer, len);
//		osDelay(1);
//
//		// 2. Master provides the clock to "pull" the echo
//		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
//		HAL_SPI_Receive(SPI_SENDER, master_rx, len, TIMEOUT);
//		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
//
//		// Invalidate cache AFTER DMA is done so CPU reads new RAM data
//		SCB_InvalidateDCache_by_Addr((uint32_t*)master_rx, len);
//
//		// 3. Wait for Slave to finish shifting out
//		if (xSemaphoreTake(SpiRxHandle, TIMEOUT) != pdPASS) {
//			printf("Iteration %d: Master RX Timeout. SR: 0x%08lx\n", iter, SPI_RECEIVER->Instance->SR);
//			return TEST_FAIL;
//
//		}
//
//		// --- Validation ---
//		if (memcmp(master_tx, master_rx, len) != 0) {
//			printf("Data Mismatch on Iteration %d\n", iter);
//			return TEST_FAIL;
//		}
//
//		printf("Iteration %d Passed\n", iter);
//
//	}
//	return TEST_PASS;
//}

//--------------------------------------------------------------------
#include "spis.h"
#include "cmsis_os.h"
#include "stm32f7xx_hal.h"
#include <string.h>
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi4;

// Use a macro to ensure all DMA buffers are 32-byte aligned for the M7 Cache
#define ALIGN_32 __attribute__((aligned(32)))
// Cache maintenance must always happen on 32-byte blocks
#define CACHE_ROUND(x) (((x) + 31) & ~31)

/* * Static buffers with fixed sizes to fix compiler error
 * and 32-byte alignment to prevent cache corruption.
 */
static uint8_t echo_rx_buffer[MAX_BIT_PATTERN_LENGTH] ALIGN_32;
static uint8_t echo_tx_buffer[MAX_BIT_PATTERN_LENGTH] ALIGN_32;
static uint8_t master_tx[MAX_BIT_PATTERN_LENGTH] ALIGN_32;
static uint8_t master_rx[MAX_BIT_PATTERN_LENGTH] ALIGN_32;

Result spi_testing(test_command_t* command)
{
    if (command == NULL || command->bit_pattern_length > MAX_BIT_PATTERN_LENGTH) return TEST_ERR;

    uint16_t len = command->bit_pattern_length;
    uint32_t clean_len = CACHE_ROUND(len);

    memcpy(master_tx, command->bit_pattern, len);
    // Clean master_tx so DMA reads the pattern we just copied, not old RAM 0s
    SCB_CleanDCache_by_Addr((uint32_t*)master_tx, clean_len);

    for (uint8_t iter = 0; iter < command->iterations; ++iter)
    {
        reset_test();
        memset(master_rx, 0, len);
        memset(echo_rx_buffer, 0, len);

        // Ensure cache is clear of the 0s we just wrote with memset
        SCB_CleanInvalidateDCache_by_Addr((uint32_t*)echo_rx_buffer, clean_len);
        SCB_CleanInvalidateDCache_by_Addr((uint32_t*)master_rx, clean_len);

        // --- PHASE 1: Master -> Slave ---
        HAL_SPI_Receive_DMA(SPI_RECEIVER, echo_rx_buffer, len);
        osDelay(2);

        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
        HAL_SPI_Transmit(SPI_SENDER, master_tx, len, TIMEOUT);
        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

        if (xSemaphoreTake(SpiSlaveRxHandle, TIMEOUT) != pdPASS) {
            printf("Iteration %d: Slave RX Timeout.\r\n", iter);
            return TEST_FAIL;
        }

        // --- CRITICAL FIX: Invalidate BEFORE memcpy ---
        // This forces the CPU to see the data the DMA just put in RAM
        SCB_InvalidateDCache_by_Addr((uint32_t*)echo_rx_buffer, clean_len);

        memcpy(echo_tx_buffer, echo_rx_buffer, len);

        // Clean echo_tx_buffer so DMA sees our memcpy result
        SCB_CleanDCache_by_Addr((uint32_t*)echo_tx_buffer, clean_len);

        // --- PHASE 2: Slave -> Master (Echo) ---
        // CORRECTED: Transmit from tx_buffer, receive into rx_buffer
        HAL_SPI_TransmitReceive_DMA(SPI_RECEIVER, echo_tx_buffer, echo_rx_buffer, len);
        osDelay(2);

        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
        HAL_SPI_Receive(SPI_SENDER, master_rx, len, TIMEOUT);
        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

        // Final Invalidate so CPU reads the echoed data from RAM
        SCB_InvalidateDCache_by_Addr((uint32_t*)master_rx, clean_len);

        if (xSemaphoreTake(SpiRxHandle, TIMEOUT) != pdPASS) {
            printf("Iteration %d: Master RX Timeout.\r\n", iter);
            return TEST_FAIL;
        }

        // --- Validation ---
        if (memcmp(master_tx, master_rx, len) != 0) {
            printf("Data Mismatch on Iteration %d\n", iter);
            return TEST_FAIL;
        }
        printf("Iteration %d Passed\n", iter);
    }
    return TEST_PASS;
}

/* --- Utilities --- */
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

    while (xSemaphoreTake(SpiSlaveRxHandle, 0) == pdTRUE);
    while (xSemaphoreTake(SpiRxHandle, 0) == pdTRUE);
}
//--------------------------------------------------------------------

/* ---- Callbacks ---- */

/* Rx complete callback (called from HAL ISR) */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if (hspi == SPI_RECEIVER)
	{
		// Slave Received data into echo_rx_buffer
		xSemaphoreGiveFromISR(SpiSlaveRxHandle, &xHigherPriorityTaskWoken);
	}
	else if (hspi == SPI_SENDER)
	{
		// Master Received data
//		xSemaphoreGiveFromISR(SpiRxHandle, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/* TxRx complete callback */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if (hspi == SPI_RECEIVER)
	{
		xSemaphoreGiveFromISR(SpiRxHandle, &xHigherPriorityTaskWoken);
	}
	else if (hspi == SPI_SENDER)
	{
		// Master finished TxRx (initial transmit)
		// xSemaphoreGiveFromISR(SpiTxHandle, &xHigherPriorityTaskWoken);
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
//void reset_test(void)
//{
//    HAL_SPI_Abort(SPI_SENDER);
//    HAL_SPI_Abort(SPI_RECEIVER);
//
//    // Hard reset the SPI peripheral state machines
//    __HAL_SPI_DISABLE(SPI_RECEIVER);
//    __HAL_SPI_DISABLE(SPI_SENDER);
//
//    __HAL_SPI_CLEAR_OVRFLAG(SPI_RECEIVER);
//    __HAL_SPI_CLEAR_FREFLAG(SPI_RECEIVER);
//
//    HAL_SPIEx_FlushRxFifo(SPI_RECEIVER);
//    HAL_SPIEx_FlushRxFifo(SPI_SENDER);
//
//    __HAL_SPI_ENABLE(SPI_RECEIVER);
//    __HAL_SPI_ENABLE(SPI_SENDER);
//
//    // Drain semaphores
//    while (xSemaphoreTake(SpiSlaveRxHandle, 0) == pdTRUE);
//    while (xSemaphoreTake(SpiRxHandle, 0) == pdTRUE);
//}
