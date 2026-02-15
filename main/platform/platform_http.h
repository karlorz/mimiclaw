#pragma once

#include <stddef.h>
#include "platform/platform_types.h"

typedef struct {
    const char *name;
    const char *value;
} platform_http_header_t;

/*
 * POST JSON body and collect response body into response_buf.
 * response_buf is always null-terminated on success.
 */
mimi_err_t platform_http_post_json(const char *url,
                                   const platform_http_header_t *headers,
                                   size_t header_count,
                                   const char *body,
                                   int timeout_ms,
                                   char *response_buf,
                                   size_t response_buf_size,
                                   int *status_code);

/*
 * GET request and collect response body into response_buf.
 */
mimi_err_t platform_http_get(const char *url,
                             const platform_http_header_t *headers,
                             size_t header_count,
                             int timeout_ms,
                             char *response_buf,
                             size_t response_buf_size,
                             int *status_code);

/*
 * HEAD request and return the Date header value.
 */
mimi_err_t platform_http_head_date(const char *url,
                                   const platform_http_header_t *headers,
                                   size_t header_count,
                                   int timeout_ms,
                                   char *date_buf,
                                   size_t date_buf_size,
                                   int *status_code);
