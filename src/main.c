#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "ir_commands.h"
#include "enums.h"
#include "app_sequences.h"
#include "ir_storage.h"
#include "ldr_sensor.h"
#include "sleep.h"
#include "display.h"
#include "wifi_ui.h"

#define TAG "IR_SNIFFER"

#define RMT_RX_GPIO GPIO_NUM_15
#define IR_LED GPIO_NUM_16
#define BTN_GPIO_ENC GPIO_NUM_4
#define ENC_DT_GPIO GPIO_NUM_38
#define ENC_CLK_GPIO GPIO_NUM_39

#define DEBOUNCE_US 50000 // 50ms
#define BTN_LONG_PRESS_MS 2000
#define RMT_RESOLUTION_HZ 1000000         // 1us per tick
#define LDR_CHECK_US 500000               // 500ms
#define ENCODER_ROTATE_COOLDOWN_US 500000 // 500ms
#define SLEEP_TIMEOUT_US 60000000         // 60 seconds
#define DISPLAY_UPDATE_US 500000

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

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t oled_dev_handle;

static ir_symbol_t ir_commands[CMD_COUNT][IR_LENGTH];
static int command_lengths[CMD_COUNT] = {0};
static rmt_symbol_word_t tx_symbols[IR_LENGTH];

int64_t last_activity_us = 0;

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

    // 4. Tx channel config
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

    // 5. carrier config
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

    // 6. Enable tx channel
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
}

void encoder_init()
{
    ESP_LOGI(TAG, "Encoder button init");
    gpio_config_t button_gpio_config = {
        .pin_bit_mask = (1ULL << BTN_GPIO_ENC) |
                        (1ULL << ENC_CLK_GPIO) |
                        (1ULL << ENC_DT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&button_gpio_config));
}

void i2c_init()
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    i2c_device_config_t oled_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &oled_dev_cfg, &oled_dev_handle));
}

