#include "calendar-ui.h"
#include "calendar-manager.h"
#include "string-utils.h"

#include <glib/gi18n.h>

static gboolean
contact_filter_func (GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
    GtkTreeModel *model = gtk_entry_completion_get_model (completion);
    gchar *full_string = NULL;
    gboolean match = FALSE;

    gtk_tree_model_get (model, iter, 0, &full_string, -1);

    if (full_string) {
        match = calendar_fuzzy_match (key, full_string);
        g_free (full_string);
    }

    return match;
}

static gboolean
calendar_search_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    if (event->keyval == GDK_KEY_Escape) {
        gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_CANCEL);
        return TRUE;
    }
    return FALSE;
}

static void
calendar_search_results_cb (const GSList *contacts, gpointer user_data)
{
    GtkWidget *dialog = user_data;
    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
    GtkEntryCompletion *completion = gtk_entry_get_completion (GTK_ENTRY (search_entry));
    GtkListStore *store = GTK_LIST_STORE (gtk_entry_completion_get_model (completion));
    const GSList *l;

    g_debug ("M365 Calendar Subscribe: Received %u contacts for pre-load", g_slist_length ((GSList *)contacts));

    gtk_list_store_clear (store);

    for (l = contacts; l; l = l->next) {
        CalendarContact *contact = l->data;
        GtkTreeIter iter;
        gchar *full_string = g_strdup_printf ("%s, %s",
                                              contact->display_name ? contact->display_name : "",
                                              contact->email ? contact->email : "");

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, full_string,
                            1, contact->email,
                            -1);
        g_free (full_string);
    }

    g_object_set_data (G_OBJECT (dialog), "contacts-loaded", GINT_TO_POINTER (1));

    const gchar *current_text = gtk_entry_get_text (GTK_ENTRY (search_entry));
    if (current_text && current_text[0] != '\0')
        gtk_entry_completion_complete (completion);
}

static gboolean
calendar_search_match_selected_cb (GtkEntryCompletion *completion, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    GtkWidget *dialog = user_data;
    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
    gchar *full_string = NULL;
    gchar *email = NULL;

    gtk_tree_model_get (model, iter, 0, &full_string, 1, &email, -1);

    if (full_string) {
        gtk_entry_set_text (GTK_ENTRY (search_entry), full_string);
        g_free (full_string);
    }

    g_object_set_data_full (G_OBJECT (dialog), "selected-email", email, g_free);

    return TRUE;
}

GtkWidget *
calendar_create_quick_subscribe_dialog (GtkWindow *parent)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Quick Subscribe"),
                                                     parent,
                                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     _("_Cancel"), GTK_RESPONSE_CANCEL,
                                                     _("_Subscribe"), GTK_RESPONSE_ACCEPT,
                                                     NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    GtkWidget *grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
    gtk_box_pack_start (GTK_BOX (content_area), grid, TRUE, TRUE, 0);

    GtkWidget *label = gtk_label_new (_("Name or Email:"));
    gtk_widget_set_halign (label, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    GtkWidget *search_entry = gtk_entry_new ();
    gtk_widget_set_hexpand (search_entry, TRUE);
    gtk_widget_set_size_request (search_entry, 400, -1);
    gtk_grid_attach (GTK_GRID (grid), search_entry, 1, 0, 1, 1);

    g_signal_connect (search_entry, "key-press-event", G_CALLBACK (calendar_search_key_press_cb), dialog);

    GtkEntryCompletion *completion = gtk_entry_completion_new ();
    GtkListStore *store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

    gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
    gtk_entry_completion_set_text_column (completion, 0);
    gtk_entry_completion_set_inline_completion (completion, FALSE);
    gtk_entry_completion_set_popup_completion (completion, TRUE);
    gtk_entry_completion_set_minimum_key_length (completion, 1);
    gtk_entry_completion_set_match_func (completion, contact_filter_func, dialog, NULL);

    g_signal_connect (completion, "match-selected", G_CALLBACK (calendar_search_match_selected_cb), dialog);

    gtk_entry_set_completion (GTK_ENTRY (search_entry), completion);
    g_object_unref (store);
    g_object_unref (completion);

    gtk_widget_show_all (grid);

    g_object_set_data (G_OBJECT (dialog), "search-entry", search_entry);

    m365_calendar_load_all_contacts (calendar_search_results_cb, dialog);

    return dialog;
}

