#pragma once

#include <stdint.h>

#ifdef MIMI_HOST_BUILD

typedef int32_t mimi_err_t;

#define MIMI_OK                    0
#define MIMI_FAIL                 -1
#define MIMI_ERR_NO_MEM           0x101
#define MIMI_ERR_INVALID_ARG      0x102
#define MIMI_ERR_INVALID_STATE    0x103
#define MIMI_ERR_INVALID_SIZE     0x104
#define MIMI_ERR_NOT_FOUND        0x105
#define MIMI_ERR_TIMEOUT          0x106
#define MIMI_ERR_HTTP_CONNECT     0x107
#define MIMI_ERR_HTTP_WRITE_DATA  0x108

const char *mimi_err_to_name(mimi_err_t err);

#else

#include "esp_err.h"

typedef esp_err_t mimi_err_t;

#define MIMI_OK                    ESP_OK
#define MIMI_FAIL                  ESP_FAIL
#define MIMI_ERR_NO_MEM            ESP_ERR_NO_MEM
#define MIMI_ERR_INVALID_ARG       ESP_ERR_INVALID_ARG
#define MIMI_ERR_INVALID_STATE     ESP_ERR_INVALID_STATE
#define MIMI_ERR_INVALID_SIZE      ESP_ERR_INVALID_SIZE
#define MIMI_ERR_NOT_FOUND         ESP_ERR_NOT_FOUND
#define MIMI_ERR_TIMEOUT           ESP_ERR_TIMEOUT
#define MIMI_ERR_HTTP_CONNECT      ESP_ERR_HTTP_CONNECT
#define MIMI_ERR_HTTP_WRITE_DATA   ESP_ERR_HTTP_WRITE_DATA

#define mimi_err_to_name esp_err_to_name

#endif
