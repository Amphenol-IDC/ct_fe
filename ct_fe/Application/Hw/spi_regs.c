/**
 ******************************************************************************
 * @file    spi_regs.c
 * @brief   SPI Slave Register Map implementation
 *
 * Protocol:
 *   Master always transfers one fixed 5-byte frame while NSS is low:
 *     Byte 0: command byte [7]=R/W (1=Read, 0=Write), [6:0]=Address
 *     Byte 1-4: write data from master, or dummy bytes for reads
 *
 *   The slave pre-arms the whole 5-byte frame before clocks arrive. This avoids
 *   SPI underrun caused by re-arming one byte at a time in the ISR.
 *
 *   Read response is pipelined: a read command prepares MISO data for the next
 *   5-byte frame. The master should send a second dummy/NOP frame to collect it.
 ******************************************************************************
 */

#include "spi_regs.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/* External SPI handle from main.c */
extern SPI_HandleTypeDef hspi4;

#define SPI_FRAME_SIZE          5U
#define SPI_FRAME_CMD_INDEX     0U
#define SPI_FRAME_DATA_INDEX    1U

/* --------------------------------------------------------------------------
 * Private variables
 * -------------------------------------------------------------------------- */
static SpiRegs_t spi_regs;

/* Fixed full-duplex frame buffers. Keep them static for HAL IT lifetime. */
static uint8_t spi_rx_frame[SPI_FRAME_SIZE];
static uint8_t spi_tx_frame[SPI_FRAME_SIZE];
static volatile uint32_t spi_error_count;
static volatile uint32_t spi_rearm_error_count;
static volatile uint32_t spi_last_error;

/* --------------------------------------------------------------------------
 * Private function prototypes
 * -------------------------------------------------------------------------- */
static void SpiRegs_StartListening(void);
static void SpiRegs_HandleFrame(void);
static void SpiRegs_PrepareReadData(void);
static void SpiRegs_ProcessWriteData(void);
static uint8_t SpiRegs_GetRegSize(uint8_t addr);
static void SpiRegs_QueueReadResponse(void);

/* --------------------------------------------------------------------------
 * Public API Implementation
 * -------------------------------------------------------------------------- */

void SpiRegs_Init(void)
{
    memset(&spi_regs, 0, sizeof(spi_regs));
    memset(spi_rx_frame, 0, sizeof(spi_rx_frame));
    memset(spi_tx_frame, 0, sizeof(spi_tx_frame));

    /* Initialize read-only registers */
    spi_regs.device_type = DEVICE_TYPE_VALUE;
    spi_regs.device_rev  = DEVICE_REV_VALUE;
    spi_regs.device_id   = DEVICE_ID_VALUE;

    /* Start in idle state */
    spi_regs.state = SPI_SLAVE_STATE_IDLE;
    spi_regs.byte_index = 0;

    /* Begin listening for a full fixed-size frame */
    SpiRegs_StartListening();
}

void SpiRegs_SetRxResult(uint32_t value)
{
    spi_regs.rx_result = value;
}

uint8_t SpiRegs_GetCtrl(uint32_t *value)
{
    if (spi_regs.ctrl_written)
    {
        *value = spi_regs.ctrl;
        spi_regs.ctrl_written = 0;
        return 1;
    }
    return 0;
}

uint8_t SpiRegs_GetStop(uint32_t *value)
{
    if (spi_regs.stop_written)
    {
        *value = spi_regs.stop;
        spi_regs.stop_written = 0;
        return 1;
    }
    return 0;
}

const SpiRegs_t* SpiRegs_GetContext(void)
{
    return &spi_regs;
}

uint32_t SpiRegs_GetRearmErrorCount(void) 
{ 
    return spi_rearm_error_count; 
}

/* --------------------------------------------------------------------------
 * Private functions
 * -------------------------------------------------------------------------- */

static void SpiRegs_StartListening(void)
{
    HAL_StatusTypeDef status;

    memset(spi_rx_frame, 0, sizeof(spi_rx_frame));

    status = HAL_SPI_TransmitReceive_IT(&hspi4, spi_tx_frame, spi_rx_frame, SPI_FRAME_SIZE);
    if (status != HAL_OK)
    {
        spi_rearm_error_count++;
    }
}

static uint8_t SpiRegs_GetRegSize(uint8_t addr)
{
    switch (addr)
    {
        case SPI_REG_RX_RESULT:
        case SPI_REG_CTRL:
        case SPI_REG_STOP:
            return 4;  /* 32-bit */

        case SPI_REG_DEVICE_TYPE:
        case SPI_REG_DEVICE_REV:
        case SPI_REG_DEVICE_ID_HI:
            return 1;  /* 8-bit */

        case SPI_REG_DEVICE_ID_LO:
            return 2;  /* 16-bit (spans 0x7E-0x7F) */

        default:
            return 0;  /* Unknown register */
    }
}

