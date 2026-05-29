#include "ldr_sensor.h"

static adc_oneshot_unit_handle_t ldr_handle = NULL;
// ldr sma
static int sma_buffer[SMA_SIZE] = {0};
static int sma_index = 0;
static int sma_sum = 0;
static bool sma_filled = false;

void ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_conf = {
        .unit_id = LDR_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_conf, &ldr_handle));

    adc_oneshot_chan_cfg_t channel_conf = {
        .atten = LDR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(ldr_handle, LDR_ADC_CH, &channel_conf));
}

int adc_read_raw(void)
{
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(ldr_handle, LDR_ADC_CH, &raw));
    return raw;
}

int adc_read_avg(int raw)
{
    sma_sum -= sma_buffer[sma_index];
    sma_buffer[sma_index] = raw;

    sma_sum += raw;

    sma_index++;

    if (sma_index >= SMA_SIZE)
    {
        sma_index = 0;
        sma_filled = true;
    }

    int divisor = sma_filled ? SMA_SIZE : sma_index;
    if (divisor == 0)
    {
        divisor = 1;
    }

    return sma_sum / divisor;
}

bool room_is_dark(int avg)
{
    static bool dark = false;

    if (!dark && avg < LDR_DARK_THRESHOLD)
    {
        dark = true;
    }
    else if (dark && avg > LDR_LIGHT_THRESHOLD)
    {
        dark = false;
    }
    return dark;
}
