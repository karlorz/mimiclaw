#include "agent/context_builder.h"
#include "memory/memory_store.h"
#include "platform/config_host.h"
#include "platform/platform_http.h"
#include "platform/platform_paths.h"
#include "tools/tool_registry.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int s_failures = 0;

#define EXPECT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        s_failures++; \
    } \
} while (0)

#define EXPECT_EQ_INT(actual, expected) do { \
    int _a = (int)(actual); \
    int _e = (int)(expected); \
    if (_a != _e) { \
        fprintf(stderr, "FAIL %s:%d: %s (%d) != %s (%d)\n", \
                __FILE__, __LINE__, #actual, _a, #expected, _e); \
        s_failures++; \
    } \
} while (0)

const host_config_t *host_config_get(void)
{
    static host_config_t cfg;
    return &cfg;
}

mimi_err_t host_config_get_str(const char *ns, const char *key, char *out, size_t out_size)
{
    (void)ns;
    (void)key;
    if (out && out_size > 0) out[0] = '\0';
    return MIMI_ERR_NOT_FOUND;
}

mimi_err_t host_config_set_str(const char *ns, const char *key, const char *value)
{
    (void)ns;
    (void)key;
    (void)value;
    return MIMI_OK;
}

mimi_err_t host_config_get_u16(const char *ns, const char *key, uint16_t *out)
{
    (void)ns;
    (void)key;
    if (out) *out = 0;
    return MIMI_ERR_NOT_FOUND;
}

mimi_err_t host_config_set_u16(const char *ns, const char *key, uint16_t value)
{
    (void)ns;
    (void)key;
    (void)value;
    return MIMI_OK;
}

mimi_err_t host_config_erase_key(const char *ns, const char *key)
{
    (void)ns;
    (void)key;
    return MIMI_OK;
}

mimi_err_t platform_http_post_json(const char *url,
                                   const platform_http_header_t *headers,
                                   size_t header_count,
                                   const char *body,
                                   int timeout_ms,
                                   char *response_buf,
                                   size_t response_buf_size,
                                   int *status_code)
{
    (void)url;
    (void)headers;
    (void)header_count;
    (void)body;
    (void)timeout_ms;
    if (response_buf && response_buf_size > 0) response_buf[0] = '\0';
    if (status_code) *status_code = 0;
    return MIMI_FAIL;
}

mimi_err_t platform_http_get(const char *url,
                             const platform_http_header_t *headers,
                             size_t header_count,
                             int timeout_ms,
                             char *response_buf,
                             size_t response_buf_size,
                             int *status_code)
{
    (void)url;
    (void)headers;
    (void)header_count;
    (void)timeout_ms;
    if (response_buf && response_buf_size > 0) response_buf[0] = '\0';
    if (status_code) *status_code = 0;
    return MIMI_FAIL;
}

mimi_err_t platform_http_head_date(const char *url,
                                   const platform_http_header_t *headers,
                                   size_t header_count,
                                   int timeout_ms,
                                   char *date_buf,
                                   size_t date_buf_size,
                                   int *status_code)
{
    (void)url;
    (void)headers;
    (void)header_count;
    (void)timeout_ms;
    if (date_buf && date_buf_size > 0) date_buf[0] = '\0';
    if (status_code) *status_code = 0;
    return MIMI_FAIL;
}

static int make_state_root(char *out, size_t out_size)
{
    if (!out || out_size < 32) {
        return -1;
    }

    snprintf(out, out_size, "/tmp/mimiclaw-context-variant-test-XXXXXX");
    if (!mkdtemp(out)) {
        return -1;
    }
    return 0;
}

static void remove_state_root(const char *root)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/config", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/memory", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/sessions", root);
    rmdir(path);
    rmdir(root);
}

static void test_context_prompt_host_variant(void)
{
    char state_root[PATH_MAX];
    char prompt[24 * 1024];

    setenv("MIMI_HOST_OS_OVERRIDE", "macOS", 1);

    EXPECT_EQ_INT(make_state_root(state_root, sizeof(state_root)), 0);
    EXPECT_EQ_INT(platform_paths_init(state_root), MIMI_OK);
    EXPECT_EQ_INT(memory_store_init(), ESP_OK);
    EXPECT_EQ_INT(tool_registry_init(), ESP_OK);
    EXPECT_EQ_INT(context_build_system_prompt(prompt, sizeof(prompt)), ESP_OK);

    EXPECT_TRUE(strstr(prompt, "running as a macOS host daemon") != NULL);
    EXPECT_TRUE(strstr(prompt, "Active host OS: macOS.") != NULL);
    EXPECT_TRUE(strstr(prompt, "Do not ask the user to choose OS when the active host OS is known.") != NULL);
    EXPECT_TRUE(strstr(prompt, "Output only commands for the active host OS.") != NULL);
    EXPECT_TRUE(strstr(prompt, "Never echo raw API keys, tokens, passwords, or secrets.") != NULL);
    EXPECT_TRUE(strstr(prompt, "[REDACTED_SECRET]") != NULL);
    EXPECT_TRUE(strstr(prompt, "/spiffs/") != NULL);
    EXPECT_TRUE(strstr(prompt, "running on an ESP32-S3 device") == NULL);
    EXPECT_TRUE(strstr(prompt, "Linux/macOS host daemon") == NULL);

    unsetenv("MIMI_HOST_OS_OVERRIDE");
    remove_state_root(state_root);
}

int main(void)
{
    test_context_prompt_host_variant();

    if (s_failures != 0) {
        fprintf(stderr, "context_variant_host_test failed with %d assertion(s)\n", s_failures);
        return 1;
    }

    printf("context_variant_host_test passed\n");
    return 0;
}
