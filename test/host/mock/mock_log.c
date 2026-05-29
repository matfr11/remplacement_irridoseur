#include "mock_log.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static char s_log_buf[4096];
static int  s_log_len = 0;

void mock_log_append(const char *tag, char level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rem = (int)sizeof(s_log_buf) - s_log_len - 1;
    if (rem > 0) {
        int n = snprintf(s_log_buf + s_log_len, (size_t)rem, "[%c][%s] ", level, tag);
        if (n > 0) s_log_len += n;
        rem = (int)sizeof(s_log_buf) - s_log_len - 1;
    }
    if (rem > 0) {
        int n = vsnprintf(s_log_buf + s_log_len, (size_t)rem, fmt, ap);
        if (n > 0) s_log_len += n;
    }
    va_end(ap);
    if (s_log_len < (int)sizeof(s_log_buf) - 1)
        s_log_buf[s_log_len++] = '\n';
}

bool mock_log_contains(const char *substr)
{
    return strstr(s_log_buf, substr) != NULL;
}

void mock_log_reset(void)
{
    s_log_len    = 0;
    s_log_buf[0] = '\0';
}
