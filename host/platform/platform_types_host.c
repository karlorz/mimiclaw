#include "platform/platform_types.h"

const char *mimi_err_to_name(mimi_err_t err)
{
    switch (err) {
        case MIMI_OK: return "MIMI_OK";
        case MIMI_FAIL: return "MIMI_FAIL";
        case MIMI_ERR_NO_MEM: return "MIMI_ERR_NO_MEM";
        case MIMI_ERR_INVALID_ARG: return "MIMI_ERR_INVALID_ARG";
        case MIMI_ERR_INVALID_STATE: return "MIMI_ERR_INVALID_STATE";
        case MIMI_ERR_INVALID_SIZE: return "MIMI_ERR_INVALID_SIZE";
        case MIMI_ERR_NOT_FOUND: return "MIMI_ERR_NOT_FOUND";
        case MIMI_ERR_TIMEOUT: return "MIMI_ERR_TIMEOUT";
        case MIMI_ERR_HTTP_CONNECT: return "MIMI_ERR_HTTP_CONNECT";
        case MIMI_ERR_HTTP_WRITE_DATA: return "MIMI_ERR_HTTP_WRITE_DATA";
        default: return "MIMI_ERR_UNKNOWN";
    }
}
