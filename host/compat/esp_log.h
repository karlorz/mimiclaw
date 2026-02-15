#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN  2
#define ESP_LOG_INFO  3
#define ESP_LOG_DEBUG 4

static inline void esp_log_level_set(const char *tag, int level)
{
    (void)tag;
    (void)level;
}

static inline void host_log_print(const char *lvl, const char *tag, const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(stderr, "%s [%s] %s: ", ts, lvl, tag ? tag : "log");

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

#define ESP_LOGE(tag, fmt, ...) host_log_print("E", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) host_log_print("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) host_log_print("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) host_log_print("D", tag, fmt, ##__VA_ARGS__)
