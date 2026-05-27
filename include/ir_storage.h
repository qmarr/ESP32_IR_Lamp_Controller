#ifndef IR_STORAGE_H
#define IR_STORAGE_H

#include "nvs.h"
#include "nvs_flash.h"
#include "ir_commands.h"
#include "esp_err.h"
#include "enums.h"

#define TAG_IR_S "IR_STORAGE"
#define STORAGE_NAMESPACE "storage"

typedef struct
{
    int length;
    ir_symbol_t symbols[IR_LENGTH];
} stored_ir_command_t;

esp_err_t ir_storage_init(void);

esp_err_t ir_storage_save_command(IR_COMMANDS cmd,
                                  const ir_symbol_t symbols[IR_LENGTH],
                                  int length);

esp_err_t ir_storage_load_command(IR_COMMANDS cmd,
                                  ir_symbol_t symbols[IR_LENGTH],
                                  int *length);

bool ir_storage_load_required_commands(ir_symbol_t commands[CMD_COUNT][IR_LENGTH],
                                       int lengths[CMD_COUNT],
                                       const IR_COMMANDS required[],
                                       int required_len);
                                       
esp_err_t ir_storage_erase_all_commands(void);

#endif
