#include "platform/platform_queue.h"

mimi_err_t platform_queue_push_inbound(const mimi_msg_t *msg)
{
    return message_bus_push_inbound(msg);
}

mimi_err_t platform_queue_pop_inbound(mimi_msg_t *msg, uint32_t timeout_ms)
{
    return message_bus_pop_inbound(msg, timeout_ms);
}

mimi_err_t platform_queue_push_outbound(const mimi_msg_t *msg)
{
    return message_bus_push_outbound(msg);
}

mimi_err_t platform_queue_pop_outbound(mimi_msg_t *msg, uint32_t timeout_ms)
{
    return message_bus_pop_outbound(msg, timeout_ms);
}
