#include <stdio.h>
#include "connectivity_tester.h"
#include "gpio_channels.h"
#include "spi_regs.h"

#define RX_CHECK_TASK_STACK_SIZE  2048U
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

        /* When channel is ACTIVE in mask, drive with bit_value; otherwise set both P and N to RESET */
        if (active_tx_mask & host_tx_bit)
            GpioChannels_WriteTxBit(GPIO_CH_HOST_TX(i), bit_value);
        else
            GpioChannels_WriteTxReset(GPIO_CH_HOST_TX(i));

        if (active_tx_mask & lane_tx_bit)
            GpioChannels_WriteTxBit(GPIO_CH_LANE_TX(i), bit_value);
        else
            GpioChannels_WriteTxReset(GPIO_CH_LANE_TX(i));
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

    const SpiRegs_t *spi_ctx = SpiRegs_GetContext();
    uint32_t log_prev_ctrl_seq  = 0;
    uint32_t log_prev_stop_seq  = 0;
    uint32_t log_prev_rearm_cnt = 0;

    /* Configure only TX pairs as outputs; RX pin setup remains under IOC/user control. */
    GpioChannels_ConfigTxOutputs();

    for (;;)
    {
        /* Drain deferred SPI log events */
        if (spi_ctx->ctrl_write_seq != log_prev_ctrl_seq)
        {
            printf("[SPI] CTRL write received: 0x%08lX\r\n", (unsigned long)spi_ctx->ctrl);
            log_prev_ctrl_seq = spi_ctx->ctrl_write_seq;
        }
        if (spi_ctx->stop_write_seq != log_prev_stop_seq)
        {
            printf("[SPI] STOP write received: 0x%08lX\r\n", (unsigned long)spi_ctx->stop);
            log_prev_stop_seq = spi_ctx->stop_write_seq;
        }
        {
            uint32_t rearm = SpiRegs_GetRearmErrorCount();
            if (rearm != log_prev_rearm_cnt)
            {
                printf("[SPI] rearm error count changed: %lu\r\n", (unsigned long)rearm);
                log_prev_rearm_cnt = rearm;
            }
        }

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
