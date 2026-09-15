// SPDX-License-Identifier: GPL-3.0-only

#include <glib.h>
#include <gtk/gtk.h>

#include "../src/calendar-ui.h"

static void
test_calendar_quick_subscribe_dialog_structure (void)
{
    GtkWidget *dialog = calendar_create_quick_subscribe_dialog (NULL);
    g_assert_true (GTK_IS_DIALOG (dialog));

    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
    GtkWidget *email_entry = g_object_get_data (G_OBJECT (dialog), "email-entry");
    GtkWidget *name_entry = g_object_get_data (G_OBJECT (dialog), "name-entry");

    g_assert_nonnull (search_entry);
    g_assert_null (email_entry);
    g_assert_null (name_entry);

    gtk_widget_destroy (dialog);
}

static void
test_calendar_quick_subscribe_search_completion (void)
{
    GtkWidget *dialog = calendar_create_quick_subscribe_dialog (NULL);

    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
    GtkEntryCompletion *completion = gtk_entry_get_completion (GTK_ENTRY (search_entry));

    g_assert_nonnull (completion);

    gtk_widget_destroy (dialog);
}

static void
test_calendar_quick_subscribe_busy_state (void)
{
    GtkWidget *dialog = calendar_create_quick_subscribe_dialog (NULL);
    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");

    g_assert_true (gtk_widget_get_sensitive (search_entry));

    calendar_dialog_set_busy (dialog, TRUE);
    g_assert_false (gtk_widget_get_sensitive (search_entry));

    calendar_dialog_set_busy (dialog, FALSE);
    g_assert_true (gtk_widget_get_sensitive (search_entry));

    gtk_widget_destroy (dialog);
}

static void
test_calendar_quick_subscribe_buffered_input (void)
{
    GtkWidget *dialog = calendar_create_quick_subscribe_dialog (NULL);

    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
    gtk_entry_set_text (GTK_ENTRY (search_entry), "test input");
    g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (search_entry)), ==, "test input");

    gtk_widget_destroy (dialog);
}

int
main (int argc, char *argv[])
{
    if (!gtk_init_check (&argc, &argv)) {
        g_printerr ("Cannot initialize GTK, skipping UI tests.\n");
        return 0;
    }

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/ui/calendar/quick-subscribe-dialog", test_calendar_quick_subscribe_dialog_structure);
    g_test_add_func ("/ui/calendar/quick-subscribe-search-completion", test_calendar_quick_subscribe_search_completion);
    g_test_add_func ("/ui/calendar/quick-subscribe-busy-state", test_calendar_quick_subscribe_busy_state);
    g_test_add_func ("/ui/calendar/quick-subscribe-buffered-input", test_calendar_quick_subscribe_buffered_input);

    return g_test_run ();
}
