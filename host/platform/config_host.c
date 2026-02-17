#include "platform/config_host.h"
#include "mimi_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "host_config";

static host_config_t s_cfg;

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static void expand_home(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) return;

    if (in[0] == '~' && in[1] == '/') {
        const char *home = getenv("HOME");
        if (home && home[0]) {
            snprintf(out, out_size, "%s/%s", home, in + 2);
            return;
        }
    }

    snprintf(out, out_size, "%s", in);
}

static char *trim_left(char *s)
{
    while (s && *s && isspace((unsigned char)*s)) s++;
    return s;
}

static void trim_right(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static void trim_inplace(char *s)
{
    if (!s) return;
    char *start = trim_left(s);
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    trim_right(s);
}

static void strip_matching_quotes(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    if (len < 2) return;
    if ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\'')) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static bool parse_bool_str(const char *s, bool *out)
{
    if (!s || !out) return false;

    if (strcasecmp(s, "1") == 0 ||
        strcasecmp(s, "true") == 0 ||
        strcasecmp(s, "yes") == 0 ||
        strcasecmp(s, "on") == 0) {
        *out = true;
        return true;
    }

    if (strcasecmp(s, "0") == 0 ||
        strcasecmp(s, "false") == 0 ||
        strcasecmp(s, "no") == 0 ||
        strcasecmp(s, "off") == 0) {
        *out = false;
        return true;
    }

    return false;
}

static bool json_read_bool(cJSON *obj, const char *key, bool *out)
{
    if (!obj || !key || !out) return false;

    cJSON *v = cJSON_GetObjectItem(obj, key);
    if (!v) return false;

    if (cJSON_IsBool(v)) {
        *out = cJSON_IsTrue(v);
        return true;
    }

    if (cJSON_IsNumber(v)) {
        *out = (v->valuedouble != 0.0);
        return true;
    }

    if (cJSON_IsString(v) && v->valuestring) {
        return parse_bool_str(v->valuestring, out);
    }

    return false;
}

static void load_dotenv_file(const char *path)
{
    if (!path || path[0] == '\0') return;

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim_left(line);
        if (!p || p[0] == '\0' || p[0] == '#') continue;

        if (strncmp(p, "export ", 7) == 0) {
            p += 7;
            p = trim_left(p);
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = p;
        char *value = eq + 1;

        trim_inplace(key);
        trim_inplace(value);

        if (!key[0]) continue;

        strip_matching_quotes(value);

        if (!getenv(key)) {
            setenv(key, value, 0);
        }
    }

    fclose(f);
}

static const char *first_nonempty_env(const char *first, const char *second)
{
    const char *v = getenv(first);
    if (v && v[0]) return v;
    v = getenv(second);
    return (v && v[0]) ? v : NULL;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > (1024 * 1024)) {
        fclose(f);
        return NULL;
    }

    char *buf = calloc(1, (size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static const char *json_get_str(cJSON *obj, const char *key)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int json_get_int(cJSON *obj, const char *key, int default_value)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return (v && cJSON_IsNumber(v)) ? v->valueint : default_value;
}

static void clear_skills_list(void)
{
    s_cfg.skills_count = 0;
    memset(s_cfg.skills_list, 0, sizeof(s_cfg.skills_list));
}

static void add_skill_entry(const char *entry)
{
    if (!entry || !entry[0]) return;
    if (s_cfg.skills_count >= HOST_CONFIG_MAX_SKILLS) {
        ESP_LOGW(TAG, "skills list capacity reached (%d), dropping extra entry", HOST_CONFIG_MAX_SKILLS);
        return;
    }

    char tmp[HOST_CONFIG_MAX_SKILL_PATH];
    safe_copy(tmp, sizeof(tmp), entry);
    trim_inplace(tmp);
    if (!tmp[0]) return;

    safe_copy(s_cfg.skills_list[s_cfg.skills_count],
              sizeof(s_cfg.skills_list[s_cfg.skills_count]),
              tmp);
    s_cfg.skills_count++;
}

static void parse_skills_csv(const char *csv)
{
    clear_skills_list();
    if (!csv || !csv[0]) return;

    char buf[2048];
    safe_copy(buf, sizeof(buf), csv);

    char *saveptr = NULL;
    for (char *tok = strtok_r(buf, ",;", &saveptr);
         tok;
         tok = strtok_r(NULL, ",;", &saveptr)) {
        trim_inplace(tok);
        add_skill_entry(tok);
    }
}

static void parse_skills_json(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!item) return;

    if (cJSON_IsString(item)) {
        parse_skills_csv(item->valuestring);
        return;
    }

    if (!cJSON_IsArray(item)) return;

    clear_skills_list();
    cJSON *entry;
    cJSON_ArrayForEach(entry, item) {
        if (cJSON_IsString(entry) && entry->valuestring) {
            add_skill_entry(entry->valuestring);
        }
    }
}

