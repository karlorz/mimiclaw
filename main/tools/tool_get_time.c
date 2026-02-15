#include "tool_get_time.h"
#include "mimi_config.h"
#include "platform/platform_http.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"

static const char *TAG = "tool_time";

static const char *MONTHS[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};

/* Parse "Sat, 01 Feb 2025 10:25:00 GMT" -> set system clock, return formatted string */
static bool parse_and_set_time(const char *date_str, char *out, size_t out_size)
{
    int day, year, hour, min, sec;
    char mon_str[4] = {0};

    if (sscanf(date_str, "%*[^,], %d %3s %d %d:%d:%d",
               &day, mon_str, &year, &hour, &min, &sec) != 6) {
        return false;
    }

    int mon = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(mon_str, MONTHS[i]) == 0) { mon = i; break; }
    }
    if (mon < 0) return false;

    struct tm tm = {
        .tm_sec = sec, .tm_min = min, .tm_hour = hour,
        .tm_mday = day, .tm_mon = mon, .tm_year = year - 1900,
    };

    char old_tz[128] = {0};
    const char *old_tz_env = getenv("TZ");
    if (old_tz_env && old_tz_env[0]) {
        snprintf(old_tz, sizeof(old_tz), "%s", old_tz_env);
    }

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t t = mktime(&tm);

    if (old_tz[0]) {
        setenv("TZ", old_tz, 1);
    } else {
        setenv("TZ", MIMI_TIMEZONE, 1);
    }
    tzset();

    if (t < 0) return false;

#ifndef MIMI_HOST_BUILD
    struct timeval tv = { .tv_sec = t };
    settimeofday(&tv, NULL);
#endif

    struct tm local;
    localtime_r(&t, &local);
    strftime(out, out_size, "%Y-%m-%d %H:%M:%S %Z (%A)", &local);

    return true;
}

static esp_err_t fetch_time(char *out, size_t out_size)
{
    char date_val[96] = {0};
    int status = 0;
    esp_err_t err = platform_http_head_date("https://api.telegram.org/",
                                            NULL, 0,
                                            10000,
                                            date_val, sizeof(date_val),
                                            &status);
    if (err != ESP_OK) return err;
    if (status < 200 || status >= 400) return ESP_FAIL;
    if (date_val[0] == '\0') return ESP_ERR_NOT_FOUND;

    if (!parse_and_set_time(date_val, out, out_size)) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t tool_get_time_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;

    ESP_LOGI(TAG, "Fetching current time...");

    esp_err_t err = fetch_time(output, output_size);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Time: %s", output);
    } else {
        snprintf(output, output_size, "Error: failed to fetch time (%s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", output);
    }

    return err;
}
