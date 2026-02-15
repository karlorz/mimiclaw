#include "tools/tool_files.h"
#include "mimi_config.h"
#include "platform/platform_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "tool_files";

#define MAX_FILE_SIZE (32 * 1024)

/* ── read_file ─────────────────────────────────────────────── */

esp_err_t tool_read_file_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    if (!platform_path_is_valid_virtual(path)) {
        snprintf(output, output_size, "Error: path must start with /spiffs/ and must not contain '..'");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    char real_path[1024];
    if (platform_path_to_real(path, real_path, sizeof(real_path)) != MIMI_OK) {
        snprintf(output, output_size, "Error: path not allowed: %s", path);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(real_path, "r");
    if (!f) {
        snprintf(output, output_size, "Error: file not found: %s", path);
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    size_t max_read = output_size - 1;
    if (max_read > MAX_FILE_SIZE) max_read = MAX_FILE_SIZE;

    size_t n = fread(output, 1, max_read, f);
    output[n] = '\0';
    fclose(f);

    ESP_LOGI(TAG, "read_file: %s (%d bytes)", path, (int)n);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── write_file ────────────────────────────────────────────── */

esp_err_t tool_write_file_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    const char *content = cJSON_GetStringValue(cJSON_GetObjectItem(root, "content"));

    if (!platform_path_is_valid_virtual(path)) {
        snprintf(output, output_size, "Error: path must start with /spiffs/ and must not contain '..'");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (!content) {
        snprintf(output, output_size, "Error: missing 'content' field");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    char real_path[1024];
    if (platform_path_to_real(path, real_path, sizeof(real_path)) != MIMI_OK) {
        snprintf(output, output_size, "Error: path not allowed: %s", path);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(real_path, "w");
    if (!f) {
        snprintf(output, output_size, "Error: cannot open file for writing: %s", path);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);

    if (written != len) {
        snprintf(output, output_size, "Error: wrote %d of %d bytes to %s", (int)written, (int)len, path);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    snprintf(output, output_size, "OK: wrote %d bytes to %s", (int)written, path);
    ESP_LOGI(TAG, "write_file: %s (%d bytes)", path, (int)written);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── edit_file ─────────────────────────────────────────────── */

esp_err_t tool_edit_file_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    const char *old_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "old_string"));
    const char *new_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "new_string"));

    if (!platform_path_is_valid_virtual(path)) {
        snprintf(output, output_size, "Error: path must start with /spiffs/ and must not contain '..'");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (!old_str || !new_str) {
        snprintf(output, output_size, "Error: missing 'old_string' or 'new_string' field");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    char real_path[1024];
    if (platform_path_to_real(path, real_path, sizeof(real_path)) != MIMI_OK) {
        snprintf(output, output_size, "Error: path not allowed: %s", path);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(real_path, "r");
    if (!f) {
        snprintf(output, output_size, "Error: file not found: %s", path);
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > MAX_FILE_SIZE) {
        snprintf(output, output_size, "Error: file too large or empty (%ld bytes)", file_size);
        fclose(f);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    size_t max_result = (size_t)file_size + (new_len > old_len ? new_len - old_len : 0) + 1;
    char *buf = malloc((size_t)file_size + 1);
    char *result = malloc(max_result);
    if (!buf || !result) {
        free(buf);
        free(result);
        fclose(f);
        snprintf(output, output_size, "Error: out of memory");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    size_t n = fread(buf, 1, (size_t)file_size, f);
    buf[n] = '\0';
    fclose(f);

    char *pos = strstr(buf, old_str);
    if (!pos) {
        snprintf(output, output_size, "Error: old_string not found in %s", path);
        free(buf);
        free(result);
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    size_t prefix_len = (size_t)(pos - buf);
    memcpy(result, buf, prefix_len);
    memcpy(result + prefix_len, new_str, new_len);
    size_t suffix_start = prefix_len + old_len;
    size_t suffix_len = n - suffix_start;
    memcpy(result + prefix_len + new_len, buf + suffix_start, suffix_len);
    size_t total = prefix_len + new_len + suffix_len;
    result[total] = '\0';

    free(buf);

    f = fopen(real_path, "w");
    if (!f) {
        snprintf(output, output_size, "Error: cannot open file for writing: %s", path);
        free(result);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    fwrite(result, 1, total, f);
    fclose(f);
    free(result);

    snprintf(output, output_size, "OK: edited %s (replaced %d bytes with %d bytes)", path, (int)old_len, (int)new_len);
    ESP_LOGI(TAG, "edit_file: %s", path);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── list_dir ──────────────────────────────────────────────── */

esp_err_t tool_list_dir_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    const char *prefix = NULL;
    if (root) {
        cJSON *pfx = cJSON_GetObjectItem(root, "prefix");
        if (pfx && cJSON_IsString(pfx)) {
            prefix = pfx->valuestring;
        }
    }

    if (prefix && !platform_path_is_valid_virtual(prefix)) {
        snprintf(output, output_size, "Error: prefix must start with /spiffs/ and must not contain '..'");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = platform_paths_list_virtual(prefix, output, output_size);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: cannot list directory");
        cJSON_Delete(root);
        return err;
    }

    if (output[0] == '\0') {
        snprintf(output, output_size, "(no files found)");
    }

    ESP_LOGI(TAG, "list_dir: prefix=%s", prefix ? prefix : "(none)");
    cJSON_Delete(root);
    return ESP_OK;
}
