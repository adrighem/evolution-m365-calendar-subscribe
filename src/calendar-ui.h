// SPDX-License-Identifier: GPL-3.0-only

#ifndef CALENDAR_UI_H
#define CALENDAR_UI_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget * calendar_create_quick_subscribe_dialog (GtkWindow *parent);
void        calendar_dialog_set_busy               (GtkWidget *dialog, gboolean busy);

G_END_DECLS

#endif /* CALENDAR_UI_H */
