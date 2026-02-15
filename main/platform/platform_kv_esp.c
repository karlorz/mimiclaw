#include "platform/platform_kv.h"

#include <string.h>
#include "nvs.h"

mimi_err_t platform_kv_get_str(const char *ns, const char *key, char *out, size_t out_size)
{
    if (!ns || !key || !out || out_size == 0) return MIMI_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) != ESP_OK) {
        return MIMI_ERR_NOT_FOUND;
    }

    size_t len = out_size;
    esp_err_t err = nvs_get_str(nvs, key, out, &len);
    nvs_close(nvs);
    return (err == ESP_OK) ? MIMI_OK : MIMI_ERR_NOT_FOUND;
}

mimi_err_t platform_kv_set_str(const char *ns, const char *key, const char *value)
{
    if (!ns || !key || !value) return MIMI_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READWRITE, &nvs) != ESP_OK) return MIMI_FAIL;
    esp_err_t err = nvs_set_str(nvs, key, value);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return (err == ESP_OK) ? MIMI_OK : MIMI_FAIL;
}

mimi_err_t platform_kv_get_u16(const char *ns, const char *key, uint16_t *out)
{
    if (!ns || !key || !out) return MIMI_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) != ESP_OK) {
        return MIMI_ERR_NOT_FOUND;
    }

    esp_err_t err = nvs_get_u16(nvs, key, out);
    nvs_close(nvs);
    return (err == ESP_OK) ? MIMI_OK : MIMI_ERR_NOT_FOUND;
}

mimi_err_t platform_kv_set_u16(const char *ns, const char *key, uint16_t value)
{
    if (!ns || !key) return MIMI_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READWRITE, &nvs) != ESP_OK) return MIMI_FAIL;
    esp_err_t err = nvs_set_u16(nvs, key, value);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return (err == ESP_OK) ? MIMI_OK : MIMI_FAIL;
}

mimi_err_t platform_kv_erase_key(const char *ns, const char *key)
{
    if (!ns || !key) return MIMI_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READWRITE, &nvs) != ESP_OK) return MIMI_FAIL;
    esp_err_t err = nvs_erase_key(nvs, key);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return (err == ESP_OK) ? MIMI_OK : MIMI_FAIL;
}
