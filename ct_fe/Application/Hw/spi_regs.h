/**
 ******************************************************************************
 * @file    spi_regs.h
 * @brief   SPI Slave Register Map definitions
 ******************************************************************************
 */

#pragma once

#include <stdint.h>

/* --------------------------------------------------------------------------
 * SPI Register Addresses
 * -------------------------------------------------------------------------- */
#define SPI_REG_RX_RESULT       0x00    /* 32-bit, Read & Clear */
#define SPI_REG_CTRL            0x20    /* 32-bit, Write Only - Trigger lanes */
#define SPI_REG_STOP            0x21    /* 32-bit, Write Only - Stop lanes */
#define SPI_REG_DEVICE_TYPE     0x7C    /* 8-bit,  Read Only */
#define SPI_REG_DEVICE_REV      0x7D    /* 8-bit,  Read Only */
#define SPI_REG_DEVICE_ID_LO    0x7E    /* 8-bit,  Read Only (low byte) */
#define SPI_REG_DEVICE_ID_HI    0x7F    /* 8-bit,  Read Only (high byte) */

/* --------------------------------------------------------------------------
 * Device Identity Constants
 * -------------------------------------------------------------------------- */
#define DEVICE_TYPE_VALUE       0xA5    /* TBD: set your device type */
#define DEVICE_REV_VALUE        0x01    /* Revision 1 */
#define DEVICE_ID_VALUE         0x1234  /* TBD: set your device ID */

/* --------------------------------------------------------------------------
 * SPI Protocol definitions
 * Byte 0 from master: [7]=R/W (0=Write, 1=Read), [6:0]=Address
 * -------------------------------------------------------------------------- */
#define SPI_CMD_RW_BIT          0x80
#define SPI_CMD_ADDR_MASK       0x7F

/* --------------------------------------------------------------------------
 * 32-bit Register Bit Allocation (for RX_RESULT, CTRL, STOP)
 *
 * Bits [31:24] = Host Rx  (channels 7..0)
 * Bits [23:16] = Host Tx  (channels 7..0)
 * Bits [15:8]  = Lane Rx  (channels 7..0)
 * Bits [7:0]   = Lane Tx  (channels 7..0)
 * -------------------------------------------------------------------------- */

/* Host Rx group - bits 31:24 */
#define SPI_REG_HOST_RX_SHIFT   24
#define SPI_REG_HOST_RX_MASK    (0xFFUL << SPI_REG_HOST_RX_SHIFT)

/* Host Tx group - bits 23:16 */
#define SPI_REG_HOST_TX_SHIFT   16
#define SPI_REG_HOST_TX_MASK    (0xFFUL << SPI_REG_HOST_TX_SHIFT)

/* Lane Rx group - bits 15:8 */
#define SPI_REG_LANE_RX_SHIFT   8
#define SPI_REG_LANE_RX_MASK    (0xFFUL << SPI_REG_LANE_RX_SHIFT)

/* Lane Tx group - bits 7:0 */
#define SPI_REG_LANE_TX_SHIFT   0
#define SPI_REG_LANE_TX_MASK    (0xFFUL << SPI_REG_LANE_TX_SHIFT)

/* Channel bit within a group (0..7) */
#define SPI_REG_CHANNEL_BIT(ch) (1UL << (ch))

/* --------------------------------------------------------------------------
 * SPI Slave State Machine
 * -------------------------------------------------------------------------- */
typedef enum {
    SPI_SLAVE_STATE_IDLE = 0,   /* Waiting for command byte */
    SPI_SLAVE_STATE_DATA,       /* Transferring data bytes */
} SpiSlaveState_t;

/* --------------------------------------------------------------------------
 * SPI Slave Register Context
 * -------------------------------------------------------------------------- */
typedef struct {
    /* Registers */
    volatile uint32_t rx_result;    /* 0x00: Read & Clear */
    volatile uint32_t ctrl;         /* 0x20: Write Only - last written value */
    volatile uint32_t stop;         /* 0x21: Write Only - last written value */
    uint8_t  device_type;           /* 0x7C: Read Only */
    uint8_t  device_rev;            /* 0x7D: Read Only */
    uint16_t device_id;             /* 0x7E-0x7F: Read Only */

    /* State machine */
    SpiSlaveState_t state;
    uint8_t  cmd_byte;              /* Received command byte */
    uint8_t  is_read;               /* 1 = read transaction, 0 = write */
    uint8_t  reg_addr;              /* Target register address */
    uint8_t  byte_index;            /* Current byte index in multi-byte transfer */

    /* Buffers */
    uint8_t  tx_buf[5];             /* Max 1 cmd + 4 data bytes out */
    uint8_t  rx_buf[5];             /* Max 1 cmd + 4 data bytes in */

    /* Flags for application */
    volatile uint8_t ctrl_written;  /* Set when CTRL register is written */
    volatile uint8_t stop_written;  /* Set when STOP register is written */

    /* Monotonic write-event counters, for logging OUTSIDE the RT thread.
     * A logger compares these against its own snapshot; no printf is done
     * in the ISR or in the 1 ms polling thread. */
    volatile uint32_t ctrl_write_seq;
    volatile uint32_t stop_write_seq;
} SpiRegs_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialize SPI slave register map and start listening
 */
void SpiRegs_Init(void);

/**
 * @brief  Set the RX_RESULT register value (called by application)
 * @param  value: 32-bit result per lane
 */
void SpiRegs_SetRxResult(uint32_t value);

/**
 * @brief  Get last written CTRL register value and clear flag
 * @param  value: pointer to store the value
 * @retval 1 if new value was written since last call, 0 otherwise
 */
uint8_t SpiRegs_GetCtrl(uint32_t *value);

/**
 * @brief  Get last written STOP register value and clear flag
 * @param  value: pointer to store the value
 * @retval 1 if new value was written since last call, 0 otherwise
 */
uint8_t SpiRegs_GetStop(uint32_t *value);

/**
 * @brief  Get the register context (for debug/status)
 * @retval Pointer to SpiRegs_t
 */
const SpiRegs_t* SpiRegs_GetContext(void);

/**
 * @brief  Get SPI re-arm error counter for debug
 * @retval Number of failed HAL_SPI_TransmitReceive_IT re-arm attempts
 */
uint32_t SpiRegs_GetRearmErrorCount(void);

/**
 * @brief  Resynchronize the SPI slave on an NSS rising edge (end of frame).
 *
 * Call this from an EXTI interrupt configured on the NSS pin (rising edge).
 * It aborts and re-arms the slave so stale FIFO/counter state can never carry
 * across frames. This is the robust fix for the overnight desync HardFault.
 */
void SpiRegs_OnNssRising(void);

