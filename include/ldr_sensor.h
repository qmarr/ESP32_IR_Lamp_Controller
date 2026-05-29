#ifndef LDR_H
#define LDR_H

#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include <stdbool.h>

#define LED_GPIO        GPIO_NUM_36
#define LDR_ADC_UNIT    ADC_UNIT_1
#define LDR_ADC_CH      ADC_CHANNEL_0
#define LDR_ADC_ATTEN   ADC_ATTEN_DB_12

#define LDR_DARK_THRESHOLD   1100
#define LDR_LIGHT_THRESHOLD  1700

#define SMA_SIZE 10

void ldr_init(void);

int adc_read_raw(void);

int adc_read_avg(int raw);

bool room_is_dark(int avg);

#endif // LDR_H
