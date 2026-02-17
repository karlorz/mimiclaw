#include "context_builder.h"
#include "mimi_config.h"
#include "memory/memory_store.h"
#include "platform/platform_paths.h"
#include "skills/skills_loader.h"
#include "tools/tool_registry.h"

#ifdef MIMI_HOST_BUILD
#include "platform/config_host.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "context";

#define MIMI_SKILLS_PROMPT_BUF_SIZE  (6 * 1024)
#define MIMI_SKILLS_PROMPT_PER_SKILL 1200
#define MIMI_SKILLS_PROMPT_TOTAL     4096

static size_t append_fmt(char *buf, size_t size, size_t off, const char *fmt, ...)
{
    if (!buf || size == 0 || !fmt) return off;
    if (off >= size - 1) return size - 1;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + off, size - off, fmt, ap);
    va_end(ap);

    if (n < 0) return off;

    size_t written = (size_t)n;
    if (written >= size - off) return size - 1;
    return off + written;
}

static size_t append_file(char *buf, size_t size, size_t offset, const char *path, const char *header)
{
    if (!buf || size == 0 || !path) return offset;

    char real_path[1024];
    if (platform_path_to_real(path, real_path, sizeof(real_path)) != MIMI_OK) {
        return offset;
    }

    FILE *f = fopen(real_path, "r");
    if (!f) return offset;

    if (header) {
        offset = append_fmt(buf, size, offset, "\n## %s\n\n", header);
    }

    if (offset >= size - 1) {
        fclose(f);
        return size - 1;
    }

    size_t cap = size - offset - 1;
    size_t n = fread(buf + offset, 1, cap, f);
    offset += n;
    buf[offset] = '\0';
    fclose(f);

    return offset;
}

static size_t append_skills_prompt(char *buf, size_t size, size_t off)
{
    char skills_prompt[MIMI_SKILLS_PROMPT_BUF_SIZE];
    skills_prompt[0] = '\0';

    const char *tool_names[16] = {0};
    size_t tool_count = tool_registry_copy_names(tool_names, 16);

    skills_loader_options_t opts = {
        .enabled = false,
        .skills_dir = NULL,
        .skill_entries = NULL,
        .skill_entry_count = 0,
        .max_loaded = 0,
        .per_skill_bytes = MIMI_SKILLS_PROMPT_PER_SKILL,
        .total_bytes = MIMI_SKILLS_PROMPT_TOTAL,
    };

#ifdef MIMI_HOST_BUILD
    const host_config_t *cfg = host_config_get();
    const char *entries[HOST_CONFIG_MAX_SKILLS] = {0};

    if (cfg && cfg->skills_enabled && cfg->skills_count > 0) {
        size_t entry_count = cfg->skills_count;
        if (entry_count > HOST_CONFIG_MAX_SKILLS) {
            entry_count = HOST_CONFIG_MAX_SKILLS;
        }

        for (size_t i = 0; i < entry_count; i++) {
            entries[i] = cfg->skills_list[i];
        }

        opts.enabled = true;
        opts.skills_dir = cfg->skills_dir;
        opts.skill_entries = entries;
        opts.skill_entry_count = entry_count;
        opts.max_loaded = cfg->skills_max_loaded;
    }
#endif

    skills_loader_result_t result = {0};
    if (skills_loader_build_prompt(&opts,
                                   tool_names,
                                   tool_count,
                                   skills_prompt,
                                   sizeof(skills_prompt),
                                   &result) == MIMI_OK &&
        skills_prompt[0]) {
        off = append_fmt(buf, size, off, "\n%s\n", skills_prompt);
    }

    if (opts.enabled) {
        ESP_LOGI(TAG, "skill-load summary loaded=%u skipped=%u",
                 (unsigned)result.loaded,
                 (unsigned)result.skipped);
    }

    return off;
}

esp_err_t context_build_system_prompt(char *buf, size_t size)
{
    if (!buf || size == 0) return ESP_ERR_INVALID_ARG;

    buf[0] = '\0';
    size_t off = 0;

    off = append_fmt(buf, size, off,
        "# MimiClaw\n\n"
        "You are MimiClaw, a personal AI assistant running on an ESP32-S3 device.\n"
        "You communicate through Telegram and WebSocket.\n\n"
        "Be helpful, accurate, and concise.\n\n"
        "## Available Tools\n"
        "You have access to the following tools:\n"
        "- web_search: Search the web for current information. "
        "Use this when you need up-to-date facts, news, weather, or anything beyond your training data.\n"
        "- get_current_time: Get the current date and time. "
        "You do NOT have an internal clock - always use this tool when you need to know the time or date.\n"
        "- read_file: Read a file from SPIFFS (path must start with /spiffs/).\n"
        "- write_file: Write/overwrite a file on SPIFFS.\n"
        "- edit_file: Find-and-replace edit a file on SPIFFS.\n"
        "- list_dir: List files on SPIFFS, optionally filter by prefix.\n\n"
        "Use tools when needed. Provide your final answer as text after using tools.\n\n"
        "## Memory\n"
        "You have persistent memory stored on local flash:\n"
        "- Long-term memory: /spiffs/memory/MEMORY.md\n"
        "- Daily notes: /spiffs/memory/daily/<YYYY-MM-DD>.md\n\n"
        "IMPORTANT: Actively use memory to remember things across conversations.\n"
        "- When you learn something new about the user (name, preferences, habits, context), write it to MEMORY.md.\n"
        "- When something noteworthy happens in a conversation, append it to today's daily note.\n"
        "- Always read_file MEMORY.md before writing, so you can edit_file to update without losing existing content.\n"
        "- Use get_current_time to know today's date before writing daily notes.\n"
        "- Keep MEMORY.md concise and organized - summarize, don't dump raw conversation.\n"
        "- You should proactively save memory without being asked. If the user tells you their name, preferences, or important facts, persist them immediately.\n");

    off = append_file(buf, size, off, MIMI_SOUL_FILE, "Personality");
    off = append_file(buf, size, off, MIMI_USER_FILE, "User Info");
    off = append_file(buf, size, off, MIMI_AGENTS_FILE, "Agent Instructions");
    off = append_file(buf, size, off, MIMI_TOOLS_FILE, "Tools Bootstrap");

    off = append_skills_prompt(buf, size, off);

    char mem_buf[4096];
    if (memory_read_long_term(mem_buf, sizeof(mem_buf)) == ESP_OK && mem_buf[0]) {
        off = append_fmt(buf, size, off, "\n## Long-term Memory\n\n%s\n", mem_buf);
    }

    char recent_buf[4096];
    if (memory_read_recent(recent_buf, sizeof(recent_buf), 3) == ESP_OK && recent_buf[0]) {
        off = append_fmt(buf, size, off, "\n## Recent Notes\n\n%s\n", recent_buf);
    }

    ESP_LOGI(TAG, "System prompt built: %d bytes", (int)off);
    return ESP_OK;
}

esp_err_t context_build_messages(const char *history_json, const char *user_message,
                                 char *buf, size_t size)
{
    cJSON *history = cJSON_Parse(history_json);
    if (!history) {
        history = cJSON_CreateArray();
    }

    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", user_message);
    cJSON_AddItemToArray(history, user_msg);

    char *json_str = cJSON_PrintUnformatted(history);
    cJSON_Delete(history);

    if (json_str) {
        strncpy(buf, json_str, size - 1);
        buf[size - 1] = '\0';
        free(json_str);
    } else {
        snprintf(buf, size, "[{\"role\":\"user\",\"content\":\"%s\"}]", user_message);
    }

    return ESP_OK;
}
