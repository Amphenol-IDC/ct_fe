/**
 ******************************************************************************
 * @file    gpio_channels.c
 * @brief   GPIO channel P/N pair implementation
 *
 * NOTE: The GPIO pin assignments below are PLACEHOLDERS for compilation.
 *       Replace with actual pin assignments from the .ioc file when ready.
 *
 * Channel layout (matching SPI register bit allocation):
 *   [0..7]   = Host Rx, channels 0-7
 *   [8..15]  = Host Tx, channels 0-7
 *   [16..23] = Lane Rx, channels 0-7
 *   [24..31] = Lane Tx, channels 0-7
 ******************************************************************************
 */

#include "gpio_channels.h"
#include "spi_regs.h"

#define GPIO_RX_FAULT_CONFIRM_SAMPLES  6U

/* A healthy RX pair carries a square wave, so its P line must keep toggling.
 * If P stays at the same level for this many consecutive samples, the pair is
 * considered stuck (e.g. P always '1', N always '0') and reported as faulty,
 * even though P != N. */
#define GPIO_RX_ACTIVITY_TIMEOUT_SAMPLES  10U

static uint8_t rx_fault_streak[GPIO_CH_TOTAL] = {0};  /* Counts consecutive samples of P==N for each channel */
static uint8_t rx_fault_active[GPIO_CH_TOTAL] = {0};  /* Latched fault state, cleared only when streak returns to 0 */

static uint8_t rx_prev_p[GPIO_CH_TOTAL]     = {0};  /* Previous P level, used to detect toggling activity */
static uint8_t rx_prev_p_valid[GPIO_CH_TOTAL] = {0};  /* 0 until the first sample has been taken */
static uint8_t rx_static_streak[GPIO_CH_TOTAL] = {0};  /* Consecutive samples with no P transition */

static uint8_t GpioChannels_IsTxChannel(uint8_t ch)
{
    if ((ch >= GPIO_CH_HOST_TX(0)) && (ch <= GPIO_CH_HOST_TX(GPIO_CH_PER_GROUP - 1)))
        return 1;

    if ((ch >= GPIO_CH_LANE_TX(0)) && (ch <= GPIO_CH_LANE_TX(GPIO_CH_PER_GROUP - 1)))
        return 1;

    return 0;
}

static uint8_t GpioChannels_IsRxChannel(uint8_t ch)
{
    if ((ch >= GPIO_CH_HOST_RX(0)) && (ch <= GPIO_CH_HOST_RX(GPIO_CH_PER_GROUP - 1)))
        return 1;

    if ((ch >= GPIO_CH_LANE_RX(0)) && (ch <= GPIO_CH_LANE_RX(GPIO_CH_PER_GROUP - 1)))
        return 1;

    return 0;
}

/* --------------------------------------------------------------------------
 * GPIO Channel Pin Assignment Table
 *
 * TODO: Replace these placeholder pins with actual hardware GPIO assignments.
 *       Each channel has a P (Positive) and N (Negative) line.
 *       Format: { {Port_P, Pin_P}, {Port_N, Pin_N} }
 * -------------------------------------------------------------------------- */
