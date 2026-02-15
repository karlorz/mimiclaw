#pragma once

#include <stddef.h>
#include <stdint.h>
#include "platform/platform_types.h"

typedef struct {
    char config_path[1024];
    char api_key[256];
    char model[128];
    char model_provider[32];
    char search_key[256];
    char ws_bind[64];
    uint16_t ws_port;
    char state_root[1024];
    char timezone[128];
    char proxy_host[128];
    uint16_t proxy_port;
} host_config_t;

mimi_err_t host_config_load(int argc, char **argv);
const host_config_t *host_config_get(void);

mimi_err_t host_config_get_str(const char *ns, const char *key, char *out, size_t out_size);
mimi_err_t host_config_set_str(const char *ns, const char *key, const char *value);

mimi_err_t host_config_get_u16(const char *ns, const char *key, uint16_t *out);
mimi_err_t host_config_set_u16(const char *ns, const char *key, uint16_t value);

mimi_err_t host_config_erase_key(const char *ns, const char *key);
