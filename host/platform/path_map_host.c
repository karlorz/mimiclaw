#include "platform/platform_paths.h"
#include "platform/config_host.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

static char s_state_root[1024] = {0};

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static void expand_home(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) return;

    if (in[0] == '~' && in[1] == '/') {
        const char *home = getenv("HOME");
        if (home && home[0]) {
            snprintf(out, out_size, "%s/%s", home, in + 2);
            return;
        }
    }

    snprintf(out, out_size, "%s", in);
}

static int mkdir_if_missing(const char *path)
{
    if (mkdir(path, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}

static void ensure_state_dirs(void)
{
    char p[1200];

    snprintf(p, sizeof(p), "%s", s_state_root);
    mkdir_if_missing(p);

    snprintf(p, sizeof(p), "%s/config", s_state_root);
    mkdir_if_missing(p);

    snprintf(p, sizeof(p), "%s/memory", s_state_root);
    mkdir_if_missing(p);

    snprintf(p, sizeof(p), "%s/sessions", s_state_root);
    mkdir_if_missing(p);
}

mimi_err_t platform_paths_init(const char *state_root)
{
    if (state_root && state_root[0]) {
        char expanded[1024];
        expand_home(state_root, expanded, sizeof(expanded));
        safe_copy(s_state_root, sizeof(s_state_root), expanded);
    } else if (s_state_root[0] == '\0') {
        const host_config_t *cfg = host_config_get();
        if (cfg && cfg->state_root[0]) {
            safe_copy(s_state_root, sizeof(s_state_root), cfg->state_root);
        } else {
            const char *home = getenv("HOME");
            if (!home || !home[0]) home = "/tmp";
            snprintf(s_state_root, sizeof(s_state_root), "%s/.mimiclaw", home);
        }
    }

    ensure_state_dirs();
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

    if (s_state_root[0] == '\0') {
        mimi_err_t err = platform_paths_init(NULL);
        if (err != MIMI_OK) return err;
    }

    if (strcmp(virtual_path, "/spiffs") == 0 || strcmp(virtual_path, "/spiffs/") == 0) {
        snprintf(real_path, real_path_size, "%s", s_state_root);
        return MIMI_OK;
    }

    if (strcmp(virtual_path, "/spiffs/config") == 0 || strcmp(virtual_path, "/spiffs/config/") == 0) {
        snprintf(real_path, real_path_size, "%s/config", s_state_root);
        return MIMI_OK;
    }

    if (strcmp(virtual_path, "/spiffs/memory") == 0 || strcmp(virtual_path, "/spiffs/memory/") == 0) {
        snprintf(real_path, real_path_size, "%s/memory", s_state_root);
        return MIMI_OK;
    }

    if (strcmp(virtual_path, "/spiffs/sessions") == 0 || strcmp(virtual_path, "/spiffs/sessions/") == 0) {
        snprintf(real_path, real_path_size, "%s/sessions", s_state_root);
        return MIMI_OK;
    }

    if (strncmp(virtual_path, "/spiffs/config/", 15) == 0) {
        snprintf(real_path, real_path_size, "%s/config/%s", s_state_root, virtual_path + 15);
        return MIMI_OK;
    }
    if (strncmp(virtual_path, "/spiffs/memory/", 15) == 0) {
        snprintf(real_path, real_path_size, "%s/memory/%s", s_state_root, virtual_path + 15);
        return MIMI_OK;
    }
    if (strncmp(virtual_path, "/spiffs/sessions/", 17) == 0) {
        snprintf(real_path, real_path_size, "%s/sessions/%s", s_state_root, virtual_path + 17);
        return MIMI_OK;
    }

    return MIMI_ERR_INVALID_ARG;
}

static void list_dir_recursive(const char *real_dir,
                               const char *virt_dir,
                               const char *prefix,
                               char *output,
                               size_t output_size,
                               size_t *off)
{
    DIR *dir = opendir(real_dir);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char child_real[1400];
        char child_virt[1400];
        snprintf(child_real, sizeof(child_real), "%s/%s", real_dir, ent->d_name);
        snprintf(child_virt, sizeof(child_virt), "%s/%s", virt_dir, ent->d_name);

        struct stat st;
        if (stat(child_real, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            list_dir_recursive(child_real, child_virt, prefix, output, output_size, off);
        } else if (S_ISREG(st.st_mode)) {
            if (prefix && prefix[0] != '\0') {
                if (strncmp(child_virt, prefix, strlen(prefix)) != 0) continue;
            }
            if (*off < output_size - 1) {
                *off += snprintf(output + *off, output_size - *off, "%s\n", child_virt);
            }
        }
    }

    closedir(dir);
}

mimi_err_t platform_paths_list_virtual(const char *prefix, char *output, size_t output_size)
{
    if (!output || output_size == 0) return MIMI_ERR_INVALID_ARG;
    output[0] = '\0';

    if (prefix && prefix[0] && !platform_path_is_valid_virtual(prefix)) {
        return MIMI_ERR_INVALID_ARG;
    }

    if (s_state_root[0] == '\0') {
        mimi_err_t err = platform_paths_init(NULL);
        if (err != MIMI_OK) return err;
    }

    size_t off = 0;
    char real[1200];

    snprintf(real, sizeof(real), "%s/config", s_state_root);
    list_dir_recursive(real, "/spiffs/config", prefix, output, output_size, &off);

    snprintf(real, sizeof(real), "%s/memory", s_state_root);
    list_dir_recursive(real, "/spiffs/memory", prefix, output, output_size, &off);

    snprintf(real, sizeof(real), "%s/sessions", s_state_root);
    list_dir_recursive(real, "/spiffs/sessions", prefix, output, output_size, &off);

    return MIMI_OK;
}

const char *platform_paths_state_root(void)
{
    if (s_state_root[0] == '\0') {
        const host_config_t *cfg = host_config_get();
        if (cfg && cfg->state_root[0]) {
            safe_copy(s_state_root, sizeof(s_state_root), cfg->state_root);
        }
    }
    return s_state_root;
}