static const GpioChannel_t gpio_channel_table[GPIO_CH_TOTAL] = {

    /* ===== Host Rx: channels 0-7 (SPI register bits 31:24) ===== */
    /* [0]  Host Rx Ch0 */ { {GPIOA, GPIO_PIN_0},  {GPIOA, GPIO_PIN_1}  },
    /* [1]  Host Rx Ch1 */ { {GPIOA, GPIO_PIN_2},  {GPIOA, GPIO_PIN_3}  },
    /* [2]  Host Rx Ch2 */ { {GPIOA, GPIO_PIN_4},  {GPIOA, GPIO_PIN_5}  },
    /* [3]  Host Rx Ch3 */ { {GPIOA, GPIO_PIN_6},  {GPIOA, GPIO_PIN_7}  },
    /* [4]  Host Rx Ch4 */ { {GPIOA, GPIO_PIN_8},  {GPIOA, GPIO_PIN_9}  },
    /* [5]  Host Rx Ch5 */ { {GPIOA, GPIO_PIN_10}, {GPIOA, GPIO_PIN_11} },
    /* [6]  Host Rx Ch6 */ { {GPIOA, GPIO_PIN_12}, {GPIOA, GPIO_PIN_15} },
    /* [7]  Host Rx Ch7 */ { {GPIOB, GPIO_PIN_0},  {GPIOB, GPIO_PIN_1}  },

    /* ===== Host Tx: channels 0-7 (SPI register bits 23:16) ===== */
    /* [8]  Host Tx Ch0 */ { {GPIOB, GPIO_PIN_2},  {GPIOB, GPIO_PIN_3}  },
    /* [9]  Host Tx Ch1 */ { {GPIOB, GPIO_PIN_4},  {GPIOB, GPIO_PIN_5}  },
    /* [10] Host Tx Ch2 */ { {GPIOB, GPIO_PIN_6},  {GPIOB, GPIO_PIN_7}  },
    /* [11] Host Tx Ch3 */ { {GPIOB, GPIO_PIN_8},  {GPIOB, GPIO_PIN_9}  },
    /* [12] Host Tx Ch4 */ { {GPIOB, GPIO_PIN_10}, {GPIOB, GPIO_PIN_11} },
    /* [13] Host Tx Ch5 */ { {GPIOB, GPIO_PIN_12}, {GPIOB, GPIO_PIN_13} },
    /* [14] Host Tx Ch6 */ { {GPIOB, GPIO_PIN_14}, {GPIOB, GPIO_PIN_15} },
    /* [15] Host Tx Ch7 */ { {GPIOC, GPIO_PIN_0},  {GPIOC, GPIO_PIN_1}  },

    /* ===== Lane Rx: channels 0-7 (SPI register bits 15:8) ===== */
    /* [16] Lane Rx Ch0 */ { {GPIOC, GPIO_PIN_10}, {GPIOC, GPIO_PIN_11} },
    /* [17] Lane Rx Ch1 */ { {GPIOC, GPIO_PIN_4},  {GPIOC, GPIO_PIN_5}  },
    /* [18] Lane Rx Ch2 */ { {GPIOC, GPIO_PIN_6},  {GPIOC, GPIO_PIN_7}  },
    /* [19] Lane Rx Ch3 */ { {GPIOD, GPIO_PIN_4},  {GPIOD, GPIO_PIN_5}  },
    /* [20] Lane Rx Ch4 */ { {GPIOC, GPIO_PIN_2},  {GPIOC, GPIO_PIN_3}  },
    /* [21] Lane Rx Ch5 */ { {GPIOC, GPIO_PIN_12}, {GPIOC, GPIO_PIN_13} },
    /* [22] Lane Rx Ch6 */ { {GPIOD, GPIO_PIN_0},  {GPIOD, GPIO_PIN_1}  },
    /* [23] Lane Rx Ch7 */ { {GPIOD, GPIO_PIN_2},  {GPIOD, GPIO_PIN_3}  },

    /* ===== Lane Tx: channels 0-7 (SPI register bits 7:0) ===== */
    /* [24] Lane Tx Ch0 */ { {GPIOC, GPIO_PIN_8},  {GPIOC, GPIO_PIN_9}  },
    /* [25] Lane Tx Ch1 */ { {GPIOD, GPIO_PIN_6},  {GPIOD, GPIO_PIN_7}  },
    /* [26] Lane Tx Ch2 */ { {GPIOD, GPIO_PIN_10}, {GPIOD, GPIO_PIN_11} },
    /* [27] Lane Tx Ch3 */ { {GPIOD, GPIO_PIN_12}, {GPIOD, GPIO_PIN_13} },
    /* [28] Lane Tx Ch4 */ { {GPIOD, GPIO_PIN_14}, {GPIOD, GPIO_PIN_15} },
    /* [29] Lane Tx Ch5 */ { {GPIOE, GPIO_PIN_0},  {GPIOE, GPIO_PIN_1}  },
    /* [30] Lane Tx Ch6 */ { {GPIOE, GPIO_PIN_2},  {GPIOE, GPIO_PIN_3}  },
    /* [31] Lane Tx Ch7 */ { {GPIOE, GPIO_PIN_4},  {GPIOE, GPIO_PIN_5}  },
};

