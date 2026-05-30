#ifndef APP_SEQUENCES_H
#define APP_SEQUENCES_H

#include "enums.h"
#include <stdio.h>

typedef enum
{
    SCENE_FAVOURITE,
    SCENE_OCEAN,
    SCENE_TENSION,
    SCENE_MOVIE,

    SCENE_COUNT
} scene_id_t;

typedef struct
{
    const char *name;
    const IR_COMMANDS *enter_sequence;
    size_t enter_length;
    const IR_COMMANDS *exit_sequence;
    size_t exit_length;
} scene_t;

extern const char *command_names[CMD_COUNT];

extern const IR_COMMANDS learning_order[];
extern const int learning_order_len;

extern const IR_COMMANDS dark_room_power_mode[];
extern const int dark_room_power_mode_len;

extern const scene_t scenes[SCENE_COUNT];

#endif