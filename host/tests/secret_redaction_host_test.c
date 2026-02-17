#include "security/secret_redaction.h"

#include <stdio.h>
#include <string.h>

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

static void test_multiline_label_redaction(void)
{
    const char *input =
        "Search key:\n"
        "sk-live-12345\n"
        "notes stay visible";
    char output[1024];
    secret_redaction_result_t res = {0};

    EXPECT_EQ_INT(secret_redact_text(input, output, sizeof(output), &res), ESP_OK);
    EXPECT_TRUE(strstr(output, "Search key:") != NULL);
    EXPECT_TRUE(strstr(output, "notes stay visible") != NULL);
    EXPECT_TRUE(strstr(output, "sk-live-12345") == NULL);
    EXPECT_TRUE(strstr(output, MIMI_SECRET_REDACTION_PLACEHOLDER) != NULL);
    EXPECT_TRUE(res.replacement_count >= 1);
}

static void test_key_value_redaction(void)
{
    const char *input =
        "OPENAI_API_KEY=sk-test-abc\n"
        "TOKEN: abc123\n"
        "PASSWORD: hunter2\n"
        "set_api_key sk-live-xyz";
    char output[1024];
    secret_redaction_result_t res = {0};

    EXPECT_EQ_INT(secret_redact_text(input, output, sizeof(output), &res), ESP_OK);
    EXPECT_TRUE(strstr(output, "sk-test-abc") == NULL);
    EXPECT_TRUE(strstr(output, "abc123") == NULL);
    EXPECT_TRUE(strstr(output, "hunter2") == NULL);
    EXPECT_TRUE(strstr(output, "sk-live-xyz") == NULL);
    EXPECT_TRUE(strstr(output, MIMI_SECRET_REDACTION_PLACEHOLDER) != NULL);
    EXPECT_TRUE(res.replacement_count >= 4);
}

static void test_benign_text_unchanged(void)
{
    const char *input = "hello mimiclaw, please summarize today";
    char output[256];
    secret_redaction_result_t res = {0};

    EXPECT_EQ_INT(secret_redact_text(input, output, sizeof(output), &res), ESP_OK);
    EXPECT_TRUE(strcmp(input, output) == 0);
    EXPECT_EQ_INT(res.replacement_count, 0);
}

int main(void)
{
    test_multiline_label_redaction();
    test_key_value_redaction();
    test_benign_text_unchanged();

    if (s_failures != 0) {
        fprintf(stderr, "secret_redaction_host_test failed with %d assertion(s)\n", s_failures);
        return 1;
    }

    printf("secret_redaction_host_test passed\n");
    return 0;
}
