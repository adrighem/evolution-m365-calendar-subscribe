// SPDX-License-Identifier: GPL-3.0-only

#include "calendar-actions.h"
#include "calendar-manager.h"
#include "calendar-ui.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

typedef struct {
    GtkWindow *parent;
    GtkWidget *dialog;
    ESource *ews_source;
    GCancellable *cancellable;
} SubscribeContext;

static void
on_subscribe_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
    SubscribeContext *ctx = (SubscribeContext *) user_data;

    if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT) {
        if (ctx->cancellable)
            g_cancellable_cancel (ctx->cancellable);
    }
}

static void
on_subscribe_dialog_destroyed (GtkWidget *widget, gpointer user_data)
{
    SubscribeContext *ctx = (SubscribeContext *) user_data;

    if (ctx->cancellable)
        g_cancellable_cancel (ctx->cancellable);
}

static void
on_subscribe_async_ready (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    SubscribeContext *ctx = (SubscribeContext *) user_data;
    ESource *ews_source = E_SOURCE (source_object);
    g_autoptr(GError) error = NULL;

    gboolean success = m365_calendar_subscribe_finish (ews_source, res, &error);

    if (success) {
        g_debug ("M365 Calendar Subscribe: Subscription processed successfully.");
    } else {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_warning ("M365 Calendar Subscribe: Subscription failed: %s", error ? error->message : "Unknown error");
            if (ctx->parent) {
                e_notice (ctx->parent, GTK_MESSAGE_ERROR, _("Subscription Failed"),
                          _("Could not subscribe to the calendar: %s"), error ? error->message : _("Unknown error"));
            }
        }
    }

    if (ctx->parent) {
        g_object_remove_weak_pointer (G_OBJECT (ctx->parent), (gpointer *) &ctx->parent);
        ctx->parent = NULL;
    }

    if (ctx->dialog) {
        GtkWidget *dialog = ctx->dialog;
        g_object_remove_weak_pointer (G_OBJECT (dialog), (gpointer *) &ctx->dialog);
        ctx->dialog = NULL;

        g_signal_handlers_disconnect_by_func (dialog, G_CALLBACK (on_subscribe_dialog_response), ctx);
        g_signal_handlers_disconnect_by_func (dialog, G_CALLBACK (on_subscribe_dialog_destroyed), ctx);
        gtk_widget_destroy (dialog);
    }

    g_clear_object (&ctx->cancellable);
    g_object_unref (ctx->ews_source);
    g_free (ctx);
}

static void on_quick_subscribe_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data);

static void
calendar_subscribe_async_handler (GtkWindow *parent, GtkWidget *dialog, const gchar *email)
{
    ESource *ews_source = m365_calendar_get_ews_source ();

    if (!ews_source) {
        g_warning ("M365 Calendar Subscribe: Could not find an enabled EWS or Microsoft 365 mail account.");
        e_notice (parent, GTK_MESSAGE_ERROR, _("No EWS Account"),
                  _("Could not find an enabled EWS or Microsoft 365 mail account to perform the subscription."));
        gtk_widget_destroy (dialog);
        return;
    }

    g_debug ("M365 Calendar Subscribe: Using EWS mail source '%s' (UID: %s) for async subscription",
             e_source_get_display_name (ews_source), e_source_get_uid (ews_source));

    // Disconnect the initial dialog response handler
    g_signal_handlers_disconnect_by_func (dialog, G_CALLBACK (on_quick_subscribe_dialog_response), parent);

    SubscribeContext *ctx = g_new0 (SubscribeContext, 1);
    ctx->cancellable = g_cancellable_new ();
    ctx->ews_source = ews_source;

    if (parent) {
        ctx->parent = parent;
        g_object_add_weak_pointer (G_OBJECT (parent), (gpointer *) &ctx->parent);
    }

    if (dialog) {
        ctx->dialog = dialog;
        g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &ctx->dialog);
        g_signal_connect (dialog, "response", G_CALLBACK (on_subscribe_dialog_response), ctx);
        g_signal_connect (dialog, "destroy", G_CALLBACK (on_subscribe_dialog_destroyed), ctx);
    }

    calendar_dialog_set_busy (dialog, TRUE);

    m365_calendar_subscribe_async (ews_source, email, ctx->cancellable, on_subscribe_async_ready, ctx);
}

static void
on_quick_subscribe_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
    GtkWindow *parent = GTK_WINDOW (user_data);

    if (response_id == GTK_RESPONSE_ACCEPT) {
        const gchar *email = g_object_get_data (G_OBJECT (dialog), "selected-email");
        if (!email || !*email) {
            GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
            if (search_entry)
                email = gtk_entry_get_text (GTK_ENTRY (search_entry));
        }

        if (email && *email) {
            calendar_subscribe_async_handler (parent, GTK_WIDGET (dialog), email);
            return;
        }
    }

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
action_calendar_quick_subscribe_cb (EUIAction *action, GVariant *parameter, gpointer user_data)
{
    EShellView *shell_view = E_SHELL_VIEW (user_data);
    EShellWindow *shell_window = e_shell_view_get_shell_window (shell_view);
    GtkWindow *parent = GTK_WINDOW (shell_window);
    GtkWidget *dialog = calendar_create_quick_subscribe_dialog (parent);

    g_signal_connect (dialog, "response", G_CALLBACK (on_quick_subscribe_dialog_response), parent);
    gtk_window_present (GTK_WINDOW (dialog));
}
