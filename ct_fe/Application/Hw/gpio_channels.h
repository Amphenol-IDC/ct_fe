/**
 ******************************************************************************
 * @file    gpio_channels.h
 * @brief   GPIO channel definitions for P/N differential pairs
 *
 * Each channel has two physical GPIO lines:
 *   - P (Positive)
 *   - N (Negative)
 *
 * Channel mapping (32 channels total):
 *   Host Rx: channels 0-7   (bits 31:24 in SPI register)
 *   Host Tx: channels 8-15  (bits 23:16 in SPI register)
 *   Lane Rx: channels 16-23 (bits 15:8  in SPI register)
 *   Lane Tx: channels 24-31 (bits 7:0   in SPI register)
 ******************************************************************************
 */

#ifndef APPLICATION_HW_GPIO_CHANNELS_H_
#define APPLICATION_HW_GPIO_CHANNELS_H_

#include "main.h"

/* --------------------------------------------------------------------------
 * Channel Group Indices
 * -------------------------------------------------------------------------- */
#define GPIO_CH_GROUP_HOST_RX       0
#define GPIO_CH_GROUP_HOST_TX       1
#define GPIO_CH_GROUP_LANE_RX       2
#define GPIO_CH_GROUP_LANE_TX       3
#define GPIO_CH_NUM_GROUPS          4

#define GPIO_CH_PER_GROUP           8
#define GPIO_CH_TOTAL               (GPIO_CH_NUM_GROUPS * GPIO_CH_PER_GROUP)  /* 32 */
#define GPIO_CH_LINES_PER_CH        2   /* P and N */
#define GPIO_CH_TOTAL_GPIOS         (GPIO_CH_TOTAL * GPIO_CH_LINES_PER_CH)   /* 64 */

/* --------------------------------------------------------------------------
 * Channel index helpers
 * -------------------------------------------------------------------------- */
#define GPIO_CH_HOST_RX(n)          ((GPIO_CH_GROUP_HOST_RX * GPIO_CH_PER_GROUP) + (n))  /* 0..7 */
#define GPIO_CH_HOST_TX(n)          ((GPIO_CH_GROUP_HOST_TX * GPIO_CH_PER_GROUP) + (n))  /* 8..15 */
#define GPIO_CH_LANE_RX(n)          ((GPIO_CH_GROUP_LANE_RX * GPIO_CH_PER_GROUP) + (n))  /* 16..23 */
#define GPIO_CH_LANE_TX(n)          ((GPIO_CH_GROUP_LANE_TX * GPIO_CH_PER_GROUP) + (n))  /* 24..31 */

/* --------------------------------------------------------------------------
 * P/N line index within a channel
 * -------------------------------------------------------------------------- */
#define GPIO_LINE_P                 0   /* Positive */
#define GPIO_LINE_N                 1   /* Negative */

/* --------------------------------------------------------------------------
 * GPIO Pin descriptor
 * -------------------------------------------------------------------------- */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} GpioPin_t;

/* --------------------------------------------------------------------------
 * Channel descriptor (P and N pair)
 * -------------------------------------------------------------------------- */
typedef struct {
    GpioPin_t p;    /* Positive line */
    GpioPin_t n;    /* Negative line */
} GpioChannel_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialize all 64 channel GPIOs as inputs
 */
void GpioChannels_Init(void);

/**
 * @brief  Configure TX channels (Host TX + Lane TX) as GPIO outputs
 * @note   RX channels are not modified by this API.
 */
void GpioChannels_ConfigTxOutputs(void);

/**
 * @brief  Read P line state of a channel
 * @param  ch: channel index (0..31)
 * @retval GPIO_PinState (GPIO_PIN_SET or GPIO_PIN_RESET)
 */
GPIO_PinState GpioChannels_ReadP(uint8_t ch);

/**
 * @brief  Read N line state of a channel
 * @param  ch: channel index (0..31)
 * @retval GPIO_PinState (GPIO_PIN_SET or GPIO_PIN_RESET)
 */
GPIO_PinState GpioChannels_ReadN(uint8_t ch);

/**
 * @brief  Check if a channel's P and N lines are complementary
 * @param  ch: channel index (0..31)
 * @retval 1 = complementary (OK), 0 = fault (P == N)
 */
uint8_t GpioChannels_IsComplementary(uint8_t ch);

/**
 * @brief  Check all 32 channels for complementary state
 * @retval 32-bit bitmask: bit=1 means channel has a fault (P == N)
 */
uint32_t GpioChannels_CheckAllFaults(void);

/**
 * @brief  Drive one TX differential channel with a logical bit
 * @param  ch: TX channel index (Host TX: 8..15, Lane TX: 24..31)
 * @param  bit: 1 -> P=1,N=0 ; 0 -> P=0,N=1
 */
void GpioChannels_WriteTxBit(uint8_t ch, uint8_t bit);

/**
 * @brief  Set one TX differential channel to idle (both P=0, N=0)
 * @param  ch: TX channel index (Host TX: 8..15, Lane TX: 24..31)
 */
void GpioChannels_WriteTxReset(uint8_t ch);

/**
 * @brief  Get channel descriptor (for debug)
 * @param  ch: channel index (0..31)
 * @retval Pointer to GpioChannel_t
 */
const GpioChannel_t* GpioChannels_GetDescriptor(uint8_t ch);

#endif /* APPLICATION_HW_GPIO_CHANNELS_H_ */