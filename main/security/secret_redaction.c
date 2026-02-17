#include "security/secret_redaction.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    bool has_key;
    bool has_api;
    bool has_search;
    bool has_token;
    bool has_secret;
    bool has_password;
    bool has_access;
    bool has_private;
    bool has_bot;
    bool has_tg;
    bool has_telegram;
    bool has_auth;
    bool has_unknown;
    size_t token_count;
} label_flags_t;

static bool is_space_ch(char c)
{
    return (c == ' ' || c == '\t' || c == '\r');
}

static size_t span_ltrim(const char *s, size_t start, size_t end)
{
    while (start < end && is_space_ch(s[start])) start++;
    return start;
}

static size_t span_rtrim(const char *s, size_t start, size_t end)
{
    while (end > start && is_space_ch(s[end - 1])) end--;
    return end;
}

static size_t span_trim_start(const char *s, size_t end)
{
    return span_ltrim(s, 0, end);
}

static size_t span_trim_end(const char *s, size_t start, size_t end)
{
    return span_rtrim(s, start, end);
}

static bool append_bytes(char *out,
                         size_t out_size,
                         size_t *off,
                         const char *src,
                         size_t len,
                         bool *truncated)
{
    if (!out || out_size == 0 || !off || !src) return false;
    if (*off >= out_size - 1) {
        *truncated = true;
        return false;
    }

    size_t remain = out_size - 1 - *off;
    size_t copy_n = (len < remain) ? len : remain;
    if (copy_n > 0) {
        memcpy(out + *off, src, copy_n);
        *off += copy_n;
    }
    if (copy_n < len) {
        *truncated = true;
        return false;
    }
    return true;
}

static bool append_placeholder(char *out, size_t out_size, size_t *off, bool *truncated)
{
    return append_bytes(out,
                        out_size,
                        off,
                        MIMI_SECRET_REDACTION_PLACEHOLDER,
                        strlen(MIMI_SECRET_REDACTION_PLACEHOLDER),
                        truncated);
}

static bool token_eq(const char *tok, size_t len, const char *lit)
{
    size_t lit_len = strlen(lit);
    if (len != lit_len) return false;
    return strncmp(tok, lit, len) == 0;
}

static bool token_allowed(const char *tok, size_t len)
{
    return token_eq(tok, len, "API") ||
           token_eq(tok, len, "KEY") ||
           token_eq(tok, len, "TOKEN") ||
           token_eq(tok, len, "SECRET") ||
           token_eq(tok, len, "PASSWORD") ||
           token_eq(tok, len, "PASSCODE") ||
           token_eq(tok, len, "SEARCH") ||
           token_eq(tok, len, "ACCESS") ||
           token_eq(tok, len, "PRIVATE") ||
           token_eq(tok, len, "BOT") ||
           token_eq(tok, len, "TG") ||
           token_eq(tok, len, "TELEGRAM") ||
           token_eq(tok, len, "OPENAI") ||
           token_eq(tok, len, "ANTHROPIC") ||
           token_eq(tok, len, "CLAUDE") ||
           token_eq(tok, len, "GPT") ||
           token_eq(tok, len, "AUTH") ||
           token_eq(tok, len, "AUTHORIZATION") ||
           token_eq(tok, len, "WS");
}

static void set_flag_for_token(label_flags_t *flags, const char *tok, size_t len)
{
    if (!flags || !tok || len == 0) return;

    flags->token_count++;

    if (token_eq(tok, len, "KEY")) flags->has_key = true;
    else if (token_eq(tok, len, "API")) flags->has_api = true;
    else if (token_eq(tok, len, "SEARCH")) flags->has_search = true;
    else if (token_eq(tok, len, "TOKEN")) flags->has_token = true;
    else if (token_eq(tok, len, "SECRET")) flags->has_secret = true;
    else if (token_eq(tok, len, "PASSWORD") || token_eq(tok, len, "PASSCODE")) flags->has_password = true;
    else if (token_eq(tok, len, "ACCESS")) flags->has_access = true;
    else if (token_eq(tok, len, "PRIVATE")) flags->has_private = true;
    else if (token_eq(tok, len, "BOT")) flags->has_bot = true;
    else if (token_eq(tok, len, "TG")) flags->has_tg = true;
    else if (token_eq(tok, len, "TELEGRAM")) flags->has_telegram = true;
    else if (token_eq(tok, len, "AUTH") || token_eq(tok, len, "AUTHORIZATION")) flags->has_auth = true;
}