/* --------------------------------------------------------------------------
 * Public API Implementation
 * -------------------------------------------------------------------------- */

void GpioChannels_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable all GPIO port clocks that may be used */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* Configure all 64 GPIOs as inputs, no pull */
    for (uint8_t ch = 0; ch < GPIO_CH_TOTAL; ch++)
    {
        /* P line */
        GPIO_InitStruct.Pin   = gpio_channel_table[ch].p.pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(gpio_channel_table[ch].p.port, &GPIO_InitStruct);

        /* N line */
        GPIO_InitStruct.Pin   = gpio_channel_table[ch].n.pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(gpio_channel_table[ch].n.port, &GPIO_InitStruct);
    }
}

void GpioChannels_ConfigTxOutputs(void)
{
//    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable all GPIO port clocks that may be used by TX channels. - TBD by ioc file in CubeMX !!! */
//    __HAL_RCC_GPIOA_CLK_ENABLE();
//    __HAL_RCC_GPIOB_CLK_ENABLE();
//    __HAL_RCC_GPIOC_CLK_ENABLE();
//    __HAL_RCC_GPIOD_CLK_ENABLE();
//    __HAL_RCC_GPIOE_CLK_ENABLE();

//    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStruct.Pull  = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (uint8_t i = 0; i < GPIO_CH_PER_GROUP; i++)
    {
        uint8_t host_tx_ch = GPIO_CH_HOST_TX(i);
        uint8_t lane_tx_ch = GPIO_CH_LANE_TX(i);

/*  TBD by ioc file in CubeMX !!!
        GPIO_InitStruct.Pin = gpio_channel_table[host_tx_ch].p.pin;
        HAL_GPIO_Init(gpio_channel_table[host_tx_ch].p.port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = gpio_channel_table[host_tx_ch].n.pin;
        HAL_GPIO_Init(gpio_channel_table[host_tx_ch].n.port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = gpio_channel_table[lane_tx_ch].p.pin;
        HAL_GPIO_Init(gpio_channel_table[lane_tx_ch].p.port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = gpio_channel_table[lane_tx_ch].n.pin;
        HAL_GPIO_Init(gpio_channel_table[lane_tx_ch].n.port, &GPIO_InitStruct); */

        /* Default idle output represents logical 0 on differential pair. */
        GpioChannels_WriteTxBit(host_tx_ch, 0);
        GpioChannels_WriteTxBit(lane_tx_ch, 0);
    }
}

GPIO_PinState GpioChannels_ReadP(uint8_t ch)
{
    if (ch >= GPIO_CH_TOTAL)
        return GPIO_PIN_RESET;

    return HAL_GPIO_ReadPin(gpio_channel_table[ch].p.port,
                            gpio_channel_table[ch].p.pin);
}

GPIO_PinState GpioChannels_ReadN(uint8_t ch)
{
    if (ch >= GPIO_CH_TOTAL)
        return GPIO_PIN_RESET;

    return HAL_GPIO_ReadPin(gpio_channel_table[ch].n.port,
                            gpio_channel_table[ch].n.pin);
}

uint8_t GpioChannels_IsComplementary(uint8_t ch)
{
    if (ch >= GPIO_CH_TOTAL)
        return 0;

    GPIO_PinState p = HAL_GPIO_ReadPin(gpio_channel_table[ch].p.port,
                                       gpio_channel_table[ch].p.pin);
    GPIO_PinState n = HAL_GPIO_ReadPin(gpio_channel_table[ch].n.port,
                                       gpio_channel_table[ch].n.pin);

    /* Complementary means P != N */
    return (p != n) ? 1 : 0;
}

