#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "platform/platform_types.h"

typedef struct {
    bool enabled;
    const char *skills_dir;
    const char *const *skill_entries;
    size_t skill_entry_count;
    size_t max_loaded;
    size_t per_skill_bytes;
    size_t total_bytes;
} skills_loader_options_t;

typedef struct {
    size_t loaded;
    size_t skipped;
} skills_loader_result_t;

mimi_err_t skills_loader_build_prompt(const skills_loader_options_t *opts,
                                      const char *const *tool_names,
                                      size_t tool_count,
                                      char *out,
                                      size_t out_size,
                                      skills_loader_result_t *result);
