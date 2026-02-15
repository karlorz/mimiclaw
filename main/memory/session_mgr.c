#include "session_mgr.h"
#include "mimi_config.h"
#include "platform/platform_paths.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "session";

static void session_path(const char *chat_id, char *buf, size_t size)
{
    snprintf(buf, size, "%s/tg_%s.jsonl", MIMI_SPIFFS_SESSION_DIR, chat_id);
}

static FILE *open_virtual(const char *virtual_path, const char *mode)
{
    char real_path[1024];
    if (platform_path_to_real(virtual_path, real_path, sizeof(real_path)) != MIMI_OK) {
        return NULL;
    }
    return fopen(real_path, mode);
}

esp_err_t session_mgr_init(void)
{
    ESP_LOGI(TAG, "Session manager initialized at %s", MIMI_SPIFFS_SESSION_DIR);
    return ESP_OK;
}

esp_err_t session_append(const char *chat_id, const char *role, const char *content)
{
    char virtual_path[128];
    session_path(chat_id, virtual_path, sizeof(virtual_path));

    FILE *f = open_virtual(virtual_path, "a");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open session file %s", virtual_path);
        return ESP_FAIL;
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "role", role);
    cJSON_AddStringToObject(obj, "content", content);
    cJSON_AddNumberToObject(obj, "ts", (double)time(NULL));

    char *line = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    if (line) {
        fprintf(f, "%s\n", line);
        free(line);
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs)
{
    char virtual_path[128];
    session_path(chat_id, virtual_path, sizeof(virtual_path));

    FILE *f = open_virtual(virtual_path, "r");
    if (!f) {
        snprintf(buf, size, "[]");
        return ESP_OK;
    }

    cJSON *messages[MIMI_SESSION_MAX_MSGS];
    int count = 0;
    int write_idx = 0;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] == '\0') continue;

        cJSON *obj = cJSON_Parse(line);
        if (!obj) continue;

        if (count >= max_msgs) {
            cJSON_Delete(messages[write_idx]);
        }
        messages[write_idx] = obj;
        write_idx = (write_idx + 1) % max_msgs;
        if (count < max_msgs) count++;
    }
    fclose(f);

    cJSON *arr = cJSON_CreateArray();
    int start = (count < max_msgs) ? 0 : write_idx;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % max_msgs;
        cJSON *src = messages[idx];

        cJSON *entry = cJSON_CreateObject();
        cJSON *role = cJSON_GetObjectItem(src, "role");
        cJSON *content = cJSON_GetObjectItem(src, "content");
        if (role && content && cJSON_IsString(role) && cJSON_IsString(content)) {
            cJSON_AddStringToObject(entry, "role", role->valuestring);
            cJSON_AddStringToObject(entry, "content", content->valuestring);
        }
        cJSON_AddItemToArray(arr, entry);
    }

    int cleanup_start = (count < max_msgs) ? 0 : write_idx;
    for (int i = 0; i < count; i++) {
        int idx = (cleanup_start + i) % max_msgs;
        cJSON_Delete(messages[idx]);
    }

    char *json_str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (json_str) {
        strncpy(buf, json_str, size - 1);
        buf[size - 1] = '\0';
        free(json_str);
    } else {
        snprintf(buf, size, "[]");
    }

    return ESP_OK;
}

esp_err_t session_clear(const char *chat_id)
{
    char virtual_path[128];
    session_path(chat_id, virtual_path, sizeof(virtual_path));

    char real_path[1024];
    if (platform_path_to_real(virtual_path, real_path, sizeof(real_path)) != MIMI_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    if (remove(real_path) == 0) {
        ESP_LOGI(TAG, "Session %s cleared", chat_id);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

void session_list(void)
{
    char listing[8192] = {0};
    if (platform_paths_list_virtual(MIMI_SPIFFS_SESSION_DIR "/", listing, sizeof(listing)) != MIMI_OK) {
        ESP_LOGW(TAG, "Cannot list sessions");
        return;
    }

    int count = 0;
    char *saveptr = NULL;
    char *line = strtok_r(listing, "\n", &saveptr);
    while (line) {
        if (strstr(line, "/tg_") && strstr(line, ".jsonl")) {
            ESP_LOGI(TAG, "  Session: %s", line);
            count++;
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }

    if (count == 0) {
        ESP_LOGI(TAG, "  No sessions found");
    }
}
