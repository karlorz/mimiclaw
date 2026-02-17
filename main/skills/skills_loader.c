#include "skills/skills_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>

#include "esp_log.h"

#define SKILLS_MAX_FILE_BYTES (128 * 1024)
#define SKILLS_MAX_REQUIRED_TOOLS 16

static const char *TAG = "skills_loader";

typedef struct {
    char name[128];
    char description[256];
    char required_tools[SKILLS_MAX_REQUIRED_TOOLS][64];
    size_t required_count;
    const char *body;
} skill_doc_t;

static void trim_inplace(char *s)
{
    if (!s || !s[0]) return;

    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

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
    if (written >= size - off) {
        return size - 1;
    }

    return off + written;
}

static size_t append_bytes(char *buf, size_t size, size_t off, const char *src, size_t src_len)
{
    if (!buf || !src || size == 0) return off;
    if (off >= size - 1) return size - 1;

    size_t cap = size - off - 1;
    if (src_len > cap) src_len = cap;

    memcpy(buf + off, src, src_len);
    off += src_len;
    buf[off] = '\0';
    return off;
}

static bool has_suffix(const char *s, const char *suffix)
{
    if (!s || !suffix) return false;
    size_t slen = strlen(s);
    size_t tlen = strlen(suffix);
    if (slen < tlen) return false;
    return strcmp(s + slen - tlen, suffix) == 0;
}

static bool validate_skill_entry(const char *entry, char *normalized, size_t normalized_size)
{
    if (!entry || !entry[0] || !normalized || normalized_size == 0) {
        return false;
    }

    if (entry[0] == '/') {
        return false;
    }

    if (strstr(entry, "..") != NULL) {
        return false;
    }

    if (has_suffix(entry, ".md")) {
        snprintf(normalized, normalized_size, "%s", entry);
    } else {
        snprintf(normalized, normalized_size, "%s/SKILL.md", entry);
    }

    return true;
}

static char *read_text_file(const char *path)
{
    if (!path || !path[0]) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long file_size = ftell(f);
    if (file_size <= 0 || file_size > SKILLS_MAX_FILE_BYTES) {
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *buf = calloc(1, (size_t)file_size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)file_size, f);
    fclose(f);

    buf[n] = '\0';
    return buf;
}

static void add_required_tool(skill_doc_t *doc, const char *tool)
{
    if (!doc || !tool || !tool[0]) return;
    if (doc->required_count >= SKILLS_MAX_REQUIRED_TOOLS) return;

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s", tool);
    trim_inplace(tmp);

    if (tmp[0] == '\0') return;

    if ((tmp[0] == '"' && tmp[strlen(tmp) - 1] == '"') ||
        (tmp[0] == '\'' && tmp[strlen(tmp) - 1] == '\'')) {
        size_t len = strlen(tmp);
        if (len >= 2) {
            memmove(tmp, tmp + 1, len - 2);
            tmp[len - 2] = '\0';
            trim_inplace(tmp);
            if (tmp[0] == '\0') return;
        }
    }

    snprintf(doc->required_tools[doc->required_count],
             sizeof(doc->required_tools[doc->required_count]),
             "%s",
             tmp);
    doc->required_count++;
}

static void parse_required_tools_csv(skill_doc_t *doc, const char *raw)
{
    if (!doc || !raw) return;

    char buf[512];
    snprintf(buf, sizeof(buf), "%s", raw);
    trim_inplace(buf);

    if (buf[0] == '[') {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == ']') {
            buf[len - 1] = '\0';
        }
        memmove(buf, buf + 1, strlen(buf));
        trim_inplace(buf);
    }

    char *saveptr = NULL;
    for (char *tok = strtok_r(buf, ",", &saveptr);
         tok;
         tok = strtok_r(NULL, ",", &saveptr)) {
        trim_inplace(tok);
        add_required_tool(doc, tok);
    }
}

