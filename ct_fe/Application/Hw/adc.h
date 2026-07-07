
#pragma once


/**
 * @brief  Read host board type via ADC1_INP2 resistor-ladder.
 * @retval Board type ID (0–15), 0 if ADC code is out of range.
 */
uint8_t gpio_get_host_board_type(void);

/**
 * @brief  Read host board revision via ADC1_INP6 resistor-ladder.
 * @retval Board revision ID (0–15), 0 if ADC code is out of range.
 */
uint8_t gpio_get_host_board_rev(void);
