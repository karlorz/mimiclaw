#pragma once

#include "platform/platform_types.h"

mimi_err_t telegram_host_init(void);
mimi_err_t telegram_host_start(void);
mimi_err_t telegram_host_send(const char *chat_id, const char *text);
mimi_err_t telegram_host_stop(void);
