#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "ir_command_write.h"
#include "enums.h"

#define TAG "IR_SNIFFER"

#define RMT_RX_GPIO GPIO_NUM_15
#define IR_LED GPIO_NUM_16
#define BTN_GPIO_ENC GPIO_NUM_4

#define DEBOUNCE_US 50000 // 50ms
#define BTN_LONG_PRESS_MS 2000
#define RMT_RESOLUTION_HZ 1000000 // 1us per tick

#define MEM_BLOCK_SYMBOLS 64
#define QUEUE_SIZE 4

static QueueHandle_t ir_queue;

// callback з ISR контексту
static bool ir_rx_done_callback(rmt_channel_handle_t channel,
                                const rmt_rx_done_event_data_t *edata,
                                void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;

    QueueHandle_t queue = (QueueHandle_t)user_data;
    xQueueSendFromISR(queue, edata, &high_task_wakeup);

    return high_task_wakeup == pdTRUE;
}

rmt_channel_handle_t rx_channel = NULL;
rmt_channel_handle_t tx_channel = NULL;
rmt_encoder_handle_t copy_encoder = NULL;
rmt_symbol_word_t raw_symbols[MEM_BLOCK_SYMBOLS];
rmt_transmit_config_t tx_trans_config = {
    .loop_count = 0, // 0 = one loop
};
rmt_receive_config_t receive_config = {
    .signal_range_min_ns = 1000,
    .signal_range_max_ns = 10000000,
};

void rmt_init()
{
    // 1. Конфіг RX каналу
    rmt_rx_channel_config_t rx_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = MEM_BLOCK_SYMBOLS,
        .gpio_num = RMT_RX_GPIO,
    };

    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_config, &rx_channel));

    // 2. Callback
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = ir_rx_done_callback,
    };

    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, ir_queue));

    // 3. Enable channel
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    // 4. Буфер для прийому

    ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols,
                                sizeof(raw_symbols), &receive_config));
    //

    // 5. Tx channel config
    rmt_tx_channel_config_t tx_conf = {
        .gpio_num = IR_LED,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .mem_block_symbols = MEM_BLOCK_SYMBOLS,
        .resolution_hz = 1 * 1000 * 1000,
        .trans_queue_depth = QUEUE_SIZE,
        .flags.invert_out = false, // do not invert output signal
        .flags.with_dma = false,   // do not need DMA backend
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_conf, &tx_channel));

    // 6. carrier config
    rmt_carrier_config_t tx_carrier_cg = {
        .duty_cycle = 0.33,                 // duty cycle 33%
        .frequency_hz = 36000,              // 36 KHz
        .flags.polarity_active_low = false, // carrier should be modulated to high level

    };
    // modulate carrier to TX channel
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &tx_carrier_cg));

    // copy encoder

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &copy_encoder));

    // transmit

    // 7. Enable tx channel
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
}

