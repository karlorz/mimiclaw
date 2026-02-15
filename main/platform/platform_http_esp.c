#include "platform/platform_http.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

#include "proxy/http_proxy.h"

static const char *TAG = "platform_http";

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} http_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_buf_t *hb = (http_buf_t *)evt->user_data;
    if (!hb || evt->event_id != HTTP_EVENT_ON_DATA || !evt->data || evt->data_len <= 0) {
        return ESP_OK;
    }

    size_t copy = (hb->len + (size_t)evt->data_len < hb->cap - 1)
                    ? (size_t)evt->data_len
                    : (hb->cap - 1 - hb->len);
    if (copy > 0) {
        memcpy(hb->buf + hb->len, evt->data, copy);
        hb->len += copy;
        hb->buf[hb->len] = '\0';
    }
    return ESP_OK;
}

static void apply_headers(esp_http_client_handle_t client,
                          const platform_http_header_t *headers,
                          size_t header_count)
{
    for (size_t i = 0; i < header_count; i++) {
        if (!headers[i].name || !headers[i].value) continue;
        esp_http_client_set_header(client, headers[i].name, headers[i].value);
    }
}

static mimi_err_t direct_request(esp_http_client_method_t method,
                                 const char *url,
                                 const platform_http_header_t *headers,
                                 size_t header_count,
                                 const char *body,
                                 int timeout_ms,
                                 char *response_buf,
                                 size_t response_buf_size,
                                 int *status_code)
{
    if (!url || !response_buf || response_buf_size == 0) return MIMI_ERR_INVALID_ARG;

    http_buf_t hb = {
        .buf = response_buf,
        .cap = response_buf_size,
        .len = 0,
    };
    response_buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .method = method,
        .timeout_ms = timeout_ms,
        .event_handler = http_event_handler,
        .user_data = &hb,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return MIMI_FAIL;

    apply_headers(client, headers, header_count);
    if (body) {
        esp_http_client_set_post_field(client, body, strlen(body));
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code) *status_code = status;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "direct request failed: %s", esp_err_to_name(err));
        return err;
    }
    return MIMI_OK;
}

static bool parse_https_url(const char *url, char *host, size_t host_size, char *path, size_t path_size)
{
    if (!url || strncmp(url, "https://", 8) != 0) return false;

    const char *p = url + 8;
    const char *slash = strchr(p, '/');
    if (!slash) {
        snprintf(host, host_size, "%s", p);
        snprintf(path, path_size, "/");
        return true;
    }

    size_t hlen = (size_t)(slash - p);
    if (hlen == 0 || hlen >= host_size) return false;
    memcpy(host, p, hlen);
    host[hlen] = '\0';
    snprintf(path, path_size, "%s", slash);
    return true;
}

static char *strcasestr_local(const char *haystack, const char *needle)
{
    if (!haystack || !needle || needle[0] == '\0') return (char *)haystack;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, nlen) == 0) {
            return (char *)p;
        }
    }
    return NULL;
}

