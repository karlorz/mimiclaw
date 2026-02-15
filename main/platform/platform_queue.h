#pragma once

#include <stdint.h>
#include "platform/platform_types.h"
#include "bus/message_bus.h"

mimi_err_t platform_queue_push_inbound(const mimi_msg_t *msg);
mimi_err_t platform_queue_pop_inbound(mimi_msg_t *msg, uint32_t timeout_ms);
mimi_err_t platform_queue_push_outbound(const mimi_msg_t *msg);
mimi_err_t platform_queue_pop_outbound(mimi_msg_t *msg, uint32_t timeout_ms);
