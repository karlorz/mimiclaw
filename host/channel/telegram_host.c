#include "channel/telegram_host.h"
#include "platform/config_host.h"
#include "platform/platform_http.h"
#include "bus/message_bus.h"
#include "mimi_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>

#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "tg_host";

static pthread_t s_thread;
static bool s_running = false;
static bool s_thread_started = false;
static int64_t s_update_offset = 0;
static char s_bot_token[256] = {0};

static bool allowlist_contains(const char *allowlist_csv, const char *id)
{
    if (!allowlist_csv || !allowlist_csv[0] || !id || !id[0]) {
        return false;
    }

    char buf[768];
    snprintf(buf, sizeof(buf), "%s", allowlist_csv);

    char *saveptr = NULL;
    for (char *tok = strtok_r(buf, ",;", &saveptr);
         tok;
         tok = strtok_r(NULL, ",;", &saveptr)) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }

        if (tok[0] && strcmp(tok, id) == 0) {
            return true;
        }
    }

    return false;
}

static bool telegram_is_allowed(const char *chat_id, const char *user_id)
{
    const host_config_t *cfg = host_config_get();
    if (!cfg) return false;

    if (!cfg->telegram_allowlist[0]) {
        return true;
    }

    if (allowlist_contains(cfg->telegram_allowlist, chat_id)) return true;
    if (allowlist_contains(cfg->telegram_allowlist, user_id)) return true;
    return false;
}

static void extract_id(cJSON *obj, const char *key, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';

    if (!obj || !key) return;

    cJSON *v = cJSON_GetObjectItem(obj, key);
    if (!v) return;

    if (cJSON_IsString(v) && v->valuestring) {
        snprintf(out, out_size, "%s", v->valuestring);
        return;
    }

    if (cJSON_IsNumber(v)) {
        snprintf(out, out_size, "%.0f", v->valuedouble);
    }
}

static mimi_err_t tg_api_get(const char *method_and_query,
                             char *response_buf,
                             size_t response_buf_size,
                             int *status_code)
{
    if (!method_and_query || !response_buf || response_buf_size == 0 || !s_bot_token[0]) {
        return MIMI_ERR_INVALID_ARG;
    }

    char url[1024];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/%s", s_bot_token, method_and_query);

    return platform_http_get(url, NULL, 0,
                             (MIMI_TG_POLL_TIMEOUT_S + 5) * 1000,
                             response_buf, response_buf_size,
                             status_code);
}

static mimi_err_t tg_api_post(const char *method,
                              const char *body,
                              char *response_buf,
                              size_t response_buf_size,
                              int *status_code)
{
    if (!method || !body || !response_buf || response_buf_size == 0 || !s_bot_token[0]) {
        return MIMI_ERR_INVALID_ARG;
    }

    char url[1024];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/%s", s_bot_token, method);

    platform_http_header_t headers[] = {
        { .name = "Content-Type", .value = "application/json" },
    };

    return platform_http_post_json(url, headers, sizeof(headers) / sizeof(headers[0]),
                                   body,
                                   (MIMI_TG_POLL_TIMEOUT_S + 5) * 1000,
                                   response_buf,
                                   response_buf_size,
                                   status_code);
}

static bool response_ok_json(const char *json_text)
{
    if (!json_text || !json_text[0]) return false;

    bool ok = false;
    cJSON *root = cJSON_Parse(json_text);
    if (root) {
        cJSON *ok_field = cJSON_GetObjectItem(root, "ok");
        ok = cJSON_IsTrue(ok_field);
    }
    cJSON_Delete(root);
    return ok;
}

static mimi_err_t telegram_send_chunk(const char *chat_id, const char *text, bool markdown)
{
    char response[16384];
    response[0] = '\0';

    cJSON *body = cJSON_CreateObject();
    if (!body) return MIMI_ERR_NO_MEM;

    cJSON_AddStringToObject(body, "chat_id", chat_id);
    cJSON_AddStringToObject(body, "text", text);
    if (markdown) {
        cJSON_AddStringToObject(body, "parse_mode", "Markdown");
    }

    char *json = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    if (!json) return MIMI_ERR_NO_MEM;

    int status = 0;
    mimi_err_t err = tg_api_post("sendMessage", json, response, sizeof(response), &status);
    free(json);

    if (err != MIMI_OK) {
        ESP_LOGW(TAG, "telegram send failed: err=%s", mimi_err_to_name(err));
        return err;
    }

    if (status < 200 || status >= 300 || !response_ok_json(response)) {
        ESP_LOGW(TAG, "telegram send rejected: status=%d", status);
        return MIMI_FAIL;
    }

    return MIMI_OK;
}