static void mark_activity()
{
    last_activity_us = esp_timer_get_time();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting IR sniffer...");

    ir_queue = xQueueCreate(QUEUE_SIZE, sizeof(rmt_rx_done_event_data_t));

    rmt_init();
    encoder_init();
    ldr_init();
    i2c_init();
    ssd1306_init(oled_dev_handle);
    ssd1306_clear(oled_dev_handle);
    mark_activity();

    // variables
    int learning_index = 0;
    bool rmt_armed = false;

    // display
    static int64_t last_display_update_us = 0;
    static int last_displayed_learning_index = -1;

    // scene
    scene_id_t selected_scene = SCENE_MOVIE;
    scene_id_t active_scene = SCENE_MOVIE;
    bool has_active_scene = false;
    // ldr
    bool lamp_assumed_on = false;
    bool room_dark = false;
    static int64_t last_ldr_check_us = 0;

    // btn variables
    int last_btn_level = 1;
    int stable_btn_level = 1;
    int64_t last_btn_change_us = 0;

    int64_t btn_press_start_us = 0;
    bool long_press_handled = false;

    // encoder
    int last_clk = gpio_get_level(ENC_CLK_GPIO);
    static int64_t last_encoder_event_us = 0;

    // sleep mode

    // states
    app_state_t app_state = APP_LEARNING;

    // WIFI
    static bool web_ui_enabled = true;
    // QueueHandle_t web_request_queue = NULL;
    // web_request_queue = xQueueCreate(5, sizeof(web_request_t));

    ESP_ERROR_CHECK(ir_storage_init());

    // wifi
    QueueHandle_t web_queue = xQueueCreate(5, sizeof(web_request_t));
    if (web_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create web request queue");
    }
    else
    {
        web_ui_start(web_queue);
    }

    if (ir_storage_load_required_commands(ir_commands,
                                          command_lengths,
                                          learning_order,
                                          learning_order_len))
    {
        app_state = APP_IDLE;
        ESP_LOGI(TAG, "Loaded commands from NVS. State -> APP_IDLE");
        mark_activity();
    }
    else
    {
        app_state = APP_LEARNING;
        ESP_LOGI(TAG, "No saved commands. State -> APP_LEARNING");
        mark_activity();
    }

    while (1)
    {
        switch (app_state)
        {
        case APP_LEARNING:
        {

            rmt_rx_done_event_data_t rx_data;
            if (!rmt_armed)
            {
                // Буфер для прийому
                ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols,
                                            sizeof(raw_symbols), &receive_config));

                rmt_armed = true;
            }

            if (learning_index >= learning_order_len)
            {
                app_state = APP_IDLE;
                break;
            }

            IR_COMMANDS cmd = learning_order[learning_index];
            if (last_displayed_learning_index != learning_index)
            {
                IR_COMMANDS cmd = learning_order[learning_index];

                ssd1306_clear(oled_dev_handle);
                display_show_learning(oled_dev_handle,
                                      command_names[cmd],
                                      learning_index + 1,
                                      learning_order_len);

                last_displayed_learning_index = learning_index;
            }

            if (xQueueReceive(ir_queue, &rx_data, pdMS_TO_TICKS(10)))
            {
                rmt_armed = false;

                int count = rx_data.num_symbols;
                ESP_LOGI(TAG, "Frame received: symbols = %d", rx_data.num_symbols);

                if (count > IR_LENGTH) // to do something about IR_LENGTH later
                    count = IR_LENGTH;

                ESP_LOGI(TAG, "Learning command: %s", command_names[cmd]);
                write_command(ir_commands, raw_symbols, command_lengths, count, cmd);

                ESP_ERROR_CHECK(ir_storage_save_command(cmd,
                                                        ir_commands[cmd],
                                                        command_lengths[cmd]));
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
                    rmt_armed = false;
                    mark_activity();
                    ESP_LOGI(TAG, "All commands captured!");
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(300));
                    // важливо: знову запустити прийом
                    ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols,
                                                sizeof(raw_symbols), &receive_config));
                    rmt_armed = true;
                }

                ESP_LOGI(TAG, "---- END FRAME ----");
            }
            break;
        }
        case APP_IDLE:
        {
            int64_t now_us = esp_timer_get_time();

            web_request_t web_req;

            if (xQueueReceive(web_queue, &web_req, 0))
            {
                mark_activity();

                switch (web_req.type)
                {
                case WEB_REQ_RUN_SCENE:
                    selected_scene = web_req.scene;
                    app_state = APP_RUNNING_SEQUENCE;
                    break;

                case WEB_REQ_POWER_TOGGLE:
                    app_state = APP_TURN_POWER_ON;
                    break;

                default:
                    break;
                }

                break;
            }

            // display
            if (now_us - last_display_update_us > DISPLAY_UPDATE_US)
            {
                display_show_idle(oled_dev_handle, scenes[selected_scene].name, has_active_scene ? scenes[active_scene].name : "None", lamp_assumed_on, room_dark);
                last_display_update_us = now_us;
            }

            if (!web_ui_enabled &&
                now_us - last_activity_us > SLEEP_TIMEOUT_US)
            {
                ESP_LOGI(TAG, "Idle timeout -> APP_SLEEP_PREPARE");
                app_state = APP_SLEEP_PREPARE;
                break;
            }

            if (now_us - last_ldr_check_us > LDR_CHECK_US)
            {
                int ldr_avg_value = adc_read_avg(adc_read_raw());

                room_dark = room_is_dark(ldr_avg_value);
                // ESP_LOGI("ADC LDR", "avg=%d dark=%d lamp assumed on =%d", ldr_avg_value, room_dark, lamp_assumed_on);
                if (room_dark && !lamp_assumed_on)
                {
                    mark_activity();
                    app_state = APP_TURN_POWER_ON;
                    break;
                }
                last_ldr_check_us = now_us; // check behaviour later
            }

            int clk = gpio_get_level(ENC_CLK_GPIO);
            int dt = gpio_get_level(ENC_DT_GPIO);
            // ESP_LOGI(TAG, "ENC edge: clk=%d dt=%d", clk, dt);
            if (last_clk == 1 && clk == 0)
            {
                if (now_us - last_encoder_event_us > ENCODER_ROTATE_COOLDOWN_US)
                {
                    last_encoder_event_us = now_us;

                    if (dt == 1)
                    {
                        selected_scene = (selected_scene + 1) % SCENE_COUNT;
                        mark_activity();
                    }
                    else
                    {
                        selected_scene = (selected_scene + SCENE_COUNT - 1) % SCENE_COUNT;
                        mark_activity();
                    }

                    ESP_LOGI(TAG, "Selected scene: %s", scenes[selected_scene].name);
                }
            }

            // BTN + DEBOUNCE
            int btn_level = gpio_get_level(BTN_GPIO_ENC);

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
                                mark_activity();
                                app_state = APP_RUNNING_SEQUENCE;
                                break;
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

                ESP_LOGI(TAG, "LONG PRESS -> clearing NVS and entering learning mode");

                ESP_ERROR_CHECK(ir_storage_erase_all_commands());

                memset(command_lengths, 0, sizeof(command_lengths));

                learning_index = 0;
                rmt_armed = false;
                ESP_LOGI(TAG, "No saved commands. APP_LEARNING");
                mark_activity();
                app_state = APP_LEARNING;
                xQueueReset(ir_queue);
            }

            vTaskDelay(pdMS_TO_TICKS(2));
            break;
        }
        case APP_RUNNING_SEQUENCE:
        {
            mark_activity();
            vTaskDelay(pdMS_TO_TICKS(700));
            if (has_active_scene)
            {
                if (active_scene == selected_scene)
                {
                    ESP_LOGI(TAG, "Scene disabled: %s", scenes[active_scene].name);
                    send_sequence(tx_symbols,
                                  ir_commands,
                                  command_lengths,
                                  tx_channel,
                                  copy_encoder,
                                  &tx_trans_config,
                                  scenes[active_scene].exit_sequence,
                                  scenes[active_scene].exit_length,
                                  500);

                    has_active_scene = false;
                    app_state = APP_IDLE;
                    break;
                }
                ESP_LOGI(TAG, "Exiting scene: %s", scenes[active_scene].name);
                send_sequence(tx_symbols,
                              ir_commands,
                              command_lengths,
                              tx_channel,
                              copy_encoder,
                              &tx_trans_config,
                              scenes[active_scene].exit_sequence,
                              scenes[active_scene].exit_length,
                              500);
            }
            ESP_LOGI(TAG, "Entering scene: %s", scenes[selected_scene].name);

            // to do length check
            send_sequence(tx_symbols,
                          ir_commands,
                          command_lengths,
                          tx_channel,
                          copy_encoder,
                          &tx_trans_config,
                          scenes[selected_scene].enter_sequence,
                          scenes[selected_scene].enter_length,
                          500);
            active_scene = selected_scene;
            has_active_scene = true;
            lamp_assumed_on = true;
            app_state = APP_IDLE;
            break;
        }
        case APP_TURN_POWER_ON:
        {
            ESP_LOGI(TAG, "Turning power on");
            vTaskDelay(pdMS_TO_TICKS(700));
            send_sequence(tx_symbols,
                          ir_commands,
                          command_lengths,
                          tx_channel,
                          copy_encoder,
                          &tx_trans_config,
                          dark_room_power_mode,
                          dark_room_power_mode_len,
                          500);
            mark_activity();
            lamp_assumed_on = true;
            app_state = APP_IDLE;
            break;
        }
        case APP_SLEEP_PREPARE:
        {
            ESP_LOGI(TAG, "Preparing to enter sleep");
            app_state = APP_SLEEP;
            break;
        }
        case APP_SLEEP:
        {
            ssd1306_clear(oled_dev_handle);
            display_show_sleep(oled_dev_handle);
            enter_light_sleep(BTN_GPIO_ENC);
            ssd1306_clear(oled_dev_handle);
            mark_activity();
            app_state = APP_IDLE;
            break;
        }
        default:
        {
            app_state = APP_IDLE;
            break;
        }
        }
        vTaskDelay(1);
    }
}