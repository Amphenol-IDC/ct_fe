#include <stdio.h>
#include "connectivity_tester.h"
#include "gpio_channels.h"
#include "spi_regs.h"

#define RX_CHECK_TASK_STACK_SIZE  1024U
#define RX_CHECK_TASK_PRIORITY    10U
#define RX_PAIR_CHECK_POLL_TICKS  1U
#define RX_PAIR_RESULT_MASK       (SPI_REG_HOST_RX_MASK | SPI_REG_LANE_RX_MASK)
#define TX_PAIR_CTRL_MASK         (SPI_REG_HOST_TX_MASK | SPI_REG_LANE_TX_MASK)
static TX_THREAD rxCheckTaskHandle;
static ULONG rxCheckTaskStack[RX_CHECK_TASK_STACK_SIZE / sizeof(ULONG)];

static void ConnectivityTester_ApplyTxPattern(uint32_t active_tx_mask, uint8_t bit_value)
{
    for (uint8_t i = 0; i < GPIO_CH_PER_GROUP; i++)
    {
        uint32_t host_tx_bit = (SPI_REG_CHANNEL_BIT(i) << SPI_REG_HOST_TX_SHIFT);
        uint32_t lane_tx_bit = (SPI_REG_CHANNEL_BIT(i) << SPI_REG_LANE_TX_SHIFT);

        GpioChannels_WriteTxBit(GPIO_CH_HOST_TX(i), (active_tx_mask & host_tx_bit) ? bit_value : 0U);
        GpioChannels_WriteTxBit(GPIO_CH_LANE_TX(i), (active_tx_mask & lane_tx_bit) ? bit_value : 0U);
    }
}

/**
 ******************************************************************************
 * @file    rx_pair_checker_thread.c
 * @brief   ThreadX polling task for RX pair complementary check
 *
 * Result format is aligned to the SPI register layout:
 *   bits [31:24] Host RX fault bits (1 = non-complementary)
 *   bits [23:16] Host TX bits forced to 0
 *   bits [15:8]  Lane RX fault bits (1 = non-complementary)
 *   bits [7:0]   Lane TX bits forced to 0
 ******************************************************************************
 */
void ConnectivityTesterTask(ULONG argument)
{
    (void) argument;
    uint32_t active_tx_mask = 0;
    uint8_t tx_toggle_value = 0;
    uint32_t ctrl_value;
    uint32_t stop_value;
    uint32_t prev_rx_only_result = 0;

    /* Configure only TX pairs as outputs; RX pin setup remains under IOC/user control. */
    GpioChannels_ConfigTxOutputs();

    for (;;)
    {
        if (SpiRegs_GetCtrl(&ctrl_value))
        {
            active_tx_mask |= (ctrl_value & TX_PAIR_CTRL_MASK);
        }

        if (SpiRegs_GetStop(&stop_value))
        {
            active_tx_mask &= ~(stop_value & TX_PAIR_CTRL_MASK);
        }

        ConnectivityTester_ApplyTxPattern(active_tx_mask, tx_toggle_value);
        tx_toggle_value ^= 1U;

        uint32_t faults = GpioChannels_CheckAllFaults();
        uint32_t rx_only_result = faults & RX_PAIR_RESULT_MASK;

        if (rx_only_result != prev_rx_only_result)
        {
// return debugging when 64 gpio_channel_table[] is properly defined:  printf("RX fault result changed: 0x%08lX\r\n", (unsigned long)rx_only_result);
            prev_rx_only_result = rx_only_result;
        }

        SpiRegs_SetRxResult(rx_only_result);

        tx_thread_sleep(RX_PAIR_CHECK_POLL_TICKS);
    }
}

void ConnectivityTesterInit(void)
{
    printf("Starting ConnectivityTesterTask\r\n");

    if (tx_thread_create(&rxCheckTaskHandle, "ConnectivityTesterTask", ConnectivityTesterTask, 0,
            rxCheckTaskStack, sizeof(rxCheckTaskStack),
            RX_CHECK_TASK_PRIORITY, RX_CHECK_TASK_PRIORITY,
            TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        printf("Failed to create ConnectivityTesterTask\r\n");
    }
}