static void parse_allowlist_json(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!item) return;

    if (cJSON_IsString(item) && item->valuestring) {
        safe_copy(s_cfg.telegram_allowlist, sizeof(s_cfg.telegram_allowlist), item->valuestring);
        return;
    }

    if (!cJSON_IsArray(item)) return;

    size_t off = 0;
    s_cfg.telegram_allowlist[0] = '\0';

    cJSON *entry;
    cJSON_ArrayForEach(entry, item) {
        char token[64] = {0};

        if (cJSON_IsString(entry) && entry->valuestring) {
            safe_copy(token, sizeof(token), entry->valuestring);
        } else if (cJSON_IsNumber(entry)) {
            snprintf(token, sizeof(token), "%.0f", entry->valuedouble);
        } else {
            continue;
        }

        trim_inplace(token);
        if (!token[0]) continue;

        if (off > 0 && off < sizeof(s_cfg.telegram_allowlist) - 1) {
            s_cfg.telegram_allowlist[off++] = ',';
        }

        if (off >= sizeof(s_cfg.telegram_allowlist) - 1) break;

        size_t remain = sizeof(s_cfg.telegram_allowlist) - off;
        size_t need = strlen(token);
        if (need >= remain) need = remain - 1;

        memcpy(s_cfg.telegram_allowlist + off, token, need);
        off += need;
        s_cfg.telegram_allowlist[off] = '\0';
    }
}