static void analyze_label_tokens(const char *s,
                                 size_t start,
                                 size_t end,
                                 label_flags_t *flags,
                                 bool track_unknown)
{
    if (!s || !flags || start >= end) return;

    size_t i = start;
    while (i < end) {
        while (i < end && !isalnum((unsigned char)s[i])) i++;
        if (i >= end) break;

        char token[32];
        size_t t = 0;
        while (i < end && isalnum((unsigned char)s[i])) {
            if (t + 1 < sizeof(token)) {
                token[t++] = (char)toupper((unsigned char)s[i]);
            }
            i++;
        }
        token[t] = '\0';
        if (t == 0) continue;

        set_flag_for_token(flags, token, t);
        if (track_unknown && !token_allowed(token, t)) {
            flags->has_unknown = true;
        }
    }
}

static bool flags_indicate_secret(const label_flags_t *flags)
{
    if (!flags) return false;
    if (flags->has_secret || flags->has_password || flags->has_token) return true;

    if (flags->has_key &&
        (flags->has_api || flags->has_search || flags->has_access || flags->has_private ||
         flags->has_bot || flags->has_tg || flags->has_telegram || flags->has_auth)) {
        return true;
    }

    return false;
}

static bool span_is_placeholder(const char *s, size_t start, size_t end)
{
    if (!s) return false;

    size_t ts = span_ltrim(s, start, end);
    size_t te = span_rtrim(s, ts, end);
    size_t ph_len = strlen(MIMI_SECRET_REDACTION_PLACEHOLDER);
    if (te - ts != ph_len) return false;

    return strncmp(s + ts, MIMI_SECRET_REDACTION_PLACEHOLDER, ph_len) == 0;
}

static bool span_is_sensitive_label(const char *s, size_t start, size_t end)
{
    label_flags_t flags = {0};
    analyze_label_tokens(s, start, end, &flags, false);
    return flags_indicate_secret(&flags);
}

static bool span_is_sensitive_label_only(const char *s, size_t start, size_t end)
{
    size_t ts = span_ltrim(s, start, end);
    size_t te = span_rtrim(s, ts, end);
    if (ts >= te) return false;

    if (s[te - 1] == ':') {
        te = span_rtrim(s, ts, te - 1);
        if (ts >= te) return false;
    }

    label_flags_t flags = {0};
    analyze_label_tokens(s, ts, te, &flags, true);
    if (flags.token_count == 0 || flags.has_unknown) return false;
    return flags_indicate_secret(&flags);
}

static bool span_starts_with_set(const char *s, size_t start, size_t end)
{
    if (!s || end <= start) return false;
    size_t len = end - start;
    if (len < 3) return false;

    char a = (char)tolower((unsigned char)s[start]);
    char b = (char)tolower((unsigned char)s[start + 1]);
    char c = (char)tolower((unsigned char)s[start + 2]);
    return (a == 's' && b == 'e' && c == 't');
}

static bool span_contains_char(const char *s, size_t start, size_t end, char needle)
{
    for (size_t i = start; i < end; i++) {
        if (s[i] == needle) return true;
    }
    return false;
}

static bool span_is_sensitive_command_token(const char *s, size_t start, size_t end)
{
    if (!span_is_sensitive_label(s, start, end)) return false;
    if (span_starts_with_set(s, start, end)) return true;
    if (span_contains_char(s, start, end, '_')) return true;
    if (span_contains_char(s, start, end, '-')) return true;
    return false;
}

