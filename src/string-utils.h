#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

gboolean calendar_fuzzy_match (const gchar *pattern, const gchar *str);

G_END_DECLS

#endif /* STRING_UTILS_H */

