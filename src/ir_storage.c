#include "ir_storage.h"

esp_err_t ir_storage_init(void)
{

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static void make_command_key(IR_COMMANDS cmd, char key[16])
{
    snprintf(key, 16, "cmd%d", (int)cmd);
}

esp_err_t ir_storage_save_command(IR_COMMANDS cmd, const ir_symbol_t symbols[IR_LENGTH], int length)
{

    if (length <= 0 || length > IR_LENGTH)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;

    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    stored_ir_command_t stored = {
        .length = length,
    };

    memcpy(stored.symbols, symbols, length * sizeof(ir_symbol_t));

    char key[16];
    make_command_key(cmd, key);

    err = nvs_set_blob(handle, key, &stored, sizeof(stored));

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    return err;
}

esp_err_t ir_storage_load_command(IR_COMMANDS cmd, ir_symbol_t symbols[IR_LENGTH], int *length)
{
    // Open
    nvs_handle_t handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);

    if (err != ESP_OK)
        return err;

    stored_ir_command_t stored;
    size_t required_size = sizeof(stored);

    char key[16];
    make_command_key(cmd, key);

    err = nvs_get_blob(handle, key, &stored, &required_size);
    nvs_close(handle);

    if (err != ESP_OK)
        return err;

    if (stored.length <= 0 || stored.length > IR_LENGTH)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(symbols, stored.symbols, stored.length * sizeof(ir_symbol_t));
    *length = stored.length;

    return ESP_OK;
}

bool ir_storage_load_required_commands(ir_symbol_t commands[CMD_COUNT][IR_LENGTH], int lengths[CMD_COUNT], const IR_COMMANDS required[], int required_len)
{

    for (size_t i = 0; i < required_len; i++)
    {
        IR_COMMANDS cmd = required[i];

        esp_err_t err = ir_storage_load_command(cmd, commands[cmd], &lengths[cmd]);

        if (err != ESP_OK)
        {
            ESP_LOGW(TAG_IR_S,
                     "Command %d not found in NVS: %s",
                     cmd,
                     esp_err_to_name(err));
            return false;
        }
    }
    return true;
}

esp_err_t ir_storage_erase_all_commands(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;
        
    nvs_erase_all(handle);
    nvs_commit(handle);

    return err;
}