static void SpiRegs_HandleFrame(void)
{
    uint8_t reg_size;

    spi_regs.cmd_byte = spi_rx_frame[SPI_FRAME_CMD_INDEX];
    spi_regs.is_read  = (spi_regs.cmd_byte & SPI_CMD_RW_BIT) ? 1 : 0;
    spi_regs.reg_addr = spi_regs.cmd_byte & SPI_CMD_ADDR_MASK;
    spi_regs.byte_index = 0;
    spi_regs.state = SPI_SLAVE_STATE_DATA;

    reg_size = SpiRegs_GetRegSize(spi_regs.reg_addr);

    if (spi_regs.is_read)
    {
        SpiRegs_PrepareReadData();
        SpiRegs_QueueReadResponse();
    }
    else if (reg_size > 0U)
    {
        memset(spi_regs.rx_buf, 0, sizeof(spi_regs.rx_buf));
        memcpy(spi_regs.rx_buf, &spi_rx_frame[SPI_FRAME_DATA_INDEX], reg_size);
        SpiRegs_ProcessWriteData();

        /* Writes do not return data. Clear queued MISO payload. */
        memset(spi_tx_frame, 0, sizeof(spi_tx_frame));
    }
    else
    {
        memset(spi_tx_frame, 0, sizeof(spi_tx_frame));
    }

    spi_regs.state = SPI_SLAVE_STATE_IDLE;
}

/* Start copies to byte 0 - So on MISO data is starting at byte 0 */
static void SpiRegs_QueueReadResponse(void)
{
    uint8_t reg_size = SpiRegs_GetRegSize(spi_regs.reg_addr);

    memset(spi_tx_frame, 0, sizeof(spi_tx_frame));

    if (reg_size > 0U)
    {
        memcpy(spi_tx_frame, spi_regs.tx_buf, reg_size);
    }
}

static void SpiRegs_PrepareReadData(void)
{
    uint32_t val32;
    uint16_t val16;

    memset(spi_regs.tx_buf, 0, sizeof(spi_regs.tx_buf));

    switch (spi_regs.reg_addr)
    {
        case SPI_REG_RX_RESULT:
            /* Read and Clear */
            val32 = spi_regs.rx_result;
            spi_regs.rx_result = 0;  /* Clear on read */
            /* Big-endian: MSB first */
            spi_regs.tx_buf[0] = (uint8_t)(val32 >> 24);
            spi_regs.tx_buf[1] = (uint8_t)(val32 >> 16);
            spi_regs.tx_buf[2] = (uint8_t)(val32 >> 8);
            spi_regs.tx_buf[3] = (uint8_t)(val32);
            break;

        case SPI_REG_DEVICE_TYPE:
            spi_regs.tx_buf[0] = spi_regs.device_type;
            break;

        case SPI_REG_DEVICE_REV:
            spi_regs.tx_buf[0] = spi_regs.device_rev;
            break;

        case SPI_REG_DEVICE_ID_LO:
            val16 = spi_regs.device_id;
            /* Big-endian: MSB first */
            spi_regs.tx_buf[0] = (uint8_t)(val16 >> 8);
            spi_regs.tx_buf[1] = (uint8_t)(val16);
            break;

        case SPI_REG_DEVICE_ID_HI:
            spi_regs.tx_buf[0] = (uint8_t)(spi_regs.device_id >> 8);
            break;

        /* CTRL and STOP are Write-Only, return 0 on read */
        case SPI_REG_CTRL:
        case SPI_REG_STOP:
        default:
            break;
    }
}

static void SpiRegs_ProcessWriteData(void)
{
    uint32_t val32;

    /* Reconstruct 32-bit value from received bytes (big-endian) */
    val32 = ((uint32_t)spi_regs.rx_buf[0] << 24) |
            ((uint32_t)spi_regs.rx_buf[1] << 16) |
            ((uint32_t)spi_regs.rx_buf[2] << 8)  |
            ((uint32_t)spi_regs.rx_buf[3]);

    switch (spi_regs.reg_addr)
    {
        case SPI_REG_CTRL:
            spi_regs.ctrl = val32;
            spi_regs.ctrl_written = 1;
            spi_regs.ctrl_write_seq++;
            break;

        case SPI_REG_STOP:
            spi_regs.stop = val32;
            spi_regs.stop_written = 1;
            spi_regs.stop_write_seq++;
            break;

        /* RX_RESULT is Read-Only (RC), ignore writes */
        /* DEVICE_TYPE/REV/ID are Read-Only, ignore writes */
        default:
            break;
    }
}

/* --------------------------------------------------------------------------
 * HAL SPI Callbacks
 * -------------------------------------------------------------------------- */

/**
 * @brief  SPI TransmitReceive complete callback (one 5-byte frame transferred)
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI4)
        return;

    SpiRegs_HandleFrame();
    SpiRegs_StartListening();
}

/**
 * @brief  SPI Error callback - reset state machine
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI4)
        return;

    spi_error_count++;
    spi_last_error = HAL_SPI_GetError(hspi);

    /* Reset state and restart listening. Avoid printf in ISR context. */
    spi_regs.state = SPI_SLAVE_STATE_IDLE;
    spi_regs.byte_index = 0;
    memset(spi_tx_frame, 0, sizeof(spi_tx_frame));
    SpiRegs_StartListening();
}
