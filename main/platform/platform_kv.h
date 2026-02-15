#pragma once

#include <stddef.h>
#include <stdint.h>
#include "platform/platform_types.h"

/*
 * Platform key-value storage abstraction.
 * ESP implementation uses NVS.
 * Host implementation uses ~/.mimiclaw/config.json + env overrides + in-memory overrides.
 */
mimi_err_t platform_kv_get_str(const char *ns, const char *key, char *out, size_t out_size);
mimi_err_t platform_kv_set_str(const char *ns, const char *key, const char *value);

mimi_err_t platform_kv_get_u16(const char *ns, const char *key, uint16_t *out);
mimi_err_t platform_kv_set_u16(const char *ns, const char *key, uint16_t value);

mimi_err_t platform_kv_erase_key(const char *ns, const char *key);