void encoder_init()
{
    ESP_LOGI(TAG, "Encoder button init");
    gpio_config_t button_gpio_config = {
        .pin_bit_mask = (1ULL << BTN_GPIO_ENC),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&button_gpio_config));
}
void app_main(void)
{
    ESP_LOGI(TAG, "Starting IR sniffer...");

    ir_queue = xQueueCreate(QUEUE_SIZE, sizeof(rmt_rx_done_event_data_t));

    rmt_init();
    encoder_init();

    // variables
    ir_symbol_t ir_commands[CMD_COUNT][IR_LENGTH];
    int command_lengths[CMD_COUNT] = {0};
    rmt_symbol_word_t tx_symbols[IR_LENGTH];
    int learning_index = 0;

    // btn variables
    int last_btn_level = 1;
    int stable_btn_level = 1;
    int64_t last_btn_change_us = 0;

    int64_t btn_press_start_us = 0;
    bool long_press_handled = false;

    // sequence
    IR_COMMANDS learning_order[] = {
        CMD_TURN_RED,
        CMD_TURN_BLUE,
        CMD_TURN_GREEN,
        CMD_TURN_WHITE,
        CMD_TURN_NORTHERN_LIGHTS,
        CMD_TURN_STARS,
        CMD_TURN_MOON,
    };

    int learning_order_len = sizeof(learning_order) / sizeof(learning_order[0]);

    IR_COMMANDS movie_mode[] = {
        CMD_TURN_RED,
        CMD_TURN_BLUE,
        CMD_TURN_GREEN,
    };

    int movie_mode_len = sizeof(movie_mode) / sizeof(movie_mode[0]);

    // states
    app_state_t app_state = APP_LEARNING;

    while (1)
    {

        switch (app_state)
        {
        case APP_LEARNING:
            rmt_rx_done_event_data_t rx_data;

            if (xQueueReceive(ir_queue, &rx_data, pdMS_TO_TICKS(10)))
            {
                int count = rx_data.num_symbols;
                ESP_LOGI(TAG, "Frame received: symbols = %d", rx_data.num_symbols);

                if (count > IR_LENGTH) // to do something about IR_LENGTH later
                    count = IR_LENGTH;

                if (learning_index >= learning_order_len)
                {
                    app_state = APP_IDLE;
                    break;
                }
                IR_COMMANDS cmd = learning_order[learning_index];
                ESP_LOGI(TAG,
                         "Learning step %d/%d: enum command id = %d",
                         learning_index + 1,
                         learning_order_len,
                         cmd);
                write_command(ir_commands, raw_symbols, command_lengths, count, cmd);

                send_command(tx_symbols,
                             ir_commands,
                             command_lengths,
                             tx_channel,
                             copy_encoder,
                             &tx_trans_config,
                             cmd);

                learning_index++;

                if (learning_index >= learning_order_len)
                {

                    app_state = APP_IDLE;
                    ESP_LOGI(TAG, "All commands captured!");
                }

                ESP_LOGI(TAG, "---- END FRAME ----");

                vTaskDelay(pdMS_TO_TICKS(300));
                // важливо: знову запустити прийом
                ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols,
                                            sizeof(raw_symbols), &receive_config));
            }
            break;

        case APP_IDLE:

            // BTN + DEBOUNCE
            int btn_level = gpio_get_level(BTN_GPIO_ENC);
            int64_t now_us = esp_timer_get_time();

            if (btn_level != last_btn_level)
            {
                last_btn_level = btn_level;
                last_btn_change_us = now_us;
            }

            if (now_us - last_btn_change_us > DEBOUNCE_US)
            {
                if (btn_level != stable_btn_level)
                {
                    stable_btn_level = btn_level;

                    if (stable_btn_level == 0)
                    {
                        btn_press_start_us = now_us;
                        long_press_handled = false;
                    }
                    else
                    {
                        if (!long_press_handled)
                        {
                            if (app_state != APP_RUNNING_SEQUENCE)
                            {
                                app_state = APP_RUNNING_SEQUENCE;
                            }
                        }
                    }
                }
            }

            if (stable_btn_level == 0 &&
                !long_press_handled &&
                (now_us - btn_press_start_us) > BTN_LONG_PRESS_MS * 1000ULL)
            {

                long_press_handled = true;

                ESP_LOGI(TAG, "LONG BUTTON PRESS detected");
            }

            vTaskDelay(pdMS_TO_TICKS(20));
            break;
        case APP_RUNNING_SEQUENCE:

            vTaskDelay(pdMS_TO_TICKS(500));
            send_sequence(tx_symbols,
                          ir_commands,
                          command_lengths,
                          tx_channel,
                          copy_encoder,
                          &tx_trans_config,
                          movie_mode,
                          movie_mode_len,
                          500);

            ESP_LOGI(TAG, "SHORT PRESS -> sent movie_mode sequence");
            app_state = APP_IDLE;
            break;
        default:
            app_state = APP_IDLE;
            break;
        }
        vTaskDelay(1);
    }
}