
#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"


extern ADC_HandleTypeDef hadc1;

#define ADC_TIMEOUT_MS       100U

/* ---------------------------------------------------------------------------
 * Helper: read a single ADC1 channel (reconfigures rank 1 each time).
 * Returns raw 12-bit value, or 0 on error.
 * ------------------------------------------------------------------------ */
static uint32_t adc1_read_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel      = channel;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return 0;
    }

    uint32_t raw = 0;
    if (HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) == HAL_OK) {
        raw = HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);
    return raw;
}

/* ---------------------------------------------------------------------------
 * ADC-code-to-ID lookup table (resistor-ladder decoding).
 * Each entry: { low_threshold, upper_threshold } → ID = index.
 * ------------------------------------------------------------------------ */
typedef struct {
    uint16_t low;
    uint16_t high;
} AdcIdRange_t;

static const AdcIdRange_t adc_id_table[] =
{
    {    0,   25 },  /* ID 0  */
    {   51,  100 },  /* ID 1  */
    {  150,  182 },  /* ID 2  */
    {  215,  252 },  /* ID 3  */
    {  289,  344 },  /* ID 4  */
    {  399,  465 },  /* ID 5  */
    {  528,  595 },  /* ID 6  */
    {  660,  742 },  /* ID 7  */
    {  825,  907 },  /* ID 8  */
    {  990, 1056 },  /* ID 9  */
    { 1122, 1186 },  /* ID 10 */
    { 1251, 1306 },  /* ID 11 */
    { 1361, 1398 },  /* ID 12 */
    { 1435, 1467 },  /* ID 13 */
    { 1500, 1575 },  /* ID 14 */
    { 1650, 2047 },  /* ID 15 */
};

#define ADC_ID_TABLE_SIZE  (sizeof(adc_id_table) / sizeof(adc_id_table[0]))

static uint8_t adc_code_to_id(uint32_t adc_code)
{
    for (uint8_t i = 0; i < ADC_ID_TABLE_SIZE; i++) {
        if (adc_code >= adc_id_table[i].low && adc_code <= adc_id_table[i].high) {
            return i;
        }
    }
    return 0;  /* default when out of all ranges */
}

/* ---------------------------------------------------------------------------
 * Public API: host board type (ADC1_INP2) and host board revision (ADC1_INP6).
 * ------------------------------------------------------------------------ */
uint8_t gpio_get_host_board_type(void)
{
    uint32_t raw = adc1_read_channel(ADC_CHANNEL_2);
    return adc_code_to_id(raw);
}

uint8_t gpio_get_host_board_rev(void)
{
    uint32_t raw = adc1_read_channel(ADC_CHANNEL_6);
    return adc_code_to_id(raw);
}
