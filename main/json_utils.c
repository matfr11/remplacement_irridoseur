#include "json_utils.h"

#include <stdio.h>
#include <string.h>

// =============================================================================
// json_utils.c — Parsing/échappement JSON minimal (extrait de webserver.c)
// =============================================================================

void json_escape(const char *src, char *dst, size_t dst_len)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_len - 2; i++) {
        if (src[i] == '"' || src[i] == '\\') dst[j++] = '\\';
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

bool json_parse_float(const char *data, const char *key, float *out)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(data, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    return sscanf(p, "%f", out) == 1;
}

bool json_parse_int(const char *data, const char *key, int *out)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(data, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    return sscanf(p, "%d", out) == 1;
}

bool json_parse_bool(const char *data, const char *key, bool *out)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(data, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    if (strncmp(p, "true",  4) == 0) { *out = true;  return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

void json_parse_string(const char *data, const char *key, char *out, size_t out_len)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(data, search);
    if (!p) return;
    p += strlen(search);
    while (*p == ' ') p++;   // tolère "key": "val" — cohérent avec les parseurs float/int/bool
    if (*p != '"') return;   // valeur non-string
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_len - 1) out[i++] = *p++;
    out[i] = '\0';
}
