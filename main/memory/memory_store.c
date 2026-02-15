#include "memory_store.h"
#include "mimi_config.h"
#include "platform/platform_paths.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "memory";

static void get_date_str(char *buf, size_t size, int days_ago)
{
    time_t now;
    time(&now);
    now -= days_ago * 86400;
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, size, "%Y-%m-%d", &tm);
}

static FILE *open_virtual(const char *virtual_path, const char *mode)
{
    char real_path[1024];
    if (platform_path_to_real(virtual_path, real_path, sizeof(real_path)) != MIMI_OK) {
        return NULL;
    }
    return fopen(real_path, mode);
}

esp_err_t memory_store_init(void)
{
    ESP_ERROR_CHECK(platform_paths_init(NULL));
    ESP_LOGI(TAG, "Memory store initialized at %s", platform_paths_state_root());
    return ESP_OK;
}

esp_err_t memory_read_long_term(char *buf, size_t size)
{
    FILE *f = open_virtual(MIMI_MEMORY_FILE, "r");
    if (!f) {
        buf[0] = '\0';
        return ESP_ERR_NOT_FOUND;
    }

    size_t n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
    fclose(f);
    return ESP_OK;
}

esp_err_t memory_write_long_term(const char *content)
{
    FILE *f = open_virtual(MIMI_MEMORY_FILE, "w");
    if (!f) {
        ESP_LOGE(TAG, "Cannot write %s", MIMI_MEMORY_FILE);
        return ESP_FAIL;
    }
    fputs(content, f);
    fclose(f);
    ESP_LOGI(TAG, "Long-term memory updated (%d bytes)", (int)strlen(content));
    return ESP_OK;
}

esp_err_t memory_append_today(const char *note)
{
    char date_str[16];
    get_date_str(date_str, sizeof(date_str), 0);

    char virtual_path[128];
    snprintf(virtual_path, sizeof(virtual_path), "%s/%s.md", MIMI_SPIFFS_MEMORY_DIR, date_str);

    FILE *f = open_virtual(virtual_path, "a");
    if (!f) {
        f = open_virtual(virtual_path, "w");
        if (!f) {
            ESP_LOGE(TAG, "Cannot open %s", virtual_path);
            return ESP_FAIL;
        }
        fprintf(f, "# %s\n\n", date_str);
    }

    fprintf(f, "%s\n", note);
    fclose(f);
    return ESP_OK;
}

esp_err_t memory_read_recent(char *buf, size_t size, int days)
{
    size_t offset = 0;
    buf[0] = '\0';

    for (int i = 0; i < days && offset < size - 1; i++) {
        char date_str[16];
        get_date_str(date_str, sizeof(date_str), i);

        char virtual_path[128];
        snprintf(virtual_path, sizeof(virtual_path), "%s/%s.md", MIMI_SPIFFS_MEMORY_DIR, date_str);

        FILE *f = open_virtual(virtual_path, "r");
        if (!f) continue;

        if (offset > 0 && offset < size - 4) {
            offset += snprintf(buf + offset, size - offset, "\n---\n");
        }

        size_t n = fread(buf + offset, 1, size - offset - 1, f);
        offset += n;
        buf[offset] = '\0';
        fclose(f);
    }

    return ESP_OK;
}
