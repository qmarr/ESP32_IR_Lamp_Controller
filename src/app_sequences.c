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

const IR_COMMANDS dark_room_power_mode[] = {
    CMD_POWER,
};

const int dark_room_power_mode_len = sizeof(dark_room_power_mode) / sizeof(dark_room_power_mode[0]);

const IR_COMMANDS movie_mode[] = {
    CMD_TURN_RED,
    CMD_TURN_BLUE,
    CMD_TURN_GREEN,
};

const IR_COMMANDS favourite_mode[] = {
    CMD_TURN_RED,
    CMD_TURN_BLUE,
};

const IR_COMMANDS ocean_mode[] = {
    CMD_TURN_BLUE,
    CMD_TURN_GREEN,
};

const IR_COMMANDS tension_mode[] = {
    CMD_TURN_RED,
};

const scene_t scenes[SCENE_COUNT] = {
    [SCENE_FAVOURITE] = {
        .name = "Fav",
        .enter_sequence = favourite_mode,
        .enter_length = sizeof(favourite_mode) / sizeof(favourite_mode[0]),
        .exit_sequence = favourite_mode,
        .exit_length = sizeof(favourite_mode) / sizeof(favourite_mode[0]),
    },

    [SCENE_OCEAN] = {
        .name = "Ocean",
        .enter_sequence = ocean_mode,
        .enter_length = sizeof(ocean_mode) / sizeof(ocean_mode[0]),
        .exit_sequence = ocean_mode,
        .exit_length = sizeof(ocean_mode) / sizeof(ocean_mode[0]),
    },

    [SCENE_TENSION] = {
        .name = "Tension",
        .enter_sequence = tension_mode,
        .enter_length = sizeof(tension_mode) / sizeof(tension_mode[0]),
        .exit_sequence = tension_mode,
        .exit_length = sizeof(tension_mode) / sizeof(tension_mode[0]),
    },

    [SCENE_MOVIE] = {
        .name = "Movie",
        .enter_sequence = movie_mode,
        .enter_length = sizeof(movie_mode) / sizeof(movie_mode[0]),
        .exit_sequence = movie_mode,
        .exit_length = sizeof(movie_mode) / sizeof(movie_mode[0]),
    }};
