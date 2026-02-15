#include "platform/platform_kv.h"
#include "platform/config_host.h"

mimi_err_t platform_kv_get_str(const char *ns, const char *key, char *out, size_t out_size)
{
    return host_config_get_str(ns, key, out, out_size);
}

mimi_err_t platform_kv_set_str(const char *ns, const char *key, const char *value)
{
    return host_config_set_str(ns, key, value);
}

mimi_err_t platform_kv_get_u16(const char *ns, const char *key, uint16_t *out)
{
    return host_config_get_u16(ns, key, out);
}

mimi_err_t platform_kv_set_u16(const char *ns, const char *key, uint16_t value)
{
    return host_config_set_u16(ns, key, value);
}

mimi_err_t platform_kv_erase_key(const char *ns, const char *key)
{
    return host_config_erase_key(ns, key);
}
