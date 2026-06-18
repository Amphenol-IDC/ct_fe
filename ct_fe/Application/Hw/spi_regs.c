/**
 ******************************************************************************
 * @file    spi_regs.c
 * @brief   SPI Slave Register Map implementation
 *
 * Protocol:
 *   Master sends 1 command byte: [7]=R/W (1=Read, 0=Write), [6:0]=Address
 *   Then data bytes follow (MSB first for multi-byte registers).
 *
 *   For Read:  Slave outputs register data on MISO during subsequent clocks
 *   For Write: Master sends data bytes on MOSI during subsequent clocks
 *
 *   NSS going high (deselected) ends the transaction.
 ******************************************************************************
 */

#include "spi_regs.h"
#include "main.h"
#include <string.h>

/* External SPI handle from main.c */
extern SPI_HandleTypeDef hspi4;

/* --------------------------------------------------------------------------
 * Private variables
 * -------------------------------------------------------------------------- */
static SpiRegs_t spi_regs;

/* DMA/IT buffers - first byte is command, next 4 are data */
static uint8_t spi_rx_byte;
static uint8_t spi_tx_byte;

/* --------------------------------------------------------------------------
 * Private function prototypes
 * -------------------------------------------------------------------------- */
static void SpiRegs_StartListening(void);
static void SpiRegs_HandleCommand(uint8_t cmd);
static void SpiRegs_PrepareReadData(void);
static void SpiRegs_ProcessWriteData(void);
static uint8_t SpiRegs_GetRegSize(uint8_t addr);

/* --------------------------------------------------------------------------
 * Public API Implementation
 * -------------------------------------------------------------------------- */

void SpiRegs_Init(void)
{
    memset(&spi_regs, 0, sizeof(spi_regs));

    /* Initialize read-only registers */
    spi_regs.device_type = DEVICE_TYPE_VALUE;
    spi_regs.device_rev  = DEVICE_REV_VALUE;
    spi_regs.device_id   = DEVICE_ID_VALUE;

    /* Start in idle state */
    spi_regs.state = SPI_SLAVE_STATE_IDLE;
    spi_regs.byte_index = 0;

    /* Begin listening for first command byte */
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

/* --------------------------------------------------------------------------
 * Private functions
 * -------------------------------------------------------------------------- */

static void SpiRegs_StartListening(void)
{
    /* Prepare dummy TX byte (0x00) and listen for 1 byte from master */
    spi_tx_byte = 0x00;
    HAL_SPI_TransmitReceive_IT(&hspi4, &spi_tx_byte, &spi_rx_byte, 1);
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
            return 1;  /* 8-bit */

        case SPI_REG_DEVICE_ID_LO:
            return 2;  /* 16-bit (spans 0x7E-0x7F) */

        default:
            return 0;  /* Unknown register */
    }
}

static void SpiRegs_HandleCommand(uint8_t cmd)
{
    spi_regs.cmd_byte = cmd;
    spi_regs.is_read  = (cmd & SPI_CMD_RW_BIT) ? 1 : 0;
    spi_regs.reg_addr = cmd & SPI_CMD_ADDR_MASK;
    spi_regs.byte_index = 0;
    spi_regs.state = SPI_SLAVE_STATE_DATA;

    if (spi_regs.is_read)
    {
        /* Prepare data to send back to master */
        SpiRegs_PrepareReadData();
    }

    /* Continue transaction - listen for next byte (or send data) */
    if (spi_regs.is_read)
    {
        /* Send first data byte, receive dummy from master */
        uint8_t reg_size = SpiRegs_GetRegSize(spi_regs.reg_addr);
        if (reg_size > 0)
        {
            spi_tx_byte = spi_regs.tx_buf[0];
            spi_regs.byte_index = 1;
        }
        else
        {
            spi_tx_byte = 0x00;  /* Unknown register - send zeros */
        }
        HAL_SPI_TransmitReceive_IT(&hspi4, &spi_tx_byte, &spi_rx_byte, 1);
    }
    else
    {
        /* Write transaction - receive data from master */
        spi_tx_byte = 0x00;
        HAL_SPI_TransmitReceive_IT(&hspi4, &spi_tx_byte, &spi_rx_byte, 1);
    }
}

static void SpiRegs_PrepareReadData(void)
{
    uint32_t val32;
    uint16_t val16;

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
            memset(spi_regs.tx_buf, 0, sizeof(spi_regs.tx_buf));
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
            break;

        case SPI_REG_STOP:
            spi_regs.stop = val32;
            spi_regs.stop_written = 1;
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
 * @brief  SPI TransmitReceive complete callback (1 byte transferred)
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI4)
        return;

    switch (spi_regs.state)
    {
        case SPI_SLAVE_STATE_IDLE:
            /* Received command byte */
            SpiRegs_HandleCommand(spi_rx_byte);
            break;

        case SPI_SLAVE_STATE_DATA:
            if (spi_regs.is_read)
            {
                /* Read transaction: send next data byte */
                uint8_t reg_size = SpiRegs_GetRegSize(spi_regs.reg_addr);
                if (spi_regs.byte_index < reg_size)
                {
                    spi_tx_byte = spi_regs.tx_buf[spi_regs.byte_index];
                    spi_regs.byte_index++;
                    HAL_SPI_TransmitReceive_IT(&hspi4, &spi_tx_byte, &spi_rx_byte, 1);
                }
                else
                {
                    /* All bytes sent - go back to idle, wait for new command */
                    spi_regs.state = SPI_SLAVE_STATE_IDLE;
                    SpiRegs_StartListening();
                }
            }
            else
            {
                /* Write transaction: store received data byte */
                uint8_t reg_size = SpiRegs_GetRegSize(spi_regs.reg_addr);
                if (spi_regs.byte_index < reg_size)
                {
                    spi_regs.rx_buf[spi_regs.byte_index] = spi_rx_byte;
                    spi_regs.byte_index++;

                    if (spi_regs.byte_index >= reg_size)
                    {
                        /* All bytes received - process the write */
                        SpiRegs_ProcessWriteData();
                        spi_regs.state = SPI_SLAVE_STATE_IDLE;
                        SpiRegs_StartListening();
                    }
                    else
                    {
                        /* More bytes expected */
                        spi_tx_byte = 0x00;
                        HAL_SPI_TransmitReceive_IT(&hspi4, &spi_tx_byte, &spi_rx_byte, 1);
                    }
                }
                else
                {
                    /* Overflow - reset to idle */
                    spi_regs.state = SPI_SLAVE_STATE_IDLE;
                    SpiRegs_StartListening();
                }
            }
            break;

        default:
            spi_regs.state = SPI_SLAVE_STATE_IDLE;
            SpiRegs_StartListening();
            break;
    }
}

/**
 * @brief  SPI Error callback - reset state machine
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI4)
        return;

    /* Reset state and restart listening */
    spi_regs.state = SPI_SLAVE_STATE_IDLE;
    spi_regs.byte_index = 0;
    SpiRegs_StartListening();
}