static void process_updates(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");

    if (!cJSON_IsTrue(ok) || !cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *update;
    cJSON_ArrayForEach(update, result) {
        cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
        if (cJSON_IsNumber(update_id)) {
            int64_t uid = (int64_t)update_id->valuedouble;
            if (uid >= s_update_offset) {
                s_update_offset = uid + 1;
            }
        }

        cJSON *message = cJSON_GetObjectItem(update, "message");
        if (!message || !cJSON_IsObject(message)) continue;

        cJSON *text = cJSON_GetObjectItem(message, "text");
        if (!text || !cJSON_IsString(text) || !text->valuestring[0]) continue;

        char chat_id[32] = {0};
        char user_id[32] = {0};

        cJSON *chat = cJSON_GetObjectItem(message, "chat");
        if (chat && cJSON_IsObject(chat)) {
            extract_id(chat, "id", chat_id, sizeof(chat_id));
        }

        cJSON *from = cJSON_GetObjectItem(message, "from");
        if (from && cJSON_IsObject(from)) {
            extract_id(from, "id", user_id, sizeof(user_id));
        }

        if (!chat_id[0]) continue;

        if (!telegram_is_allowed(chat_id, user_id)) {
            ESP_LOGW(TAG, "allowlist-deny chat_id=%s user_id=%s", chat_id, user_id[0] ? user_id : "<none>");
            continue;
        }

        ESP_LOGI(TAG, "telegram inbound chat_id=%s", chat_id);

        mimi_msg_t msg = {0};
        snprintf(msg.channel, sizeof(msg.channel), "%s", MIMI_CHAN_TELEGRAM);
        snprintf(msg.chat_id, sizeof(msg.chat_id), "%s", chat_id);
        msg.content = strdup(text->valuestring);
        if (!msg.content) {
            ESP_LOGW(TAG, "Dropping message: out of memory");
            continue;
        }

        mimi_err_t err = message_bus_push_inbound(&msg);
        if (err != MIMI_OK) {
            ESP_LOGW(TAG, "Failed to enqueue telegram inbound: %s", mimi_err_to_name(err));
            free(msg.content);
        }
    }

    cJSON_Delete(root);
}

static void *telegram_poll_thread(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Telegram poller started");

    while (s_running) {
        char method[256];
        snprintf(method, sizeof(method), "getUpdates?offset=%" PRId64 "&timeout=%d",
                 s_update_offset, MIMI_TG_POLL_TIMEOUT_S);

        char response[32768];
        response[0] = '\0';

        int status = 0;
        mimi_err_t err = tg_api_get(method, response, sizeof(response), &status);
        if (err == MIMI_OK && status >= 200 && status < 300) {
            process_updates(response);
            continue;
        }

        if (!s_running) {
            break;
        }

        ESP_LOGW(TAG, "telegram polling error: err=%s status=%d", mimi_err_to_name(err), status);
        usleep(300 * 1000);
    }

    ESP_LOGI(TAG, "Telegram poller stopped");
    return NULL;
}

mimi_err_t telegram_host_init(void)
{
    const host_config_t *cfg = host_config_get();
    if (!cfg) return MIMI_ERR_INVALID_STATE;

    s_update_offset = 0;
    snprintf(s_bot_token, sizeof(s_bot_token), "%s", cfg->tg_token);

    if (cfg->channel_telegram_enabled && !s_bot_token[0]) {
        ESP_LOGW(TAG, "Telegram channel enabled but bot token is empty");
    }

    if (s_bot_token[0]) {
        ESP_LOGI(TAG, "Telegram bot token loaded (len=%d)", (int)strlen(s_bot_token));
    }

    return MIMI_OK;
}

mimi_err_t telegram_host_start(void)
{
    const host_config_t *cfg = host_config_get();
    if (!cfg) return MIMI_ERR_INVALID_STATE;

    if (!cfg->channel_telegram_enabled) {
        ESP_LOGI(TAG, "Telegram channel disabled in config");
        return MIMI_OK;
    }

    if (!s_bot_token[0]) {
        ESP_LOGW(TAG, "Telegram channel enabled but token missing; not starting poller");
        return MIMI_OK;
    }

    if (s_thread_started) {
        return MIMI_OK;
    }

    s_running = true;
    if (pthread_create(&s_thread, NULL, telegram_poll_thread, NULL) != 0) {
        s_running = false;
        ESP_LOGE(TAG, "Failed to start Telegram poll thread");
        return MIMI_FAIL;
    }

    s_thread_started = true;
    ESP_LOGI(TAG, "Telegram channel started");
    return MIMI_OK;
}

mimi_err_t telegram_host_send(const char *chat_id, const char *text)
{
    if (!chat_id || !text || !chat_id[0]) return MIMI_ERR_INVALID_ARG;

    const host_config_t *cfg = host_config_get();
    if (!cfg || !cfg->channel_telegram_enabled) {
        return MIMI_ERR_INVALID_STATE;
    }

    if (!s_bot_token[0]) {
        return MIMI_ERR_INVALID_STATE;
    }

    size_t text_len = strlen(text);
    size_t offset = 0;

    while (offset < text_len) {
        size_t chunk_len = text_len - offset;
        if (chunk_len > MIMI_TG_MAX_MSG_LEN) {
            chunk_len = MIMI_TG_MAX_MSG_LEN;
        }

        char *segment = calloc(1, chunk_len + 1);
        if (!segment) return MIMI_ERR_NO_MEM;
        memcpy(segment, text + offset, chunk_len);

        mimi_err_t err = telegram_send_chunk(chat_id, segment, true);
        if (err != MIMI_OK) {
            err = telegram_send_chunk(chat_id, segment, false);
        }

        free(segment);

        if (err != MIMI_OK) {
            return err;
        }

        offset += chunk_len;
    }

    return MIMI_OK;
}

mimi_err_t telegram_host_stop(void)
{
    s_running = false;

    if (s_thread_started) {
        pthread_join(s_thread, NULL);
        s_thread_started = false;
    }

    return MIMI_OK;
}
