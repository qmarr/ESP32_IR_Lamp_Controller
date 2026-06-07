#include "wifi_ui.h"
#include "string.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define ESP_WIFI_SSID "IR_Lamp_Controller"
#define ESP_WIFI_PASS "12345678"

#define ESP_WIFI_CHANNEL 1
#define MAX_STA_CONN 2

static const char *TAG = "wifi softAP";
static QueueHandle_t web_request_queue = NULL;
static SemaphoreHandle_t status_mutex = NULL;
static app_status_t *shared_status = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

static void wifi_init_softap(void)

{
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(err);
    }
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
#ifdef CONFIG_ESP_WIFI_BSS_MAX_IDLE_SUPPORT
            .bss_max_idle_cfg = {
                .period = WIFI_AP_DEFAULT_MAX_IDLE_PERIOD,
                .protected_keep_alive = 1,
            },
#endif
            .gtk_rekey_interval = GTK_REKEY_INTERVAL,
        },
    };
    if (strlen(ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP started");
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}

static const char *safe_scene_name(scene_id_t scene)
{
    if (scene >= 0 && scene < SCENE_COUNT)
    {
        return scenes[scene].name;
    }

    return "Unknown";
}

static esp_err_t root_get_handler(httpd_req_t *req)
{

    app_status_t local_status = {
        .app_state = APP_IDLE,
        .selected_scene = SCENE_MOVIE,
        .active_scene = SCENE_MOVIE,
        .has_active_scene = false,
        .lamp_assumed_on = false,
        .room_dark = false,
    };

    if (status_mutex && shared_status &&
        xSemaphoreTake(status_mutex, pdMS_TO_TICKS(20)) == pdTRUE)
    {
        local_status = *shared_status;
        xSemaphoreGive(status_mutex);
    }

    char html[2048];
    const char *selected_name = safe_scene_name(local_status.selected_scene);
    const char *active_name = local_status.has_active_scene
                                  ? safe_scene_name(local_status.active_scene)
                                  : "None";

    snprintf(html, sizeof(html),
             "<!DOCTYPE html>"
             "<html><body>"
             "<h1>IR Lamp Controller</h1>"
             "<p>Selected: %s</p>"
             "<p>Active: %s</p>"
             "<p>Lamp: %s</p>"
             "<p>Room: %s</p>"
             "<a href='/scene/favourite'><button>Favourite</button></a><br><br>"
             "<a href='/scene/ocean'><button>Ocean</button></a><br><br>"
             "<a href='/scene/tension'><button>Tension</button></a><br><br>"
             "<a href='/scene/movie'><button>Movie</button></a><br><br>"
             "<p>Control:</p>"
             "<a href='/power'><button>Power</button></a>"

             "</body></html>",
             selected_name,
             active_name,
             local_status.lamp_assumed_on ? "ON" : "OFF",
             local_status.room_dark ? "Dark" : "Light");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t scene_favourite_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WEB: Favourite pressed");

    if (web_request_queue != NULL)
    {
        web_request_t web_req = {
            .type = WEB_REQ_RUN_SCENE,
            .scene = SCENE_FAVOURITE,
        };

        xQueueSend(web_request_queue, &web_req, 0);
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t scene_ocean_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WEB: Ocean pressed");

    if (web_request_queue != NULL)
    {
        web_request_t web_req = {
            .type = WEB_REQ_RUN_SCENE,
            .scene = SCENE_OCEAN,
        };

        xQueueSend(web_request_queue, &web_req, 0);
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}
static esp_err_t tension_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WEB: Tension pressed");
    if (web_request_queue != NULL)
    {
        web_request_t web_req = {
            .type = WEB_REQ_RUN_SCENE,
            .scene = SCENE_TENSION,
        };

        xQueueSend(web_request_queue, &web_req, 0);
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}
static esp_err_t power_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WEB: Power pressed");

    if (web_request_queue != NULL)
    {
        web_request_t web_req = {
            .type = WEB_REQ_POWER_TOGGLE,
        };

        xQueueSend(web_request_queue, &web_req, 0);
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}
static esp_err_t movie_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WEB: Movie pressed");

    if (web_request_queue != NULL)
    {
        web_request_t web_req = {
            .type = WEB_REQ_RUN_SCENE,
            .scene = SCENE_MOVIE,
        };

        xQueueSend(web_request_queue, &web_req, 0);
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL,
};

static httpd_uri_t favourite_uri = {
    .uri = "/scene/favourite",
    .method = HTTP_GET,
    .handler = scene_favourite_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t scene_ocean_uri = {
    .uri = "/scene/ocean",
    .method = HTTP_GET,
    .handler = scene_ocean_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t tension_uri = {
    .uri = "/scene/tension",
    .method = HTTP_GET,
    .handler = tension_handler,
    .user_ctx = NULL,
};
static const httpd_uri_t movie_uri = {
    .uri = "/scene/movie",
    .method = HTTP_GET,
    .handler = movie_handler,
    .user_ctx = NULL,
};
static const httpd_uri_t power_uri = {
    .uri = "/power",
    .method = HTTP_GET,
    .handler = power_handler,
    .user_ctx = NULL,
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return NULL;
    }

    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &favourite_uri);
    httpd_register_uri_handler(server, &scene_ocean_uri);
    httpd_register_uri_handler(server, &tension_uri);
    httpd_register_uri_handler(server, &movie_uri);
    httpd_register_uri_handler(server, &power_uri);

    ESP_LOGI(TAG, "HTTP server started");

    return server;
}

static bool web_ui_started = false;
static httpd_handle_t web_server = NULL;

void web_ui_start(QueueHandle_t queue, app_status_t *status, SemaphoreHandle_t app_status_mutex)
{
    if (web_ui_started)
    {
        ESP_LOGW(TAG, "Web UI already started, skipping init");
        return;
    }

    web_ui_started = true;

    web_request_queue = queue;
    shared_status = status;
    status_mutex = app_status_mutex;

    wifi_init_softap();
    web_server = start_webserver();
}