static void apply_json_config(cJSON *root)
{
    const char *v;
    bool b;

    v = json_get_str(root, "api_key");
    if (v) safe_copy(s_cfg.api_key, sizeof(s_cfg.api_key), v);
    v = json_get_str(root, "model");
    if (v) safe_copy(s_cfg.model, sizeof(s_cfg.model), v);
    v = json_get_str(root, "model_provider");
    if (v) safe_copy(s_cfg.model_provider, sizeof(s_cfg.model_provider), v);
    v = json_get_str(root, "api_base");
    if (v) safe_copy(s_cfg.api_base, sizeof(s_cfg.api_base), v);
    v = json_get_str(root, "search_key");
    if (v) safe_copy(s_cfg.search_key, sizeof(s_cfg.search_key), v);
    v = json_get_str(root, "ws_bind");
    if (v) safe_copy(s_cfg.ws_bind, sizeof(s_cfg.ws_bind), v);
    s_cfg.ws_port = (uint16_t)json_get_int(root, "ws_port", s_cfg.ws_port);
    v = json_get_str(root, "timezone");
    if (v) safe_copy(s_cfg.timezone, sizeof(s_cfg.timezone), v);

    v = json_get_str(root, "state_root");
    if (v) {
        char expanded[1024];
        expand_home(v, expanded, sizeof(expanded));
        safe_copy(s_cfg.state_root, sizeof(s_cfg.state_root), expanded);
    }

    v = json_get_str(root, "proxy_host");
    if (v) safe_copy(s_cfg.proxy_host, sizeof(s_cfg.proxy_host), v);
    s_cfg.proxy_port = (uint16_t)json_get_int(root, "proxy_port", s_cfg.proxy_port);

    v = json_get_str(root, "tg_token");
    if (!v) v = json_get_str(root, "telegram_token");
    if (v) safe_copy(s_cfg.tg_token, sizeof(s_cfg.tg_token), v);

    if (json_read_bool(root, "telegram_enabled", &b)) {
        s_cfg.channel_telegram_enabled = b;
    }

    if (json_read_bool(root, "ws_require_token", &b)) {
        s_cfg.ws_require_token = b;
    }

    v = json_get_str(root, "ws_token");
    if (v) safe_copy(s_cfg.ws_token, sizeof(s_cfg.ws_token), v);

    parse_allowlist_json(root, "telegram_allowlist");

    if (json_read_bool(root, "skills_enabled", &b)) {
        s_cfg.skills_enabled = b;
    }

    v = json_get_str(root, "skills_dir");
    if (v) {
        char expanded[1024];
        expand_home(v, expanded, sizeof(expanded));
        safe_copy(s_cfg.skills_dir, sizeof(s_cfg.skills_dir), expanded);
    }

    int parsed_max = json_get_int(root, "skills_max_loaded", s_cfg.skills_max_loaded);
    if (parsed_max > 0) {
        if (parsed_max > HOST_CONFIG_MAX_SKILLS) parsed_max = HOST_CONFIG_MAX_SKILLS;
        s_cfg.skills_max_loaded = (uint16_t)parsed_max;
    }

    parse_skills_json(root, "skills_list");

    cJSON *llm = cJSON_GetObjectItem(root, "llm");
    if (llm && cJSON_IsObject(llm)) {
        v = json_get_str(llm, "api_key");
        if (v) safe_copy(s_cfg.api_key, sizeof(s_cfg.api_key), v);
        v = json_get_str(llm, "model");
        if (v) safe_copy(s_cfg.model, sizeof(s_cfg.model), v);
        v = json_get_str(llm, "provider");
        if (v) safe_copy(s_cfg.model_provider, sizeof(s_cfg.model_provider), v);
        v = json_get_str(llm, "api_base");
        if (v) safe_copy(s_cfg.api_base, sizeof(s_cfg.api_base), v);
        v = json_get_str(llm, "base_url");
        if (v) safe_copy(s_cfg.api_base, sizeof(s_cfg.api_base), v);
    }

    cJSON *search = cJSON_GetObjectItem(root, "search");
    if (search && cJSON_IsObject(search)) {
        v = json_get_str(search, "api_key");
        if (v) safe_copy(s_cfg.search_key, sizeof(s_cfg.search_key), v);
    }

    cJSON *ws = cJSON_GetObjectItem(root, "ws");
    if (ws && cJSON_IsObject(ws)) {
        v = json_get_str(ws, "bind");
        if (v) safe_copy(s_cfg.ws_bind, sizeof(s_cfg.ws_bind), v);
        s_cfg.ws_port = (uint16_t)json_get_int(ws, "port", s_cfg.ws_port);
    }

    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (state && cJSON_IsObject(state)) {
        v = json_get_str(state, "root");
        if (v) {
            char expanded[1024];
            expand_home(v, expanded, sizeof(expanded));
            safe_copy(s_cfg.state_root, sizeof(s_cfg.state_root), expanded);
        }
    }

    cJSON *proxy = cJSON_GetObjectItem(root, "proxy");
    if (proxy && cJSON_IsObject(proxy)) {
        v = json_get_str(proxy, "host");
        if (v) safe_copy(s_cfg.proxy_host, sizeof(s_cfg.proxy_host), v);
        s_cfg.proxy_port = (uint16_t)json_get_int(proxy, "port", s_cfg.proxy_port);
    }

    cJSON *channels = cJSON_GetObjectItem(root, "channels");
    if (channels && cJSON_IsObject(channels)) {
        if (json_read_bool(channels, "telegram_enabled", &b)) {
            s_cfg.channel_telegram_enabled = b;
        }
    }

    cJSON *telegram = cJSON_GetObjectItem(root, "telegram");
    if (telegram && cJSON_IsObject(telegram)) {
        if (json_read_bool(telegram, "enabled", &b)) {
            s_cfg.channel_telegram_enabled = b;
        }
        v = json_get_str(telegram, "bot_token");
        if (v) safe_copy(s_cfg.tg_token, sizeof(s_cfg.tg_token), v);
    }

    cJSON *security = cJSON_GetObjectItem(root, "security");
    if (security && cJSON_IsObject(security)) {
        if (json_read_bool(security, "ws_require_token", &b)) {
            s_cfg.ws_require_token = b;
        }
        v = json_get_str(security, "ws_token");
        if (v) safe_copy(s_cfg.ws_token, sizeof(s_cfg.ws_token), v);
        parse_allowlist_json(security, "telegram_allowlist");
    }

    cJSON *skills = cJSON_GetObjectItem(root, "skills");
    if (skills && cJSON_IsObject(skills)) {
        if (json_read_bool(skills, "enabled", &b)) {
            s_cfg.skills_enabled = b;
        }

        v = json_get_str(skills, "dir");
        if (v) {
            char expanded[1024];
            expand_home(v, expanded, sizeof(expanded));
            safe_copy(s_cfg.skills_dir, sizeof(s_cfg.skills_dir), expanded);
        }

        parsed_max = json_get_int(skills, "max_loaded", s_cfg.skills_max_loaded);
        if (parsed_max > 0) {
            if (parsed_max > HOST_CONFIG_MAX_SKILLS) parsed_max = HOST_CONFIG_MAX_SKILLS;
            s_cfg.skills_max_loaded = (uint16_t)parsed_max;
        }

        parse_skills_json(skills, "list");
    }
}

