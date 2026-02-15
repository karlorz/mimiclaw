#pragma once

#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#elif defined(__has_include_next) && __has_include_next(<cJSON.h>)
#include_next <cJSON.h>
#else
#error "cJSON header not found"
#endif
