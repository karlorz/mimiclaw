#include "platform/platform_paths.h"
#include "mimi_config.h"

#include <string.h>
#include <dirent.h>
#include <stdio.h>

mimi_err_t platform_paths_init(const char *state_root)
{
    (void)state_root;
    return MIMI_OK;
}

bool platform_path_is_valid_virtual(const char *path)
{
    if (!path) return false;
    if (strncmp(path, "/spiffs", 7) != 0) return false;
    if (path[7] != '\0' && path[7] != '/') return false;
    if (strstr(path, "..") != NULL) return false;
    return true;
}

mimi_err_t platform_path_to_real(const char *virtual_path, char *real_path, size_t real_path_size)
{
    if (!platform_path_is_valid_virtual(virtual_path) || !real_path || real_path_size == 0) {
        return MIMI_ERR_INVALID_ARG;
    }
    snprintf(real_path, real_path_size, "%s", virtual_path);
    return MIMI_OK;
}

mimi_err_t platform_paths_list_virtual(const char *prefix, char *output, size_t output_size)
{
    if (!output || output_size == 0) return MIMI_ERR_INVALID_ARG;

    DIR *dir = opendir(MIMI_SPIFFS_BASE);
    if (!dir) return MIMI_FAIL;

    output[0] = '\0';
    size_t off = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && off < output_size - 1) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", MIMI_SPIFFS_BASE, ent->d_name);

        if (prefix && prefix[0] != '\0') {
            if (strncmp(full_path, prefix, strlen(prefix)) != 0) continue;
        }

        off += snprintf(output + off, output_size - off, "%s\n", full_path);
    }

    closedir(dir);
    return MIMI_OK;
}

const char *platform_paths_state_root(void)
{
    return MIMI_SPIFFS_BASE;
}
