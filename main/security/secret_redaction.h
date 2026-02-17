#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define MIMI_SECRET_REDACTION_PLACEHOLDER "[REDACTED_SECRET]"

typedef struct {
    size_t replacement_count;
    bool truncated;
} secret_redaction_result_t;

/*
 * Returns a conservative output buffer size for redaction output.
 * Caller should allocate at least this size for deterministic behavior.
 */
size_t secret_redaction_max_output(size_t input_len);

/*
 * Redact secret-like values from text.
 * - Key/value labels like *_API_KEY=..., TOKEN: ..., SECRET: ..., PASSWORD: ...
 * - Multiline label + value forms (e.g., "API key:\\n<value>")
 *
 * Output is always NUL-terminated when output_size > 0.
 * result may be NULL.
 */
esp_err_t secret_redact_text(const char *input,
                             char *output,
                             size_t output_size,
                             secret_redaction_result_t *result);