uint32_t GpioChannels_CheckAllFaults(void)
{
    uint32_t faults = 0;

    for (uint8_t ch = 0; ch < GPIO_CH_TOTAL; ch++)
    {
        GPIO_PinState p = HAL_GPIO_ReadPin(gpio_channel_table[ch].p.port,
                                           gpio_channel_table[ch].p.pin);
        GPIO_PinState n = HAL_GPIO_ReadPin(gpio_channel_table[ch].n.port,
                                           gpio_channel_table[ch].n.pin);

        /* RX faults are qualified to reject short transients between P/N updates. */
        if (GpioChannels_IsRxChannel(ch))
        {
            /* --- Activity (toggle) tracking -------------------------------
             * A valid RX pair carries a square wave, so P must keep toggling.
             * Detect whether P changed level since the previous sample. */
            uint8_t p_level = (p != GPIO_PIN_RESET) ? 1U : 0U;
            uint8_t no_activity = 0U;

            if (rx_prev_p_valid[ch] && (p_level == rx_prev_p[ch]))
            {
                if (rx_static_streak[ch] < GPIO_RX_ACTIVITY_TIMEOUT_SAMPLES)
                {
                    rx_static_streak[ch]++;
                }
            }
            else
            {
                rx_static_streak[ch] = 0U;  /* Toggle seen: pair is active */
            }

            rx_prev_p[ch]       = p_level;
            rx_prev_p_valid[ch] = 1U;

            if (rx_static_streak[ch] >= GPIO_RX_ACTIVITY_TIMEOUT_SAMPLES)
            {
                no_activity = 1U;  /* P stuck at a constant level -> faulty */
            }

            /* Fault if P/N are not complementary OR the pair is not toggling. */
            if ((p == n) || no_activity)
            {
                if (rx_fault_streak[ch] < GPIO_RX_FAULT_CONFIRM_SAMPLES)
                {
                    rx_fault_streak[ch]++;
                }

                if (rx_fault_streak[ch] >= GPIO_RX_FAULT_CONFIRM_SAMPLES)
                {
                    rx_fault_active[ch] = 1;
                }
            }
            else if (rx_fault_streak[ch] != 0U)
            {
                rx_fault_streak[ch]--;

                if (rx_fault_streak[ch] == 0U)
                {
                    rx_fault_active[ch] = 0;
                }
            }

            if (rx_fault_active[ch])
            {
                if ((ch >= GPIO_CH_HOST_RX(0)) && (ch <= GPIO_CH_HOST_RX(GPIO_CH_PER_GROUP - 1)))
                {
                    uint8_t group_bit = (uint8_t)(ch - GPIO_CH_HOST_RX(0));
                    faults |= (SPI_REG_CHANNEL_BIT(group_bit) << SPI_REG_HOST_RX_SHIFT);
                }
                else if ((ch >= GPIO_CH_LANE_RX(0)) && (ch <= GPIO_CH_LANE_RX(GPIO_CH_PER_GROUP - 1)))
                {
                    uint8_t group_bit = (uint8_t)(ch - GPIO_CH_LANE_RX(0));
                    faults |= (SPI_REG_CHANNEL_BIT(group_bit) << SPI_REG_LANE_RX_SHIFT);
                }
            }
        }
        /* TX channels are intentionally ignored in RX_RESULT and remain 0. */
    }

    return faults;
}

void GpioChannels_WriteTxBit(uint8_t ch, uint8_t bit)
{
    if (!GpioChannels_IsTxChannel(ch))
        return;

    HAL_GPIO_WritePin(gpio_channel_table[ch].p.port,
                      gpio_channel_table[ch].p.pin,
                      bit ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(gpio_channel_table[ch].n.port,
                      gpio_channel_table[ch].n.pin,
                      bit ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void GpioChannels_WriteTxReset(uint8_t ch)
{
    if (!GpioChannels_IsTxChannel(ch))
        return;

    /* Set both P and N to RESET (logical idle state) */
    HAL_GPIO_WritePin(gpio_channel_table[ch].p.port,
                      gpio_channel_table[ch].p.pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(gpio_channel_table[ch].n.port,
                      gpio_channel_table[ch].n.pin,
                      GPIO_PIN_RESET);
}

const GpioChannel_t* GpioChannels_GetDescriptor(uint8_t ch)
{
    if (ch >= GPIO_CH_TOTAL)
        return NULL;

    return &gpio_channel_table[ch];
}