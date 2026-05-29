#include "mock_nvs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_NS    8
#define MAX_KEYS  64
#define KEY_LEN   16
#define VAL_LEN   256  // doit couvrir config_machine_t et config_programme_t

typedef struct {
    char   key[KEY_LEN];
    char   val[VAL_LEN];
    size_t val_len;  // 0 = string, >0 = blob
    int    used;
} nvs_entry_t;

typedef struct {
    char        ns[16];
    nvs_entry_t entries[MAX_KEYS];
    int         used;
} nvs_ns_t;

static nvs_ns_t s_store[MAX_NS];

static nvs_ns_t *find_or_create_ns(const char *ns)
{
    for (int i = 0; i < MAX_NS; i++) {
        if (s_store[i].used && strcmp(s_store[i].ns, ns) == 0)
            return &s_store[i];
    }
    for (int i = 0; i < MAX_NS; i++) {
        if (!s_store[i].used) {
            strncpy(s_store[i].ns, ns, sizeof(s_store[i].ns) - 1);
            s_store[i].used = 1;
            return &s_store[i];
        }
    }
    return NULL;
}

static nvs_entry_t *find_entry(nvs_ns_t *n, const char *key)
{
    for (int i = 0; i < MAX_KEYS; i++) {
        if (n->entries[i].used && strcmp(n->entries[i].key, key) == 0)
            return &n->entries[i];
    }
    return NULL;
}

static nvs_entry_t *find_or_create_entry(nvs_ns_t *n, const char *key)
{
    nvs_entry_t *e = find_entry(n, key);
    if (e) return e;
    for (int i = 0; i < MAX_KEYS; i++) {
        if (!n->entries[i].used) {
            strncpy(n->entries[i].key, key, KEY_LEN - 1);
            n->entries[i].used = 1;
            return &n->entries[i];
        }
    }
    return NULL;
}

esp_err_t nvs_flash_init(void)  { return ESP_OK; }
esp_err_t nvs_flash_erase(void) { mock_nvs_reset(); return ESP_OK; }

esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *out)
{
    (void)mode;
    nvs_ns_t *n = find_or_create_ns(ns);
    if (!n) return ESP_FAIL;
    *out = (nvs_handle_t)n;
    return ESP_OK;
}

void      nvs_close(nvs_handle_t h)  { (void)h; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }

esp_err_t nvs_get_i32(nvs_handle_t h, const char *k, int32_t *out)
{
    nvs_entry_t *e = find_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    *out = (int32_t)atoi(e->val);
    return ESP_OK;
}
esp_err_t nvs_set_i32(nvs_handle_t h, const char *k, int32_t v)
{
    nvs_entry_t *e = find_or_create_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    snprintf(e->val, VAL_LEN, "%d", (int)v);
    return ESP_OK;
}

esp_err_t nvs_get_u32(nvs_handle_t h, const char *k, uint32_t *out)
{
    nvs_entry_t *e = find_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    *out = (uint32_t)atoll(e->val);
    return ESP_OK;
}
esp_err_t nvs_set_u32(nvs_handle_t h, const char *k, uint32_t v)
{
    nvs_entry_t *e = find_or_create_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    snprintf(e->val, VAL_LEN, "%u", v);
    return ESP_OK;
}

esp_err_t nvs_get_i8(nvs_handle_t h, const char *k, int8_t *out)
{
    nvs_entry_t *e = find_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    *out = (int8_t)atoi(e->val);
    return ESP_OK;
}
esp_err_t nvs_set_i8(nvs_handle_t h, const char *k, int8_t v)
{
    nvs_entry_t *e = find_or_create_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    snprintf(e->val, VAL_LEN, "%d", (int)v);
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *out)
{
    nvs_entry_t *e = find_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    *out = (uint8_t)atoi(e->val);
    return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t v)
{
    nvs_entry_t *e = find_or_create_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    snprintf(e->val, VAL_LEN, "%d", (int)v);
    return ESP_OK;
}

esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *out, size_t *len)
{
    nvs_entry_t *e = find_entry((nvs_ns_t *)h, k);
    if (!e || e->val_len > 0) return ESP_FAIL;  // blob is not a string
    size_t n = strlen(e->val) + 1;
    if (out == NULL) { if (len) *len = n; return ESP_OK; }
    if (len && *len < n) return ESP_FAIL;
    memcpy(out, e->val, n);
    return ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v)
{
    nvs_entry_t *e = find_or_create_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    strncpy(e->val, v, VAL_LEN - 1);
    e->val_len = 0;  // 0 = string
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t h, const char *k, void *out, size_t *len)
{
    nvs_entry_t *e = find_entry((nvs_ns_t *)h, k);
    if (!e || e->val_len == 0) return ESP_ERR_NVS_NOT_FOUND;
    if (out == NULL) { if (len) *len = e->val_len; return ESP_OK; }
    if (len && *len < e->val_len) return ESP_FAIL;
    memcpy(out, e->val, e->val_len);
    if (len) *len = e->val_len;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *k, const void *v, size_t len)
{
    if (len > VAL_LEN) return ESP_FAIL;
    nvs_entry_t *e = find_or_create_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    memcpy(e->val, v, len);
    e->val_len = len;
    return ESP_OK;
}

esp_err_t nvs_erase_key(nvs_handle_t h, const char *k)
{
    nvs_entry_t *e = find_entry((nvs_ns_t *)h, k);
    if (!e) return ESP_ERR_NVS_NOT_FOUND;
    e->used = 0;
    return ESP_OK;
}

esp_err_t nvs_erase_all(nvs_handle_t h)
{
    nvs_ns_t *n = (nvs_ns_t *)h;
    for (int i = 0; i < MAX_KEYS; i++) n->entries[i].used = 0;
    return ESP_OK;
}

void mock_nvs_reset(void)
{
    memset(s_store, 0, sizeof(s_store));
}