static void apply_env_overrides(void)
{
    const char *v = first_nonempty_env("MIMI_API_KEY", "AI_API_KEY");
    if (v && v[0]) safe_copy(s_cfg.api_key, sizeof(s_cfg.api_key), v);

    const char *model_env = first_nonempty_env("MIMI_MODEL", "AI_MODEL");
    v = model_env;
    if (v && v[0]) safe_copy(s_cfg.model, sizeof(s_cfg.model), v);

    const char *provider_env = first_nonempty_env("MIMI_MODEL_PROVIDER", "AI_PROVIDER");
    v = provider_env;
    if (v && v[0]) safe_copy(s_cfg.model_provider, sizeof(s_cfg.model_provider), v);

    v = getenv("MIMI_SEARCH_KEY");
    if (v && v[0]) safe_copy(s_cfg.search_key, sizeof(s_cfg.search_key), v);

    const char *api_base_env = first_nonempty_env("MIMI_API_BASE", "AI_API_BASE");
    if (api_base_env && api_base_env[0]) {
        safe_copy(s_cfg.api_base, sizeof(s_cfg.api_base), api_base_env);
        if (!provider_env || !provider_env[0]) {
            safe_copy(s_cfg.model_provider, sizeof(s_cfg.model_provider), "openai");
        }
    }

    if ((!provider_env || !provider_env[0]) &&
        model_env && strncasecmp(model_env, "openai/", strlen("openai/")) == 0) {
        safe_copy(s_cfg.model_provider, sizeof(s_cfg.model_provider), "openai");
    }

    v = getenv("MIMI_WS_BIND");
    if (v && v[0]) safe_copy(s_cfg.ws_bind, sizeof(s_cfg.ws_bind), v);

    v = getenv("MIMI_WS_PORT");
    if (v && v[0]) s_cfg.ws_port = (uint16_t)atoi(v);

    v = getenv("MIMI_STATE_ROOT");
    if (v && v[0]) {
        char expanded[1024];
        expand_home(v, expanded, sizeof(expanded));
        safe_copy(s_cfg.state_root, sizeof(s_cfg.state_root), expanded);
    }

    v = getenv("MIMI_TIMEZONE");
    if (v && v[0]) safe_copy(s_cfg.timezone, sizeof(s_cfg.timezone), v);

    v = first_nonempty_env("MIMI_TG_TOKEN", "MIMI_TELEGRAM_TOKEN");
    if (v && v[0]) safe_copy(s_cfg.tg_token, sizeof(s_cfg.tg_token), v);

    v = getenv("MIMI_CHANNEL_TELEGRAM_ENABLED");
    if (v && v[0]) {
        bool parsed = false;
        if (parse_bool_str(v, &parsed)) s_cfg.channel_telegram_enabled = parsed;
    }

    v = getenv("MIMI_WS_REQUIRE_TOKEN");
    if (v && v[0]) {
        bool parsed = false;
        if (parse_bool_str(v, &parsed)) s_cfg.ws_require_token = parsed;
    }

    v = getenv("MIMI_WS_TOKEN");
    if (v && v[0]) safe_copy(s_cfg.ws_token, sizeof(s_cfg.ws_token), v);

    v = getenv("MIMI_TG_ALLOWLIST");
    if (v && v[0]) safe_copy(s_cfg.telegram_allowlist, sizeof(s_cfg.telegram_allowlist), v);

    v = getenv("MIMI_SKILLS_ENABLED");
    if (v && v[0]) {
        bool parsed = false;
        if (parse_bool_str(v, &parsed)) s_cfg.skills_enabled = parsed;
    }

    v = getenv("MIMI_SKILLS_DIR");
    if (v && v[0]) {
        char expanded[1024];
        expand_home(v, expanded, sizeof(expanded));
        safe_copy(s_cfg.skills_dir, sizeof(s_cfg.skills_dir), expanded);
    }

    v = getenv("MIMI_SKILLS_MAX_LOADED");
    if (v && v[0]) {
        int parsed = atoi(v);
        if (parsed > HOST_CONFIG_MAX_SKILLS) parsed = HOST_CONFIG_MAX_SKILLS;
        if (parsed > 0) s_cfg.skills_max_loaded = (uint16_t)parsed;
    }

    v = getenv("MIMI_SKILLS_LIST");
    if (v && v[0]) {
        parse_skills_csv(v);
    }
}

