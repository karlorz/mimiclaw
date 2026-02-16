#include "platform/platform_paths.h"
#include "platform/config_host.h"

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

#define EXPECT_STREQ(actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (strcmp(_a, _e) != 0) { \
        fprintf(stderr, "FAIL %s:%d:\n  actual:   %s\n  expected: %s\n", \
                __FILE__, __LINE__, _a, _e); \
        s_failures++; \
    } \
} while (0)

const host_config_t *host_config_get(void)
{
    static host_config_t cfg;
    return &cfg;
}

static int make_state_root(char *out, size_t out_size)
{
    if (!out || out_size < 32) {
        return -1;
    }

    snprintf(out, out_size, "/tmp/mimiclaw-path-map-test-XXXXXX");
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

static void test_virtual_path_validation(void)
{
    EXPECT_TRUE(platform_path_is_valid_virtual("/spiffs"));
    EXPECT_TRUE(platform_path_is_valid_virtual("/spiffs/config/SOUL.md"));
    EXPECT_TRUE(platform_path_is_valid_virtual("/spiffs/memory/2026-02-16.md"));
    EXPECT_TRUE(platform_path_is_valid_virtual("/spiffs/sessions/tg_ci.jsonl"));

    EXPECT_TRUE(!platform_path_is_valid_virtual("/spiffs/../config/SOUL.md"));
    EXPECT_TRUE(!platform_path_is_valid_virtual("/spiffs/config/../../etc/passwd"));
    EXPECT_TRUE(!platform_path_is_valid_virtual("/etc/passwd"));
    EXPECT_TRUE(!platform_path_is_valid_virtual("spiffs/config/SOUL.md"));
    EXPECT_TRUE(!platform_path_is_valid_virtual("/spiffs2/config/SOUL.md"));
}

static void test_real_path_mapping(void)
{
    char state_root[PATH_MAX];
    char actual[PATH_MAX];
    char expected[PATH_MAX];

    EXPECT_EQ_INT(make_state_root(state_root, sizeof(state_root)), 0);
    EXPECT_EQ_INT(platform_paths_init(state_root), MIMI_OK);

    EXPECT_EQ_INT(platform_path_to_real("/spiffs", actual, sizeof(actual)), MIMI_OK);
    snprintf(expected, sizeof(expected), "%s", state_root);
    EXPECT_STREQ(actual, expected);

    EXPECT_EQ_INT(platform_path_to_real("/spiffs/config/SOUL.md", actual, sizeof(actual)), MIMI_OK);
    snprintf(expected, sizeof(expected), "%s/config/SOUL.md", state_root);
    EXPECT_STREQ(actual, expected);

    EXPECT_EQ_INT(platform_path_to_real("/spiffs/memory/MEMORY.md", actual, sizeof(actual)), MIMI_OK);
    snprintf(expected, sizeof(expected), "%s/memory/MEMORY.md", state_root);
    EXPECT_STREQ(actual, expected);

    EXPECT_EQ_INT(platform_path_to_real("/spiffs/sessions/tg_demo.jsonl", actual, sizeof(actual)), MIMI_OK);
    snprintf(expected, sizeof(expected), "%s/sessions/tg_demo.jsonl", state_root);
    EXPECT_STREQ(actual, expected);

    EXPECT_EQ_INT(platform_path_to_real("/etc/passwd", actual, sizeof(actual)), MIMI_ERR_INVALID_ARG);
    EXPECT_EQ_INT(platform_path_to_real("/spiffs/config/../../etc/passwd", actual, sizeof(actual)), MIMI_ERR_INVALID_ARG);
    EXPECT_EQ_INT(platform_path_to_real("/spiffs/../config/SOUL.md", actual, sizeof(actual)), MIMI_ERR_INVALID_ARG);

    remove_state_root(state_root);
}

int main(void)
{
    test_virtual_path_validation();
    test_real_path_mapping();

    if (s_failures != 0) {
        fprintf(stderr, "path_map_host_test failed with %d assertion(s)\n", s_failures);
        return 1;
    }

    printf("path_map_host_test passed\n");
    return 0;
}
