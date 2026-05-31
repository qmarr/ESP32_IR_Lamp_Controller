#ifndef SLEEP_H
#define SLEEP_H

#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG_SLEEP "SLEEP"

void enter_light_sleep(gpio_num_t encoder_gpio);

#endif /* SLEEP_H */
