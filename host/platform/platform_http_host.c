#include "platform/platform_http.h"
#include "platform/platform_kv.h"
#include "mimi_config.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <curl/curl.h>

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    char *date_buf;
    size_t date_cap;
} curl_acc_t;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    curl_acc_t *acc = (curl_acc_t *)userdata;
    size_t total = size * nmemb;

    if (!acc || !acc->buf || acc->cap == 0) return total;

    size_t copy = (acc->len + total < acc->cap - 1) ? total : (acc->cap - 1 - acc->len);
    if (copy > 0) {
        memcpy(acc->buf + acc->len, ptr, copy);
        acc->len += copy;
        acc->buf[acc->len] = '\0';
    }

    return total;
}

static size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
    curl_acc_t *acc = (curl_acc_t *)userdata;
    size_t total = size * nitems;

    if (!acc || !acc->date_buf || acc->date_cap == 0) return total;

    if (total > 6 && strncasecmp(buffer, "Date:", 5) == 0) {
        const char *p = buffer + 5;
        while (*p == ' ' || *p == '\t') p++;

        const char *end = buffer + total;
        while (end > p && (end[-1] == '\r' || end[-1] == '\n')) end--;

        size_t len = (size_t)(end - p);
        if (len >= acc->date_cap) len = acc->date_cap - 1;

        memcpy(acc->date_buf, p, len);
        acc->date_buf[len] = '\0';
    }

    return total;
}

static void apply_proxy_if_configured(CURL *curl)
{
    char host[128] = {0};
    uint16_t port = 0;

    if (platform_kv_get_str(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST, host, sizeof(host)) != MIMI_OK) {
        return;
    }
    if (platform_kv_get_u16(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT, &port) != MIMI_OK || port == 0) {
        return;
    }

    char proxy[256];
    snprintf(proxy, sizeof(proxy), "http://%s:%u", host, (unsigned)port);
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
}

static mimi_err_t perform_request(const char *method,
                                  const char *url,
                                  const platform_http_header_t *headers,
                                  size_t header_count,
                                  const char *body,
                                  int timeout_ms,
                                  char *response_buf,
                                  size_t response_buf_size,
                                  int *status_code,
                                  char *date_buf,
                                  size_t date_buf_size)
{
    if (!url) return MIMI_ERR_INVALID_ARG;

    CURL *curl = curl_easy_init();
    if (!curl) return MIMI_FAIL;

    struct curl_slist *hdrs = NULL;
    for (size_t i = 0; i < header_count; i++) {
        if (!headers[i].name || !headers[i].value) continue;
        char line[1024];
        snprintf(line, sizeof(line), "%s: %s", headers[i].name, headers[i].value);
        hdrs = curl_slist_append(hdrs, line);
    }

    curl_acc_t acc = {
        .buf = response_buf,
        .cap = response_buf_size,
        .len = 0,
        .date_buf = date_buf,
        .date_cap = date_buf_size,
    };

    if (response_buf && response_buf_size > 0) response_buf[0] = '\0';
    if (date_buf && date_buf_size > 0) date_buf[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "mimiclaw-host/phase1");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &acc);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &acc);

    apply_proxy_if_configured(curl);

    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(body ? strlen(body) : 0));
    } else if (strcmp(method, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    CURLcode rc = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status_code) *status_code = (int)status;

    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        return MIMI_FAIL;
    }

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
    return perform_request("POST", url, headers, header_count, body,
                           timeout_ms, response_buf, response_buf_size,
                           status_code, NULL, 0);
}

mimi_err_t platform_http_get(const char *url,
                             const platform_http_header_t *headers,
                             size_t header_count,
                             int timeout_ms,
                             char *response_buf,
                             size_t response_buf_size,
                             int *status_code)
{
    return perform_request("GET", url, headers, header_count, NULL,
                           timeout_ms, response_buf, response_buf_size,
                           status_code, NULL, 0);
}

mimi_err_t platform_http_head_date(const char *url,
                                   const platform_http_header_t *headers,
                                   size_t header_count,
                                   int timeout_ms,
                                   char *date_buf,
                                   size_t date_buf_size,
                                   int *status_code)
{
    char scratch[2] = {0};
    return perform_request("HEAD", url, headers, header_count, NULL,
                           timeout_ms, scratch, sizeof(scratch),
                           status_code, date_buf, date_buf_size);
}