static bool parse_frontmatter(char *text, skill_doc_t *doc)
{
    if (!text || !doc) return false;
    memset(doc, 0, sizeof(*doc));

    if (strncmp(text, "---", 3) != 0) {
        return false;
    }

    char *line_start = strchr(text, '\n');
    if (!line_start) return false;
    line_start++;

    char *cursor = line_start;
    char *frontmatter_end = NULL;
    while (*cursor) {
        char *line_end = strchr(cursor, '\n');
        size_t line_len = line_end ? (size_t)(line_end - cursor) : strlen(cursor);

        if ((line_len == 3 || (line_len == 4 && cursor[3] == '\r')) &&
            strncmp(cursor, "---", 3) == 0) {
            frontmatter_end = line_end ? line_end + 1 : cursor + line_len;
            break;
        }

        cursor = line_end ? line_end + 1 : cursor + line_len;
    }

    if (!frontmatter_end) {
        return false;
    }

    bool list_mode = false;
    cursor = line_start;
    while (cursor < frontmatter_end && *cursor) {
        char *line_end = strchr(cursor, '\n');
        if (!line_end || line_end > frontmatter_end) {
            line_end = frontmatter_end;
        }

        size_t line_len = (size_t)(line_end - cursor);
        if (line_len > 0 && cursor[line_len - 1] == '\r') {
            line_len--;
        }

        char line[512];
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, cursor, line_len);
        line[line_len] = '\0';
        trim_inplace(line);

        if (line[0] == '\0') {
            cursor = (line_end < frontmatter_end) ? line_end + 1 : line_end;
            continue;
        }

        if (list_mode && line[0] == '-') {
            add_required_tool(doc, line + 1);
            cursor = (line_end < frontmatter_end) ? line_end + 1 : line_end;
            continue;
        }

        list_mode = false;

        char *colon = strchr(line, ':');
        if (!colon) {
            cursor = (line_end < frontmatter_end) ? line_end + 1 : line_end;
            continue;
        }

        *colon = '\0';
        char *key = line;
        char *value = colon + 1;
        trim_inplace(key);
        trim_inplace(value);

        if (strcmp(key, "name") == 0) {
            snprintf(doc->name, sizeof(doc->name), "%s", value);
        } else if (strcmp(key, "description") == 0) {
            snprintf(doc->description, sizeof(doc->description), "%s", value);
        } else if (strcmp(key, "required_tools") == 0) {
            if (value[0] == '\0') {
                list_mode = true;
            } else {
                parse_required_tools_csv(doc, value);
            }
        }

        cursor = (line_end < frontmatter_end) ? line_end + 1 : line_end;
    }

    trim_inplace(doc->name);
    trim_inplace(doc->description);

    doc->body = frontmatter_end;
    while (doc->body[0] == '\n' || doc->body[0] == '\r') {
        doc->body++;
    }

    if (!doc->name[0] || !doc->description[0]) {
        return false;
    }

    return true;
}

static bool tool_available(const char *tool,
                           const char *const *tool_names,
                           size_t tool_count)
{
    if (!tool || !tool[0]) return false;

    for (size_t i = 0; i < tool_count; i++) {
        if (tool_names[i] && strcmp(tool_names[i], tool) == 0) {
            return true;
        }
    }
    return false;
}

