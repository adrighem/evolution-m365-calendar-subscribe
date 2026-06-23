#include "string-utils.h"

gboolean
calendar_fuzzy_match (const gchar *pattern, const gchar *str)
{
    if (!pattern || !str)
        return FALSE;

    if (!*pattern)
        return TRUE;

    gchar *pattern_lower = g_utf8_strdown (pattern, -1);
    gchar *str_lower = g_utf8_strdown (str, -1);

    const gchar *p = pattern_lower;
    const gchar *s = str_lower;

    while (*p && *s) {
        if (*p == *s)
            p++;
        s++;
    }

    gboolean matched = (*p == '\0');

    g_free (pattern_lower);
    g_free (str_lower);

    return matched;
}

