#ifndef APP_SEQUENCES_H
#define APP_SEQUENCES_H

#include "enums.h"

extern const char *command_names[CMD_COUNT];

extern const IR_COMMANDS learning_order[];
extern const int learning_order_len;

extern const IR_COMMANDS movie_mode[];
extern const int movie_mode_len;

extern const IR_COMMANDS dark_room_power_mode[];
extern const int dark_room_power_mode_len;

#endif  