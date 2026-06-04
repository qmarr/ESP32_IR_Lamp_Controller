#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "font5x7.h"
#include "esp_log.h"
#include "app_sequences.h"
#include <stdio.h>

#define I2C_MASTER_SCL_IO GPIO_NUM_9
#define I2C_MASTER_SDA_IO GPIO_NUM_8
#define I2C_MASTER_NUM I2C_NUM_0 // I2C port 0
#define I2C_MASTER_FREQ_HZ 100000
#define OLED_ADDR 0x3C
#define OLED_I2C_TIMEOUT_MS 100

esp_err_t ssd1306_cmd(i2c_master_dev_handle_t dev, uint8_t cmd);
esp_err_t ssd1306_cmd2(i2c_master_dev_handle_t dev, uint8_t cmd, uint8_t arg);
esp_err_t ssd1306_data(i2c_master_dev_handle_t dev, const uint8_t *data, size_t len);
void ssd1306_init(i2c_master_dev_handle_t dev);
void ssd1306_set_full_area(i2c_master_dev_handle_t dev);
void ssd1306_clear(i2c_master_dev_handle_t dev);
const uint8_t *get_char_bitmap(char c);
void ssd1306_draw_char(i2c_master_dev_handle_t dev, char c);
void ssd1306_draw_string(i2c_master_dev_handle_t dev, const char *str);
void ssd1306_set_cursor(i2c_master_dev_handle_t dev, uint8_t page, uint8_t col);
void ssd1306_print_at(i2c_master_dev_handle_t dev, uint8_t page, uint8_t col, const char *text);
void ssd1306_clear_line(i2c_master_dev_handle_t dev, uint8_t page);

void display_show_idle(i2c_master_dev_handle_t oled_handle, const char *selected_scene, const char *active_scene, bool lamp_on, bool room_dark);

void display_show_learning(i2c_master_dev_handle_t oled_handle, const char *command_name, int step, int total);

void display_show_sleep(i2c_master_dev_handle_t oled_handle);

#endif // DISPLAY_H