static void redact_line(const char *line,
                        size_t len,
                        bool *pending_multiline_label,
                        char *out,
                        size_t out_size,
                        size_t *off,
                        secret_redaction_result_t *res,
                        bool *truncated)
{
    if (!line || !pending_multiline_label || !out || !off || !res || !truncated) return;

    size_t trim_start = span_trim_start(line, len);
    size_t trim_end = span_trim_end(line, trim_start, len);

    if (*pending_multiline_label && trim_start < trim_end) {
        if (span_is_placeholder(line, trim_start, trim_end)) {
            append_bytes(out, out_size, off, line, len, truncated);
        } else {
            append_bytes(out, out_size, off, line, trim_start, truncated);
            append_placeholder(out, out_size, off, truncated);
            append_bytes(out, out_size, off, line + trim_end, len - trim_end, truncated);
            res->replacement_count++;
        }
        *pending_multiline_label = false;
        return;
    }

    size_t delim = len;
    for (size_t i = 0; i < len; i++) {
        if (line[i] == '=' || line[i] == ':') {
            delim = i;
            break;
        }
    }

    if (delim < len) {
        size_t label_start = span_ltrim(line, 0, delim);
        size_t label_end = span_rtrim(line, label_start, delim);

        if (label_start < label_end && span_is_sensitive_label(line, label_start, label_end)) {
            size_t value_start = span_ltrim(line, delim + 1, len);
            size_t value_end = span_rtrim(line, value_start, len);

            if (value_start < value_end) {
                if (span_is_placeholder(line, value_start, value_end)) {
                    append_bytes(out, out_size, off, line, len, truncated);
                } else {
                    append_bytes(out, out_size, off, line, value_start, truncated);
                    append_placeholder(out, out_size, off, truncated);
                    append_bytes(out, out_size, off, line + value_end, len - value_end, truncated);
                    res->replacement_count++;
                }
            } else {
                append_bytes(out, out_size, off, line, len, truncated);
                *pending_multiline_label = true;
            }
            return;
        }
    }

    if (trim_start < trim_end) {
        size_t token_end = trim_start;
        while (token_end < trim_end && !is_space_ch(line[token_end])) token_end++;

        size_t value_start = span_ltrim(line, token_end, len);
        size_t value_end = span_rtrim(line, value_start, len);

        if (token_end > trim_start &&
            value_start < value_end &&
            span_is_sensitive_command_token(line, trim_start, token_end)) {
            if (span_is_placeholder(line, value_start, value_end)) {
                append_bytes(out, out_size, off, line, len, truncated);
            } else {
                append_bytes(out, out_size, off, line, value_start, truncated);
                append_placeholder(out, out_size, off, truncated);
                append_bytes(out, out_size, off, line + value_end, len - value_end, truncated);
                res->replacement_count++;
            }
            return;
        }
    }

    if (trim_start < trim_end && span_is_sensitive_label_only(line, trim_start, trim_end)) {
        *pending_multiline_label = true;
    }

    append_bytes(out, out_size, off, line, len, truncated);
}

size_t secret_redaction_max_output(size_t input_len)
{
    if (input_len > ((SIZE_MAX - 64U) / 8U)) {
        return SIZE_MAX;
    }
    return (input_len * 8U) + 64U;
}

esp_err_t secret_redact_text(const char *input,
                             char *output,
                             size_t output_size,
                             secret_redaction_result_t *result)
{
    if (!input || !output || output_size == 0) return ESP_ERR_INVALID_ARG;

    secret_redaction_result_t local = {0};
    bool truncated = false;
    size_t off = 0;
    bool pending_multiline_label = false;

    size_t len = strlen(input);
    size_t line_start = 0;

    while (line_start < len) {
        size_t line_end = line_start;
        while (line_end < len && input[line_end] != '\n') line_end++;

        redact_line(input + line_start,
                    line_end - line_start,
                    &pending_multiline_label,
                    output,
                    output_size,
                    &off,
                    &local,
                    &truncated);

        if (line_end < len && input[line_end] == '\n') {
            append_bytes(output, output_size, &off, "\n", 1, &truncated);
            line_end++;
        }

        line_start = line_end;
    }

    if (len == 0) {
        output[0] = '\0';
    } else if (off < output_size) {
        output[off] = '\0';
    } else {
        output[output_size - 1] = '\0';
        truncated = true;
    }

    local.truncated = truncated;
    if (result) *result = local;
    return truncated ? ESP_ERR_INVALID_SIZE : ESP_OK;
}
