#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "ir_command_write.h"
#include "commands.h"

#define TAG "IR_SNIFFER"

#define RMT_RX_GPIO GPIO_NUM_15
#define IR_LED GPIO_NUM_16
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

void app_main(void)
{
    ESP_LOGI(TAG, "Starting IR sniffer...");

    ir_queue = xQueueCreate(QUEUE_SIZE, sizeof(rmt_rx_done_event_data_t));

    // 1. Конфіг RX каналу
    rmt_rx_channel_config_t rx_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = MEM_BLOCK_SYMBOLS,
        .gpio_num = RMT_RX_GPIO,
    };

    rmt_channel_handle_t rx_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_config, &rx_channel));

    // 2. Callback
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = ir_rx_done_callback,
    };

    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, ir_queue));

    // 3. Enable channel
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    // 4. Буфер для прийому
    rmt_symbol_word_t raw_symbols[MEM_BLOCK_SYMBOLS];

    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = 1000,
        .signal_range_max_ns = 10000000,
    };

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

    rmt_channel_handle_t tx_channel = NULL;
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
    rmt_encoder_handle_t copy_encoder = NULL;
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &copy_encoder));

    // transmit

    rmt_transmit_config_t tx_trans_config = {
        .loop_count = 0, // 0 = one loop
    };
    // 7. Enable tx channel
    ESP_ERROR_CHECK(rmt_enable(tx_channel));

    ir_symbol_t ir_commands[CMD_COUNT][IR_LENGTH];
    int command_lengths[CMD_COUNT] = {0};
    rmt_symbol_word_t tx_symbols[IR_LENGTH];
    bool captured = false;
    int cmd_index = 0;

    bool learning_mode = true;

    while (1)
    {
        rmt_rx_done_event_data_t rx_data;

        if (xQueueReceive(ir_queue, &rx_data, portMAX_DELAY))
        {
            int count = rx_data.num_symbols;
            ESP_LOGI(TAG, "Frame received: symbols = %d", rx_data.num_symbols);

            if (count > 34) // to do something about IR_LENGTH later
                count = 34;

            int curr_cmd_index = cmd_index;
            if (learning_mode)
            {
                write_command(ir_commands, raw_symbols, command_lengths, count, curr_cmd_index);

                
                cmd_index++;


                if (cmd_index >= CMD_COUNT)
                {
                    learning_mode = false;
                    ESP_LOGI(TAG, "All commands captured!");
                }
            }

            if (captured) // true for test -> !captured <-
            {

                send_command(tx_symbols,
                             ir_commands,
                             command_lengths,
                             tx_channel,
                             copy_encoder,
                             &tx_trans_config,
                             curr_cmd_index);
                captured = false;
            }
            else
            {
                ESP_LOGI(TAG, "Frame ignored, already captured");
            }
            ESP_LOGI(TAG, "---- END FRAME ----");

            // важливо: знову запустити прийом
            ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols,
                                        sizeof(raw_symbols), &receive_config));
        }
    }
}