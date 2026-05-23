#include "ir_command_write.h"

void build_tx_symbols_from_command(rmt_symbol_word_t out[IR_LENGTH],
                                   ir_symbol_t buffer[CMD_COUNT][IR_LENGTH],
                                   int command_lengths[CMD_COUNT],
                                   IR_COMMANDS cmd)
{
    for (size_t j = 0; j < command_lengths[cmd]; j++)
    {
        out[j].level0 = 1;
        out[j].duration0 = buffer[cmd][j].duration0;
        out[j].level1 = 0;
        out[j].duration1 = buffer[cmd][j].duration1;
    }
}

void send_command(rmt_symbol_word_t out[IR_LENGTH],
                  ir_symbol_t buffer[CMD_COUNT][IR_LENGTH],
                  int command_lengths[CMD_COUNT],
                  rmt_channel_handle_t tx_channel,
                  rmt_encoder_handle_t encoder,
                  const rmt_transmit_config_t *conf,
                  IR_COMMANDS cmd_index)
{
    build_tx_symbols_from_command(out, buffer, command_lengths, cmd_index);

    ESP_ERROR_CHECK(rmt_transmit(tx_channel, encoder, out, command_lengths[cmd_index] * sizeof(rmt_symbol_word_t), conf));

    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_channel, portMAX_DELAY));
}

void send_sequence(rmt_symbol_word_t out[IR_LENGTH],
                   ir_symbol_t buffer[CMD_COUNT][IR_LENGTH],
                   int command_lengths[CMD_COUNT],
                   rmt_channel_handle_t tx_channel,
                   rmt_encoder_handle_t encoder,
                   const rmt_transmit_config_t *conf,
                   IR_COMMANDS sequence[], size_t len, int delay_ms)
{

    for (size_t i = 0; i < len; i++)
    {
        ESP_LOGI("Sequence", "Send command %d", i);
        send_command(out, buffer, command_lengths, tx_channel, encoder, conf, sequence[i]);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void write_command(ir_symbol_t to[CMD_COUNT][IR_LENGTH],
                   const rmt_symbol_word_t from[],
                   int command_lengths[CMD_COUNT],
                   const int ir_length,
                   IR_COMMANDS cmd_index)
{

    for (size_t j = 0; j < ir_length; j++)
    {
        rmt_symbol_word_t s = from[j];
        to[cmd_index][j].duration0 = s.duration0;
        to[cmd_index][j].duration1 = s.duration1;
        ESP_LOGI(TAG_WRITE,
                 "[%d] l0=%d d0=%d | l1=%d d1=%d",
                 j,
                 s.level0, s.duration0,
                 s.level1, s.duration1);
    }
    
    command_lengths[cmd_index] = ir_length;
    ESP_LOGI(TAG_WRITE, "Command[%d] captured: %d symbols", cmd_index, command_lengths[cmd_index]);
}