static bool key_match(const char *a, const char *b)
{
    return (a && b && strcmp(a, b) == 0);
}

mimi_err_t host_config_load(int argc, char **argv)
{
    memset(&s_cfg, 0, sizeof(s_cfg));

    const char *env_file = getenv("MIMI_ENV_FILE");
    if (env_file && env_file[0]) {
        load_dotenv_file(env_file);
    } else {
        load_dotenv_file(".env");
    }

    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";

    safe_copy(s_cfg.model, sizeof(s_cfg.model), MIMI_LLM_DEFAULT_MODEL);
    safe_copy(s_cfg.model_provider, sizeof(s_cfg.model_provider), MIMI_LLM_PROVIDER_DEFAULT);
    safe_copy(s_cfg.api_base, sizeof(s_cfg.api_base), MIMI_SECRET_API_BASE);
    safe_copy(s_cfg.ws_bind, sizeof(s_cfg.ws_bind), "127.0.0.1");
    s_cfg.ws_port = MIMI_WS_PORT;
    safe_copy(s_cfg.timezone, sizeof(s_cfg.timezone), MIMI_TIMEZONE);
    safe_copy(s_cfg.tg_token, sizeof(s_cfg.tg_token), MIMI_SECRET_TG_TOKEN);
    s_cfg.channel_telegram_enabled = false;
    s_cfg.ws_require_token = false;
    s_cfg.ws_token[0] = '\0';
    s_cfg.telegram_allowlist[0] = '\0';
    s_cfg.skills_enabled = false;
    s_cfg.skills_max_loaded = 4;
    clear_skills_list();

    snprintf(s_cfg.state_root, sizeof(s_cfg.state_root), "%s/.mimiclaw", home);
    snprintf(s_cfg.config_path, sizeof(s_cfg.config_path), "%s/config.json", s_cfg.state_root);

    const char *arg_config = NULL;
    const char *arg_ws_bind = NULL;
    const char *arg_state_root = NULL;
    int arg_ws_port = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            arg_config = argv[++i];
        } else if (strcmp(argv[i], "--ws-bind") == 0 && i + 1 < argc) {
            arg_ws_bind = argv[++i];
        } else if (strcmp(argv[i], "--ws-port") == 0 && i + 1 < argc) {
            arg_ws_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--state-root") == 0 && i + 1 < argc) {
            arg_state_root = argv[++i];
        } else {
            ESP_LOGW(TAG, "Ignoring unknown arg: %s", argv[i]);
        }
    }

    if (arg_state_root) {
        char expanded[1024];
        expand_home(arg_state_root, expanded, sizeof(expanded));
        safe_copy(s_cfg.state_root, sizeof(s_cfg.state_root), expanded);
        if (!arg_config) {
            snprintf(s_cfg.config_path, sizeof(s_cfg.config_path), "%s/config.json", s_cfg.state_root);
        }
    }

    if (arg_config) {
        char expanded[1024];
        expand_home(arg_config, expanded, sizeof(expanded));
        safe_copy(s_cfg.config_path, sizeof(s_cfg.config_path), expanded);
    }

    char *json_text = read_file(s_cfg.config_path);
    if (json_text) {
        cJSON *root = cJSON_Parse(json_text);
        if (root && cJSON_IsObject(root)) {
            apply_json_config(root);
        } else {
            ESP_LOGW(TAG, "Invalid JSON config, using defaults/env: %s", s_cfg.config_path);
        }
        cJSON_Delete(root);
        free(json_text);
    }

    apply_env_overrides();

    if (arg_ws_bind) safe_copy(s_cfg.ws_bind, sizeof(s_cfg.ws_bind), arg_ws_bind);
    if (arg_ws_port > 0 && arg_ws_port <= 65535) s_cfg.ws_port = (uint16_t)arg_ws_port;
    if (arg_state_root) {
        char expanded[1024];
        expand_home(arg_state_root, expanded, sizeof(expanded));
        safe_copy(s_cfg.state_root, sizeof(s_cfg.state_root), expanded);
    }

    if (s_cfg.skills_dir[0] == '\0') {
        snprintf(s_cfg.skills_dir, sizeof(s_cfg.skills_dir), "%s/skills", s_cfg.state_root);
    }

    if (s_cfg.skills_max_loaded == 0 || s_cfg.skills_max_loaded > HOST_CONFIG_MAX_SKILLS) {
        s_cfg.skills_max_loaded = 4;
    }

    if (s_cfg.timezone[0]) {
        setenv("TZ", s_cfg.timezone, 1);
        tzset();
    }

    ESP_LOGI(TAG,
             "config=%s state_root=%s ws=%s:%u ws_auth_required=%s telegram_enabled=%s telegram_allowlist=%s skills_enabled=%s skills_count=%u provider=%s model=%s api_base=%s",
             s_cfg.config_path,
             s_cfg.state_root,
             s_cfg.ws_bind,
             (unsigned)s_cfg.ws_port,
             s_cfg.ws_require_token ? "true" : "false",
             s_cfg.channel_telegram_enabled ? "true" : "false",
             s_cfg.telegram_allowlist[0] ? "set" : "unset",
             s_cfg.skills_enabled ? "true" : "false",
             (unsigned)s_cfg.skills_count,
             s_cfg.model_provider,
             s_cfg.model,
             s_cfg.api_base[0] ? s_cfg.api_base : "<default>");

    return MIMI_OK;
}

