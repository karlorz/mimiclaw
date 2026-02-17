#include "memory/session_mgr.h"
#include "platform/config_host.h"
#include "platform/platform_paths.h"

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

static int make_state_root(char *out, size_t out_size)
{
    if (!out || out_size < 32) return -1;
    snprintf(out, out_size, "/tmp/mimiclaw-session-scrub-test-XXXXXX");
    return mkdtemp(out) ? 0 : -1;
}

static void remove_state_root(const char *root)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/sessions/tg_scrub.jsonl", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/config", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/memory", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/sessions", root);
    rmdir(path);
    rmdir(root);
}

static void test_session_scrub_rewrites_secret_content(void)
{
    char state_root[PATH_MAX];
    char real_session_path[PATH_MAX];

    EXPECT_EQ_INT(make_state_root(state_root, sizeof(state_root)), 0);
    EXPECT_EQ_INT(platform_paths_init(state_root), MIMI_OK);
    EXPECT_EQ_INT(session_mgr_init(), ESP_OK);

    EXPECT_EQ_INT(platform_path_to_real("/spiffs/sessions/tg_scrub.jsonl",
                                        real_session_path,
                                        sizeof(real_session_path)),
                  MIMI_OK);

    FILE *f = fopen(real_session_path, "w");
    EXPECT_TRUE(f != NULL);
    if (f) {
        fputs("{\"role\":\"user\",\"content\":\"OPENAI_API_KEY=sk-live-abc\",\"ts\":1}\n", f);
        fputs("{\"role\":\"assistant\",\"content\":\"Search key:\\nsk-live-xyz\",\"ts\":2}\n", f);
        fputs("{\"role\":\"assistant\",\"content\":\"safe text\",\"ts\":3}\n", f);
        fclose(f);
    }

    session_scrub_summary_t summary = {0};
    EXPECT_EQ_INT(session_scrub_secrets_all(&summary), ESP_OK);
    EXPECT_EQ_INT((int)summary.files_total, 1);
    EXPECT_EQ_INT((int)summary.files_updated, 1);
    EXPECT_TRUE(summary.lines_redacted >= 2);
    EXPECT_TRUE(summary.replacement_count >= 2);

    f = fopen(real_session_path, "r");
    EXPECT_TRUE(f != NULL);
    char buf[4096] = {0};
    if (f) {
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
    }

    EXPECT_TRUE(strstr(buf, "sk-live-abc") == NULL);
    EXPECT_TRUE(strstr(buf, "sk-live-xyz") == NULL);
    EXPECT_TRUE(strstr(buf, "[REDACTED_SECRET]") != NULL);
    EXPECT_TRUE(strstr(buf, "safe text") != NULL);

    remove_state_root(state_root);
}

int main(void)
{
    test_session_scrub_rewrites_secret_content();

    if (s_failures != 0) {
        fprintf(stderr, "session_scrub_host_test failed with %d assertion(s)\n", s_failures);
        return 1;
    }

    printf("session_scrub_host_test passed\n");
    return 0;
}
