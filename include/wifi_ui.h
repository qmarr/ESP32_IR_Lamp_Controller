#ifndef WIFI_UI_H
#define WIFI_UI_H

#include "app_sequences.h"
#include "app_status.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"



#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define GTK_REKEY_INTERVAL CONFIG_ESP_GTK_REKEY_INTERVAL
#else
#define GTK_REKEY_INTERVAL 0
#endif

typedef enum
{
    WEB_REQ_NONE,
    WEB_REQ_RUN_SCENE,
    WEB_REQ_POWER_TOGGLE,
    WEB_REQ_RELEARN,
} web_request_type_t;

typedef struct
{
    web_request_type_t type;
    scene_id_t scene;
} web_request_t;


void web_ui_start(QueueHandle_t queue, app_status_t *status, SemaphoreHandle_t app_status_mutex);

#endif // WIFI_UI_H