const host_config_t *host_config_get(void)
{
    return &s_cfg;
}

mimi_err_t host_config_get_str(const char *ns, const char *key, char *out, size_t out_size)
{
    if (!ns || !key || !out || out_size == 0) return MIMI_ERR_INVALID_ARG;

    const char *src = NULL;

    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_API_KEY)) src = s_cfg.api_key;
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_MODEL)) src = s_cfg.model;
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_PROVIDER)) src = s_cfg.model_provider;
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_API_BASE)) src = s_cfg.api_base;
    if (key_match(ns, MIMI_NVS_SEARCH) && key_match(key, MIMI_NVS_KEY_API_KEY)) src = s_cfg.search_key;
    if (key_match(ns, MIMI_NVS_PROXY) && key_match(key, MIMI_NVS_KEY_PROXY_HOST)) src = s_cfg.proxy_host;
    if (key_match(ns, MIMI_NVS_TG) && key_match(key, MIMI_NVS_KEY_TG_TOKEN)) src = s_cfg.tg_token;

    if (!src || src[0] == '\0') {
        out[0] = '\0';
        return MIMI_ERR_NOT_FOUND;
    }

    snprintf(out, out_size, "%s", src);
    return MIMI_OK;
}

mimi_err_t host_config_set_str(const char *ns, const char *key, const char *value)
{
    if (!ns || !key || !value) return MIMI_ERR_INVALID_ARG;

    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_API_KEY)) {
        safe_copy(s_cfg.api_key, sizeof(s_cfg.api_key), value);
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_MODEL)) {
        safe_copy(s_cfg.model, sizeof(s_cfg.model), value);
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_PROVIDER)) {
        safe_copy(s_cfg.model_provider, sizeof(s_cfg.model_provider), value);
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_API_BASE)) {
        safe_copy(s_cfg.api_base, sizeof(s_cfg.api_base), value);
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_SEARCH) && key_match(key, MIMI_NVS_KEY_API_KEY)) {
        safe_copy(s_cfg.search_key, sizeof(s_cfg.search_key), value);
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_PROXY) && key_match(key, MIMI_NVS_KEY_PROXY_HOST)) {
        safe_copy(s_cfg.proxy_host, sizeof(s_cfg.proxy_host), value);
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_TG) && key_match(key, MIMI_NVS_KEY_TG_TOKEN)) {
        safe_copy(s_cfg.tg_token, sizeof(s_cfg.tg_token), value);
        return MIMI_OK;
    }

    return MIMI_ERR_INVALID_ARG;
}

