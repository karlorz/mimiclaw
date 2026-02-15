#include "gateway/ws_server.h"
#include "mimi_config.h"
#include "bus/message_bus.h"
#include "platform/config_host.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <libwebsockets.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "ws_host";

static struct lws_context *s_context = NULL;
static pthread_t s_thread;
static bool s_running = false;
static bool s_thread_started = false;

typedef struct {
    struct lws *wsi;
    char chat_id[32];
    bool active;
    char *pending;
} ws_client_t;

static ws_client_t s_clients[MIMI_WS_MAX_CLIENTS];
static pthread_mutex_t s_clients_mu = PTHREAD_MUTEX_INITIALIZER;

static ws_client_t *find_client_by_wsi(struct lws *wsi)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].wsi == wsi) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static ws_client_t *find_client_by_chat_id(const char *chat_id)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && strcmp(s_clients[i].chat_id, chat_id) == 0) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static void remove_client_locked(struct lws *wsi)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].wsi == wsi) {
            ESP_LOGI(TAG, "Client disconnected: %s", s_clients[i].chat_id);
            free(s_clients[i].pending);
            memset(&s_clients[i], 0, sizeof(s_clients[i]));
            return;
        }
    }
}

static void add_client_locked(struct lws *wsi)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            s_clients[i].wsi = wsi;
            snprintf(s_clients[i].chat_id, sizeof(s_clients[i].chat_id), "ws_%p", (void *)wsi);
            s_clients[i].active = true;
            s_clients[i].pending = NULL;
            ESP_LOGI(TAG, "Client connected: %s", s_clients[i].chat_id);
            return;
        }
    }

    ESP_LOGW(TAG, "Max clients reached, closing socket");
    lws_close_reason(wsi, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION,
                     (unsigned char *)"too many clients", 16);
}

static int ws_callback(struct lws *wsi,
                       enum lws_callback_reasons reason,
                       void *user,
                       void *in,
                       size_t len)
{
    (void)user;

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            pthread_mutex_lock(&s_clients_mu);
            add_client_locked(wsi);
            pthread_mutex_unlock(&s_clients_mu);
            break;
        }

        case LWS_CALLBACK_CLOSED: {
            pthread_mutex_lock(&s_clients_mu);
            remove_client_locked(wsi);
            pthread_mutex_unlock(&s_clients_mu);
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            char *payload = calloc(1, len + 1);
            if (!payload) break;
            memcpy(payload, in, len);
            payload[len] = '\0';

            cJSON *root = cJSON_Parse(payload);
            free(payload);
            if (!root) break;

            cJSON *type = cJSON_GetObjectItem(root, "type");
            cJSON *content = cJSON_GetObjectItem(root, "content");
            cJSON *cid = cJSON_GetObjectItem(root, "chat_id");

            if (type && cJSON_IsString(type) && strcmp(type->valuestring, "message") == 0 &&
                content && cJSON_IsString(content)) {

                char chat_id[32] = "ws_unknown";

                pthread_mutex_lock(&s_clients_mu);
                ws_client_t *client = find_client_by_wsi(wsi);
                if (client) {
                    snprintf(chat_id, sizeof(chat_id), "%s", client->chat_id);
                }
                if (cid && cJSON_IsString(cid) && cid->valuestring[0]) {
                    snprintf(chat_id, sizeof(chat_id), "%s", cid->valuestring);
                    if (client) {
                        snprintf(client->chat_id, sizeof(client->chat_id), "%s", chat_id);
                    }
                }
                pthread_mutex_unlock(&s_clients_mu);

                mimi_msg_t msg = {0};
                snprintf(msg.channel, sizeof(msg.channel), "%s", MIMI_CHAN_WEBSOCKET);
                snprintf(msg.chat_id, sizeof(msg.chat_id), "%s", chat_id);
                msg.content = strdup(content->valuestring);
                if (msg.content) {
                    message_bus_push_inbound(&msg);
                }
            }

            cJSON_Delete(root);
            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            pthread_mutex_lock(&s_clients_mu);
            ws_client_t *client = find_client_by_wsi(wsi);
            if (client && client->pending) {
                size_t plen = strlen(client->pending);
                unsigned char *buf = malloc(LWS_PRE + plen);
                if (buf) {
                    memcpy(buf + LWS_PRE, client->pending, plen);
                    int n = lws_write(wsi, buf + LWS_PRE, plen, LWS_WRITE_TEXT);
                    free(buf);
                    if (n < 0) {
                        ESP_LOGW(TAG, "lws_write failed for %s", client->chat_id);
                    }
                }
                free(client->pending);
                client->pending = NULL;
            }
            pthread_mutex_unlock(&s_clients_mu);
            break;
        }

        default:
            break;
    }

    return 0;
}

static void *ws_thread_main(void *arg)
{
    (void)arg;
    while (s_running) {
        lws_service(s_context, 100);
    }
    return NULL;
}

mimi_err_t ws_server_start(void)
{
    if (s_context) return ESP_OK;

    memset(s_clients, 0, sizeof(s_clients));

    const host_config_t *cfg = host_config_get();

    static struct lws_protocols protocols[] = {
        {
            .name = "mimi-ws",
            .callback = ws_callback,
            .per_session_data_size = 0,
            .rx_buffer_size = 8192,
        },
        { NULL, NULL, 0, 0 }
    };

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = cfg->ws_port;
    info.iface = cfg->ws_bind;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    s_context = lws_create_context(&info);
    if (!s_context) {
        ESP_LOGE(TAG, "Failed to create libwebsockets context");
        return ESP_FAIL;
    }

    s_running = true;
    if (pthread_create(&s_thread, NULL, ws_thread_main, NULL) != 0) {
        lws_context_destroy(s_context);
        s_context = NULL;
        s_running = false;
        return ESP_FAIL;
    }
    s_thread_started = true;

    ESP_LOGI(TAG, "WebSocket server started on %s:%u", cfg->ws_bind, (unsigned)cfg->ws_port);
    return ESP_OK;
}

mimi_err_t ws_server_send(const char *chat_id, const char *text)
{
    if (!s_context) return ESP_ERR_INVALID_STATE;

    pthread_mutex_lock(&s_clients_mu);
    ws_client_t *client = find_client_by_chat_id(chat_id);
    if (!client) {
        pthread_mutex_unlock(&s_clients_mu);
        ESP_LOGW(TAG, "No WS client with chat_id=%s", chat_id);
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "response");
    cJSON_AddStringToObject(resp, "content", text);
    cJSON_AddStringToObject(resp, "chat_id", chat_id);

    char *json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (!json) {
        pthread_mutex_unlock(&s_clients_mu);
        return ESP_ERR_NO_MEM;
    }

    free(client->pending);
    client->pending = json;

    lws_callback_on_writable(client->wsi);
    lws_cancel_service(s_context);

    pthread_mutex_unlock(&s_clients_mu);
    return ESP_OK;
}

mimi_err_t ws_server_stop(void)
{
    s_running = false;

    if (s_context) {
        lws_cancel_service(s_context);
    }

    if (s_thread_started) {
        pthread_join(s_thread, NULL);
        s_thread_started = false;
    }

    pthread_mutex_lock(&s_clients_mu);
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        free(s_clients[i].pending);
        memset(&s_clients[i], 0, sizeof(s_clients[i]));
    }
    pthread_mutex_unlock(&s_clients_mu);

    if (s_context) {
        lws_context_destroy(s_context);
        s_context = NULL;
    }

    ESP_LOGI(TAG, "WebSocket server stopped");
    return ESP_OK;
}