static mimi_err_t proxy_request(const char *method,
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
    char host[128];
    char path[512];
    if (!parse_https_url(url, host, sizeof(host), path, sizeof(path))) {
        return MIMI_ERR_INVALID_ARG;
    }

    proxy_conn_t *conn = proxy_conn_open(host, 443, timeout_ms);
    if (!conn) return MIMI_ERR_HTTP_CONNECT;

    int body_len = body ? (int)strlen(body) : 0;
    char req[4096];
    int off = snprintf(req, sizeof(req),
                       "%s %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Connection: close\r\n",
                       method, path, host);

    for (size_t i = 0; i < header_count && off < (int)sizeof(req) - 4; i++) {
        if (!headers[i].name || !headers[i].value) continue;
        off += snprintf(req + off, sizeof(req) - off, "%s: %s\r\n", headers[i].name, headers[i].value);
    }

    if (body_len > 0) {
        off += snprintf(req + off, sizeof(req) - off, "Content-Length: %d\r\n", body_len);
    }
    off += snprintf(req + off, sizeof(req) - off, "\r\n");

    if (off <= 0 || off >= (int)sizeof(req)) {
        proxy_conn_close(conn);
        return MIMI_ERR_INVALID_SIZE;
    }

    if (proxy_conn_write(conn, req, off) < 0) {
        proxy_conn_close(conn);
        return MIMI_ERR_HTTP_WRITE_DATA;
    }
    if (body_len > 0 && proxy_conn_write(conn, body, body_len) < 0) {
        proxy_conn_close(conn);
        return MIMI_ERR_HTTP_WRITE_DATA;
    }

    size_t cap = response_buf_size + 4096;
    char *raw = calloc(1, cap);
    if (!raw) {
        proxy_conn_close(conn);
        return MIMI_ERR_NO_MEM;
    }

    size_t len = 0;
    char tmp[4096];
    while (1) {
        int n = proxy_conn_read(conn, tmp, sizeof(tmp), timeout_ms);
        if (n <= 0) break;
        if (len + (size_t)n + 1 > cap) {
            size_t new_cap = cap * 2;
            char *grown = realloc(raw, new_cap);
            if (!grown) break;
            raw = grown;
            cap = new_cap;
        }
        memcpy(raw + len, tmp, (size_t)n);
        len += (size_t)n;
        raw[len] = '\0';
    }

    proxy_conn_close(conn);

    if (status_code) *status_code = 0;
    if (len > 5 && strncmp(raw, "HTTP/", 5) == 0) {
        char *sp = strchr(raw, ' ');
        if (sp && status_code) *status_code = atoi(sp + 1);
    }

    if (date_buf && date_buf_size > 0) {
        date_buf[0] = '\0';
        char *date_hdr = strcasestr_local(raw, "\r\nDate:");
        if (date_hdr) {
            date_hdr += 7;
            while (*date_hdr == ' ') date_hdr++;
            char *eol = strstr(date_hdr, "\r\n");
            if (eol) {
                size_t dlen = (size_t)(eol - date_hdr);
                if (dlen >= date_buf_size) dlen = date_buf_size - 1;
                memcpy(date_buf, date_hdr, dlen);
                date_buf[dlen] = '\0';
            }
        }
    }

    if (response_buf && response_buf_size > 0) {
        response_buf[0] = '\0';
        char *body_start = strstr(raw, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            size_t blen = strlen(body_start);
            if (blen >= response_buf_size) blen = response_buf_size - 1;
            memcpy(response_buf, body_start, blen);
            response_buf[blen] = '\0';
        }
    }

    free(raw);
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
    if (http_proxy_is_enabled()) {
        return proxy_request("POST", url, headers, header_count, body, timeout_ms,
                             response_buf, response_buf_size, status_code, NULL, 0);
    }
    return direct_request(HTTP_METHOD_POST, url, headers, header_count, body, timeout_ms,
                          response_buf, response_buf_size, status_code);
}

mimi_err_t platform_http_get(const char *url,
                             const platform_http_header_t *headers,
                             size_t header_count,
                             int timeout_ms,
                             char *response_buf,
                             size_t response_buf_size,
                             int *status_code)
{
    if (http_proxy_is_enabled()) {
        return proxy_request("GET", url, headers, header_count, NULL, timeout_ms,
                             response_buf, response_buf_size, status_code, NULL, 0);
    }
    return direct_request(HTTP_METHOD_GET, url, headers, header_count, NULL, timeout_ms,
                          response_buf, response_buf_size, status_code);
}

mimi_err_t platform_http_head_date(const char *url,
                                   const platform_http_header_t *headers,
                                   size_t header_count,
                                   int timeout_ms,
                                   char *date_buf,
                                   size_t date_buf_size,
                                   int *status_code)
{
    if (!date_buf || date_buf_size == 0) return MIMI_ERR_INVALID_ARG;
    date_buf[0] = '\0';

    if (http_proxy_is_enabled()) {
        char scratch[2] = {0};
        return proxy_request("HEAD", url, headers, header_count, NULL, timeout_ms,
                             scratch, sizeof(scratch), status_code, date_buf, date_buf_size);
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_HEAD,
        .timeout_ms = timeout_ms,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return MIMI_FAIL;
    apply_headers(client, headers, header_count);
    esp_err_t req_err = esp_http_client_perform(client);
    char *date_ptr = NULL;
    if (req_err == ESP_OK) {
        esp_http_client_get_header(client, "Date", &date_ptr);
        if (status_code) *status_code = esp_http_client_get_status_code(client);
    }
    if (date_ptr) {
        snprintf(date_buf, date_buf_size, "%s", date_ptr);
    }
    esp_http_client_cleanup(client);
    return (req_err == ESP_OK) ? MIMI_OK : req_err;
}
