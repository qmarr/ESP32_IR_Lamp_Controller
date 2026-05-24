
#ifndef IR_COMMAND_WRITE_H
#define IR_COMMAND_WRITE_H
#define TAG_WRITE "IR_SNIFFER_WRITE"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "enums.h"

#define IR_LENGTH 34

typedef struct
{
    uint16_t duration0;
    uint16_t duration1;
} ir_symbol_t;

void build_tx_symbols_from_command(rmt_symbol_word_t out[IR_LENGTH],
                                   ir_symbol_t buffer[CMD_COUNT][IR_LENGTH],
                                   int command_lengths[CMD_COUNT],
                                   IR_COMMANDS cmd_index);

void send_command(rmt_symbol_word_t out[IR_LENGTH],
                  ir_symbol_t buffer[CMD_COUNT][IR_LENGTH],
                  int command_lengths[CMD_COUNT], rmt_channel_handle_t tx_channel,
                  rmt_encoder_handle_t encoder,
                  const rmt_transmit_config_t *conf,
                  IR_COMMANDS cmd_index);

void send_sequence(rmt_symbol_word_t out[IR_LENGTH],
                   ir_symbol_t buffer[CMD_COUNT][IR_LENGTH],
                   int command_lengths[CMD_COUNT],
                   rmt_channel_handle_t tx_channel,
                   rmt_encoder_handle_t encoder,
                   const rmt_transmit_config_t *conf,
                   IR_COMMANDS sequence[], size_t len, int delay_ms);

void write_command(ir_symbol_t to[CMD_COUNT][IR_LENGTH],
                   const rmt_symbol_word_t from[],
                   int command_lengths[CMD_COUNT],
                   const int ir_length,
                   IR_COMMANDS cmd_index);
// deleting captured as local boolean

#endif