mimi_err_t skills_loader_build_prompt(const skills_loader_options_t *opts,
                                      const char *const *tool_names,
                                      size_t tool_count,
                                      char *out,
                                      size_t out_size,
                                      skills_loader_result_t *result)
{
    if (!out || out_size == 0) return MIMI_ERR_INVALID_ARG;

    out[0] = '\0';
    if (result) {
        result->loaded = 0;
        result->skipped = 0;
    }

    if (!opts || !opts->enabled) {
        return MIMI_OK;
    }

    if (!opts->skills_dir || !opts->skills_dir[0] ||
        !opts->skill_entries || opts->skill_entry_count == 0) {
        ESP_LOGI(TAG, "skill-load-skip reason=no-config");
        return MIMI_OK;
    }

    size_t max_loaded = opts->max_loaded > 0 ? opts->max_loaded : 4;
    size_t per_skill_bytes = opts->per_skill_bytes > 0 ? opts->per_skill_bytes : 1200;
    size_t total_bytes = opts->total_bytes > 0 ? opts->total_bytes : 4096;

    if (max_loaded > opts->skill_entry_count) {
        max_loaded = opts->skill_entry_count;
    }

    size_t off = 0;
    size_t total_used = 0;

    for (size_t i = 0; i < opts->skill_entry_count; i++) {
        if (result && result->loaded >= max_loaded) break;

        const char *entry = opts->skill_entries[i];
        if (!entry || !entry[0]) {
            if (result) result->skipped++;
            continue;
        }

        char rel_path[512];
        if (!validate_skill_entry(entry, rel_path, sizeof(rel_path))) {
            ESP_LOGW(TAG, "skill-load-fail reason=invalid-path entry=%s", entry);
            if (result) result->skipped++;
            continue;
        }

        char full_path[1536];
        snprintf(full_path, sizeof(full_path), "%s/%s", opts->skills_dir, rel_path);

        char *file_text = read_text_file(full_path);
        if (!file_text) {
            ESP_LOGW(TAG, "skill-load-fail reason=missing-file entry=%s path=%s", entry, full_path);
            if (result) result->skipped++;
            continue;
        }

        skill_doc_t doc;
        if (!parse_frontmatter(file_text, &doc)) {
            ESP_LOGW(TAG, "skill-load-fail reason=invalid-frontmatter path=%s", full_path);
            free(file_text);
            if (result) result->skipped++;
            continue;
        }

        bool missing_required_tool = false;
        for (size_t t = 0; t < doc.required_count; t++) {
            if (!tool_available(doc.required_tools[t], tool_names, tool_count)) {
                ESP_LOGW(TAG,
                         "skill-load-fail reason=missing-required-tool skill=%s tool=%s",
                         doc.name,
                         doc.required_tools[t]);
                missing_required_tool = true;
                break;
            }
        }

        if (missing_required_tool) {
            free(file_text);
            if (result) result->skipped++;
            continue;
        }

        if (total_used >= total_bytes) {
            free(file_text);
            break;
        }

        char block[8192];
        block[0] = '\0';
        size_t boff = 0;

        boff = append_fmt(block, sizeof(block), boff, "### %s\n", doc.name);
        boff = append_fmt(block, sizeof(block), boff, "Description: %s\n", doc.description);

        if (doc.required_count > 0) {
            boff = append_fmt(block, sizeof(block), boff, "Required tools: ");
            for (size_t t = 0; t < doc.required_count; t++) {
                boff = append_fmt(block, sizeof(block), boff,
                                  "%s%s",
                                  doc.required_tools[t],
                                  (t + 1 < doc.required_count) ? ", " : "");
            }
            boff = append_fmt(block, sizeof(block), boff, "\n");
        }

        boff = append_fmt(block, sizeof(block), boff, "Instructions:\n");

        size_t body_len = doc.body ? strlen(doc.body) : 0;
        if (body_len > per_skill_bytes) {
            body_len = per_skill_bytes;
        }

        boff = append_bytes(block, sizeof(block), boff, doc.body ? doc.body : "", body_len);
        boff = append_fmt(block, sizeof(block), boff, "\n\n");

        size_t remain = total_bytes - total_used;
        size_t to_copy = boff;
        if (to_copy > remain) {
            to_copy = remain;
        }

        if (to_copy > 0) {
            if (off == 0) {
                off = append_fmt(out, out_size, off, "## Skills\n\n");
            }
            off = append_bytes(out, out_size, off, block, to_copy);
            total_used += to_copy;
            if (result) result->loaded++;

            ESP_LOGI(TAG,
                     "skill-load success skill=%s path=%s bytes=%u",
                     doc.name,
                     full_path,
                     (unsigned)to_copy);
        } else if (result) {
            result->skipped++;
        }

        free(file_text);
    }

    return MIMI_OK;
}
