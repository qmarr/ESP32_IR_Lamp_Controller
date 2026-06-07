#ifndef APP_STATUS_H
#define APP_STATUS_H

#include <stdbool.h>
#include "enums.h"
#include "app_sequences.h"

typedef struct
{
    app_state_t app_state;
    scene_id_t selected_scene;
    scene_id_t active_scene;
    bool has_active_scene;
    bool lamp_assumed_on;
    bool room_dark;
} app_status_t;

#endif /* APP_STATUS_H */
