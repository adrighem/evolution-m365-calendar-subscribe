#include "calendar-actions.h"
#include "calendar-manager.h"
#include "calendar-ui.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <shell/e-shell-view.h>

static void
calendar_subscribe (GtkWindow *parent, const gchar *email)
{
    ESource *ews_source = m365_calendar_get_ews_source ();

    if (!ews_source) {
        g_warning ("M365 Calendar Subscribe: Could not find an enabled EWS or Microsoft 365 mail account.");
        e_notice (parent, GTK_MESSAGE_ERROR, _("No EWS Account"),
                  _("Could not find an enabled EWS or Microsoft 365 mail account to perform the subscription."));
        return;
    }

    g_debug ("M365 Calendar Subscribe: Using EWS mail source '%s' (UID: %s) for subscription",
             e_source_get_display_name (ews_source), e_source_get_uid (ews_source));

    GError *error = NULL;
    if (m365_calendar_subscribe (ews_source, email, &error)) {
        g_debug ("M365 Calendar Subscribe: Subscription request for '%s' processed successfully.", email);
    } else {
        g_warning ("M365 Calendar Subscribe: Subscription failed: %s", error ? error->message : "Unknown error");
        e_notice (parent, GTK_MESSAGE_ERROR, _("Subscription Failed"),
                  _("Could not subscribe to the calendar: %s"), error ? error->message : _("Unknown error"));
        g_clear_error (&error);
    }

    g_object_unref (ews_source);
}

void
action_calendar_quick_subscribe_cb (EUIAction *action, GVariant *parameter, gpointer user_data)
{
    EShellView *shell_view = E_SHELL_VIEW (user_data);
    GtkWindow *parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (shell_view)));
    GtkWidget *dialog = calendar_create_quick_subscribe_dialog (parent);

    gint response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response == GTK_RESPONSE_ACCEPT) {
        gchar *email = g_object_get_data (G_OBJECT (dialog), "selected-email");
        if (email) {
            calendar_subscribe (parent, email);
        } else {
            GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
            const gchar *text = gtk_entry_get_text (GTK_ENTRY (search_entry));
            if (text && *text)
                calendar_subscribe (parent, text);
        }
    }

    gtk_widget_destroy (dialog);
}

