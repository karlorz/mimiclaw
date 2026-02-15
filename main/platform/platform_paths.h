#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "platform/platform_types.h"

/*
 * Initialize path mapping. For host, state_root controls where /spiffs maps to.
 * For ESP, this is a no-op.
 */
mimi_err_t platform_paths_init(const char *state_root);

/*
 * Validate virtual /spiffs path safety.
 */
bool platform_path_is_valid_virtual(const char *path);

/*
 * Translate a virtual /spiffs path to a platform real path.
 * On ESP this is identity mapping.
 */
mimi_err_t platform_path_to_real(const char *virtual_path, char *real_path, size_t real_path_size);

/*
 * List virtual /spiffs files, optionally filtered by prefix.
 */
mimi_err_t platform_paths_list_virtual(const char *prefix, char *output, size_t output_size);

/*
 * Return current state root path when meaningful (host).
 */
const char *platform_paths_state_root(void);
