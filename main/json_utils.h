#pragma once

#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// json_utils.h — Parsing/échappement JSON minimal sans bibliothèque
// Extrait de webserver.c pour être testable sur host (suite test/host).
// Limites assumées : pas de JSON imbriqué, clés uniques, valeurs simples.
// =============================================================================

// Échappe " et \ pour insertion dans une chaîne JSON. dst_len >= 2.
void json_escape(const char *src, char *dst, size_t dst_len);

// Cherchent "key": dans data, tolèrent les espaces après le ':'.
// Retournent false (ou laissent out intact pour string) si clé absente
// ou valeur du mauvais type.
bool json_parse_float(const char *data, const char *key, float *out);
bool json_parse_int(const char *data, const char *key, int *out);
bool json_parse_bool(const char *data, const char *key, bool *out);
void json_parse_string(const char *data, const char *key, char *out, size_t out_len);
