// SPDX-License-Identifier: GPL-3.0-only

#include "calendar-ui.h"
#include "calendar-manager.h"
#include "string-utils.h"

#include <glib/gi18n.h>

typedef struct {
    GtkWidget *dialog;
    GtkWidget *search_entry;
    GtkWidget *spinner;
    GtkWidget *subscribe_button;
    guint debounce_id;
} DialogState;

static void
dialog_state_free (gpointer data)
{
    DialogState *state = (DialogState *) data;
    if (!state)
        return;

    if (state->debounce_id > 0) {
        g_source_remove (state->debounce_id);
        state->debounce_id = 0;
    }
    g_free (state);
}

static gboolean
contact_filter_func (GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
    GtkTreeModel *model = gtk_entry_completion_get_model (completion);
    g_autofree gchar *full_string = NULL;
    gboolean match = FALSE;

    gtk_tree_model_get (model, iter, 0, &full_string, -1);

    if (full_string)
        match = calendar_fuzzy_match (key, full_string);

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
    GtkWidget *dialog = GTK_WIDGET (user_data);
    if (!GTK_IS_WIDGET (dialog))
        return;

    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
    if (!search_entry)
        return;

    GtkEntryCompletion *completion = gtk_entry_get_completion (GTK_ENTRY (search_entry));
    if (!completion)
        return;

    GtkListStore *store = GTK_LIST_STORE (gtk_entry_completion_get_model (completion));
    gtk_list_store_clear (store);

    for (const GSList *l = contacts; l; l = l->next) {
        CalendarContact *contact = l->data;
        GtkTreeIter iter;
        g_autofree gchar *full_string = g_strdup_printf ("%s, %s",
                                                          contact->display_name ? contact->display_name : "",
                                                          contact->email ? contact->email : "");

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, full_string,
                            1, contact->email,
                            -1);
    }

    g_object_set_data (G_OBJECT (dialog), "contacts-loaded", GINT_TO_POINTER (1));

    const gchar *current_text = gtk_entry_get_text (GTK_ENTRY (search_entry));
    if (current_text && current_text[0] != '\0')
        gtk_entry_completion_complete (completion);
}

static gboolean
on_search_debounced (gpointer user_data)
{
    DialogState *state = (DialogState *) user_data;
    state->debounce_id = 0;

    const gchar *text = gtk_entry_get_text (GTK_ENTRY (state->search_entry));
    if (text && strlen (text) >= 2) {
        m365_calendar_search_contacts (text, calendar_search_results_cb, state->dialog);
    }

    return G_SOURCE_REMOVE;
}

static void
on_search_entry_changed (GtkEditable *editable, gpointer user_data)
{
    DialogState *state = (DialogState *) user_data;

    if (state->debounce_id > 0)
        g_source_remove (state->debounce_id);

    state->debounce_id = g_timeout_add (300, on_search_debounced, state);
}

static gboolean
calendar_search_match_selected_cb (GtkEntryCompletion *completion, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    GtkWidget *dialog = GTK_WIDGET (user_data);
    GtkWidget *search_entry = g_object_get_data (G_OBJECT (dialog), "search-entry");
    g_autofree gchar *full_string = NULL;
    g_autofree gchar *email = NULL;

    gtk_tree_model_get (model, iter, 0, &full_string, 1, &email, -1);

    if (full_string && search_entry)
        gtk_entry_set_text (GTK_ENTRY (search_entry), full_string);

    g_object_set_data_full (G_OBJECT (dialog), "selected-email", g_strdup (email), g_free);

    return TRUE;
}

void
calendar_dialog_set_busy (GtkWidget *dialog, gboolean busy)
{
    if (!dialog || !GTK_IS_DIALOG (dialog))
        return;

    DialogState *state = g_object_get_data (G_OBJECT (dialog), "dialog-state");
    if (!state)
        return;

    gtk_widget_set_sensitive (state->search_entry, !busy);
    if (state->subscribe_button)
        gtk_widget_set_sensitive (state->subscribe_button, !busy);

    if (busy) {
        gtk_widget_show (state->spinner);
        gtk_spinner_start (GTK_SPINNER (state->spinner));
    } else {
        gtk_spinner_stop (GTK_SPINNER (state->spinner));
        gtk_widget_hide (state->spinner);
    }
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

    GtkWidget *spinner = gtk_spinner_new ();
    gtk_widget_set_no_show_all (spinner, TRUE);
    gtk_grid_attach (GTK_GRID (grid), spinner, 2, 0, 1, 1);

    GtkWidget *subscribe_button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

    DialogState *state = g_new0 (DialogState, 1);
    state->dialog = dialog;
    state->search_entry = search_entry;
    state->spinner = spinner;
    state->subscribe_button = subscribe_button;
    g_object_set_data_full (G_OBJECT (dialog), "dialog-state", state, dialog_state_free);

    g_signal_connect (search_entry, "key-press-event", G_CALLBACK (calendar_search_key_press_cb), dialog);
    g_signal_connect (search_entry, "changed", G_CALLBACK (on_search_entry_changed), state);

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

    return dialog;
}
