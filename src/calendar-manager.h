#ifndef CALENDAR_MANAGER_H
#define CALENDAR_MANAGER_H

#include <glib.h>
#include <libedataserver/libedataserver.h>

G_BEGIN_DECLS

#define EWS_FOLDER_TYPE_CALENDAR 2

typedef struct {
    gchar *display_name;
    gchar *email;
} CalendarContact;

typedef void (*CalendarContactCallback) (const GSList *contacts, gpointer user_data);

void m365_calendar_search_contacts (const gchar *search_text,
                                    CalendarContactCallback callback,
                                    gpointer user_data);

void m365_calendar_load_all_contacts (CalendarContactCallback callback,
                                      gpointer user_data);

ESource * m365_calendar_get_ews_source (void);

gboolean m365_calendar_subscribe (ESource *ews_source,
                                  const gchar *email,
                                  GError **error);

G_END_DECLS

#endif /* CALENDAR_MANAGER_H */

