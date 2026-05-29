#include "app_sequences.h"

const char *command_names[CMD_COUNT] = {

    "RED",
    "BLUE",
    "GREEN",
    "WHITE",
    "MOON",
    "STARS",
    "NORTHERN_LIGHTS",
    "BRIGHTNESS_DOWN",
    "BRIGHTNESS_UP",
    "POWER",
};

// sequence
const IR_COMMANDS learning_order[] = {
    CMD_TURN_RED,
    CMD_TURN_BLUE,
    CMD_TURN_GREEN,
    CMD_TURN_WHITE,
    CMD_TURN_MOON,
    CMD_TURN_STARS,
    CMD_TURN_NORTHERN_LIGHTS,
    CMD_BRIGHTNESS_DOWN,
    CMD_BRIGHTNESS_UP,
    CMD_POWER,
};

const int learning_order_len = sizeof(learning_order) / sizeof(learning_order[0]);

const IR_COMMANDS movie_mode[] = {
    CMD_TURN_RED,
    CMD_TURN_BLUE,
    CMD_TURN_GREEN,
};

const int movie_mode_len = sizeof(movie_mode) / sizeof(movie_mode[0]);

const IR_COMMANDS dark_room_power_mode[] = {
    CMD_POWER
};

const int dark_room_power_mode_len = sizeof(dark_room_power_mode) / sizeof(dark_room_power_mode[0]);