mimi_err_t host_config_get_u16(const char *ns, const char *key, uint16_t *out)
{
    if (!ns || !key || !out) return MIMI_ERR_INVALID_ARG;

    if (key_match(ns, MIMI_NVS_PROXY) && key_match(key, MIMI_NVS_KEY_PROXY_PORT)) {
        if (s_cfg.proxy_port == 0) return MIMI_ERR_NOT_FOUND;
        *out = s_cfg.proxy_port;
        return MIMI_OK;
    }

    return MIMI_ERR_NOT_FOUND;
}

mimi_err_t host_config_set_u16(const char *ns, const char *key, uint16_t value)
{
    if (!ns || !key) return MIMI_ERR_INVALID_ARG;

    if (key_match(ns, MIMI_NVS_PROXY) && key_match(key, MIMI_NVS_KEY_PROXY_PORT)) {
        s_cfg.proxy_port = value;
        return MIMI_OK;
    }

    return MIMI_ERR_INVALID_ARG;
}

mimi_err_t host_config_erase_key(const char *ns, const char *key)
{
    if (!ns || !key) return MIMI_ERR_INVALID_ARG;

    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_API_KEY)) {
        s_cfg.api_key[0] = '\0';
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_MODEL)) {
        s_cfg.model[0] = '\0';
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_PROVIDER)) {
        s_cfg.model_provider[0] = '\0';
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_LLM) && key_match(key, MIMI_NVS_KEY_API_BASE)) {
        s_cfg.api_base[0] = '\0';
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_SEARCH) && key_match(key, MIMI_NVS_KEY_API_KEY)) {
        s_cfg.search_key[0] = '\0';
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_PROXY) && key_match(key, MIMI_NVS_KEY_PROXY_HOST)) {
        s_cfg.proxy_host[0] = '\0';
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_PROXY) && key_match(key, MIMI_NVS_KEY_PROXY_PORT)) {
        s_cfg.proxy_port = 0;
        return MIMI_OK;
    }
    if (key_match(ns, MIMI_NVS_TG) && key_match(key, MIMI_NVS_KEY_TG_TOKEN)) {
        s_cfg.tg_token[0] = '\0';
        return MIMI_OK;
    }

    return MIMI_ERR_INVALID_ARG;
}
