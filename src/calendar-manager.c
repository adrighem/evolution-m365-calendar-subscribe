#include "calendar-manager.h"

#include <camel/camel.h>
#include <e-util/e-util.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <glib/gi18n.h>
#include <libebook/libebook.h>
#include <libemail-engine/libemail-engine.h>
#include <mail/e-mail-backend.h>
#include <shell/e-shell.h>
#include <string.h>

typedef struct {
    gchar *search_text;
    CalendarContactCallback callback;
    gpointer user_data;
    GSList *contacts;
    gint pending_books;
} CalendarSearchContext;

static void
calendar_contact_free (CalendarContact *contact)
{
    if (!contact)
        return;

    g_free (contact->display_name);
    g_free (contact->email);
    g_free (contact);
}

static void
calendar_search_context_free (CalendarSearchContext *ctx)
{
    g_free (ctx->search_text);
    g_slist_free_full (ctx->contacts, (GDestroyNotify) calendar_contact_free);
    g_free (ctx);
}

static gboolean
calendar_source_is_ews_or_m365_address_book (ESource *source)
{
    if (!e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
        return FALSE;

    ESourceAddressBook *address_book = e_source_get_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
    const gchar *backend_name = e_source_backend_get_backend_name (E_SOURCE_BACKEND (address_book));

    return backend_name &&
           (g_strcmp0 (backend_name, "ews") == 0 ||
            g_strcmp0 (backend_name, "microsoft365") == 0);
}

static void
calendar_search_got_contacts_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    EBookClient *book_client = E_BOOK_CLIENT (source_object);
    CalendarSearchContext *ctx = user_data;
    GSList *contacts = NULL;
    GError *error = NULL;

    if (e_book_client_get_contacts_finish (book_client, res, &contacts, &error)) {
        for (GSList *l = contacts; l; l = l->next) {
            EContact *contact = E_CONTACT (l->data);
            CalendarContact *calendar_contact = g_new0 (CalendarContact, 1);

            calendar_contact->display_name = g_strdup (e_contact_get_const (contact, E_CONTACT_FULL_NAME));
            calendar_contact->email = g_strdup (e_contact_get_const (contact, E_CONTACT_EMAIL_1));

            if (calendar_contact->email) {
                ctx->contacts = g_slist_prepend (ctx->contacts, calendar_contact);
            } else {
                calendar_contact_free (calendar_contact);
            }
        }

        g_slist_free_full (contacts, g_object_unref);
    } else {
        g_warning ("M365 Calendar Subscribe: Failed to get contacts: %s", error ? error->message : "Unknown error");
        g_clear_error (&error);
    }

    ctx->pending_books--;
    if (ctx->pending_books <= 0) {
        ctx->contacts = g_slist_reverse (ctx->contacts);
        ctx->callback (ctx->contacts, ctx->user_data);
        calendar_search_context_free (ctx);
    }
}

static void
calendar_search_connected_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    CalendarSearchContext *ctx = user_data;
    GError *error = NULL;
    EBookClient *book_client = (EBookClient *) e_book_client_connect_finish (res, &error);

    if (book_client) {
        EBookQuery *query = e_book_query_any_field_contains (ctx->search_text);
        gchar *sexp = e_book_query_to_string (query);

        e_book_client_get_contacts (book_client, sexp, NULL, calendar_search_got_contacts_cb, ctx);

        g_free (sexp);
        e_book_query_unref (query);
        g_object_unref (book_client);
    } else {
        g_warning ("M365 Calendar Subscribe: Failed to connect to book: %s", error ? error->message : "Unknown error");
        g_clear_error (&error);

        ctx->pending_books--;
        if (ctx->pending_books <= 0) {
            ctx->callback (ctx->contacts, ctx->user_data);
            calendar_search_context_free (ctx);
        }
    }
}

static void
calendar_load_all_connected_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    CalendarSearchContext *ctx = user_data;
    GError *error = NULL;
    EBookClient *book_client = (EBookClient *) e_book_client_connect_finish (res, &error);

    if (book_client) {
        e_book_client_get_contacts (book_client, "(contains \"x-evolution-any-field\" \"\")", NULL, calendar_search_got_contacts_cb, ctx);
        g_object_unref (book_client);
    } else {
        g_warning ("M365 Calendar Subscribe: Failed to connect to book for bulk load: %s", error ? error->message : "Unknown error");
        g_clear_error (&error);

        ctx->pending_books--;
        if (ctx->pending_books <= 0) {
            ctx->callback (ctx->contacts, ctx->user_data);
            calendar_search_context_free (ctx);
        }
    }
}

static GList *
calendar_list_ews_or_m365_address_books (void)
{
    EShell *shell = e_shell_get_default ();
    if (!E_IS_SHELL (shell))
        return NULL;

    ESourceRegistry *registry = e_shell_get_registry (shell);
    GList *sources = e_source_registry_list_sources (registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);
    GList *filtered_sources = NULL;

    for (GList *l = sources; l; l = l->next) {
        ESource *source = E_SOURCE (l->data);
        if (calendar_source_is_ews_or_m365_address_book (source)) {
            g_debug ("M365 Calendar Subscribe: Processing address book source '%s' (UID: %s)",
                     e_source_get_display_name (source), e_source_get_uid (source));
            filtered_sources = g_list_append (filtered_sources, g_object_ref (source));
        }
    }

    g_list_free_full (sources, g_object_unref);

    return filtered_sources;
}

void
m365_calendar_search_contacts (const gchar *search_text, CalendarContactCallback callback, gpointer user_data)
{
    GList *filtered_sources = calendar_list_ews_or_m365_address_books ();

    if (!filtered_sources) {
        g_warning ("M365 Calendar Subscribe: No EWS or Microsoft 365 address books available.");
        callback (NULL, user_data);
        return;
    }

    CalendarSearchContext *ctx = g_new0 (CalendarSearchContext, 1);
    ctx->search_text = g_strdup (search_text);
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->pending_books = g_list_length (filtered_sources);

    for (GList *l = filtered_sources; l; l = l->next) {
        ESource *source = E_SOURCE (l->data);
        e_book_client_connect (source, 30, NULL, calendar_search_connected_cb, ctx);
    }

    g_list_free_full (filtered_sources, g_object_unref);
}

void
m365_calendar_load_all_contacts (CalendarContactCallback callback, gpointer user_data)
{
    GList *filtered_sources = calendar_list_ews_or_m365_address_books ();

    if (!filtered_sources) {
        g_warning ("M365 Calendar Subscribe: No EWS or Microsoft 365 address books available.");
        callback (NULL, user_data);
        return;
    }

    CalendarSearchContext *ctx = g_new0 (CalendarSearchContext, 1);
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->pending_books = g_list_length (filtered_sources);

    for (GList *l = filtered_sources; l; l = l->next) {
        ESource *source = E_SOURCE (l->data);
        e_book_client_connect (source, 30, NULL, calendar_load_all_connected_cb, ctx);
    }

    g_list_free_full (filtered_sources, g_object_unref);
}

typedef enum {
    E_EWS_PERMISSION_USER_TYPE_NONE         = 0,
    E_EWS_PERMISSION_USER_TYPE_ANONYMOUS    = 1 << 1,
    E_EWS_PERMISSION_USER_TYPE_DEFAULT      = 1 << 2,
    E_EWS_PERMISSION_USER_TYPE_REGULAR      = 1 << 3
} EEwsPermissionUserType;

typedef struct {
    EEwsPermissionUserType user_type;
    gchar *display_name;
    gchar *primary_smtp;
    gchar *sid;
    guint32 rights;
} EEwsPermission;

static gboolean
ews_permissions_allow_read (GSList *perms, const gchar *our_email)
{
    EEwsPermission *default_perm = NULL;
    EEwsPermission *regular_perm = NULL;

    for (GSList *l = perms; l; l = l->next) {
        EEwsPermission *perm = l->data;

        if (perm->user_type == E_EWS_PERMISSION_USER_TYPE_DEFAULT) {
            default_perm = perm;
        } else if (perm->user_type == E_EWS_PERMISSION_USER_TYPE_REGULAR &&
                   our_email && perm->primary_smtp &&
                   g_ascii_strcasecmp (perm->primary_smtp, our_email) == 0) {
            regular_perm = perm;
        }
    }

    if (regular_perm)
        return (regular_perm->rights & 0x00000001) != 0;

    if (default_perm)
        return (default_perm->rights & 0x00000001) != 0;

    return FALSE;
}

ESource *
m365_calendar_get_ews_source (void)
{
    EShell *shell = e_shell_get_default ();
    if (!E_IS_SHELL (shell))
        return NULL;

    ESourceRegistry *registry = e_shell_get_registry (shell);
    GList *sources = e_source_registry_list_enabled (registry, NULL);
    ESource *ews_source = NULL;

    for (GList *l = sources; l; l = l->next) {
        ESource *source = E_SOURCE (l->data);
        ESourceBackend *backend = NULL;
        const gchar *backend_name = NULL;

        if (!e_source_has_extension (source, "Mail Account"))
            continue;

        if (e_source_has_extension (source, "Backend")) {
            backend = e_source_get_extension (source, "Backend");
            backend_name = e_source_backend_get_backend_name (backend);
        }

        if (!backend_name) {
            const gchar *parent_id = e_source_get_parent (source);
            if (parent_id && *parent_id) {
                ESource *parent_src = e_source_registry_ref_source (registry, parent_id);
                if (parent_src) {
                    if (e_source_has_extension (parent_src, "Collection")) {
                        ESourceBackend *collection_backend = e_source_get_extension (parent_src, "Collection");
                        backend_name = e_source_backend_get_backend_name (collection_backend);
                    }
                    g_object_unref (parent_src);
                }
            }
        }

        if (backend_name &&
            (g_strcmp0 (backend_name, "ews") == 0 ||
             g_strcmp0 (backend_name, "microsoft365") == 0)) {
            ews_source = g_object_ref (source);
            break;
        }
    }

    g_list_free_full (sources, g_object_unref);

    return ews_source;
}

typedef gpointer (*CamelEwsStoreGetConnFunc) (gpointer ews_store);
typedef gpointer (*EwsFolderIdNewFunc) (const gchar *id, const gchar *change_key, gboolean is_distinguished);
typedef void (*EwsFolderIdFreeFunc) (gpointer fid);
typedef gboolean (*EwsGetFolderInfoFunc) (gpointer cnc, gint pri, const gchar *mail_id, gpointer fid, gpointer *out_folder, GCancellable *cancellable, GError **error);
typedef gboolean (*EwsGetFolderPermissionsFunc) (gpointer cnc, gint pri, gpointer fid, GSList **out_permissions, GCancellable *cancellable, GError **error);
typedef void (*EwsPermissionsFreeFunc) (GSList *permissions);
typedef void (*EwsSetMailboxFunc) (gpointer cnc, const gchar *mailbox);
typedef const gchar * (*EwsGetMailboxFunc) (gpointer cnc);

typedef GType (*EwsFolderGetTypeFunc) (void);
typedef void (*EwsFolderSetIdFunc) (gpointer folder, gpointer fid);
typedef void (*EwsFolderSetNameFunc) (gpointer folder, const gchar *name);
typedef void (*EwsFolderSetFolderTypeFunc) (gpointer folder, gint type);
typedef void (*EwsFolderSetForeignMailFunc) (gpointer folder, const gchar *email);
typedef void (*EwsFolderSetForeignFunc) (gpointer folder, gboolean is_foreign);

/* Evolution EWS exports this symbol with "subscrive" in its name. */
typedef gboolean (*EwsSubscribeSyncFunc) (gpointer ews_store,
                                          gpointer folder,
                                          const gchar *user_displayname,
                                          const gchar *user_email,
                                          const gchar *fallback_folder_name,
                                          gboolean include_subfolders,
                                          GCancellable *cancellable,
                                          GError **error);

gboolean
m365_calendar_subscribe (ESource *ews_source, const gchar *email, GError **error)
{
    EShell *shell = e_shell_get_default ();
    EShellBackend *mail_backend;
    EMailSession *session;
    CamelService *service;
    GModule *ews_module = NULL;
    GModule *ews_lib = NULL;
    GModule *ews_priv = NULL;
    CamelEwsStoreGetConnFunc get_conn;
    EwsFolderIdNewFunc fid_new;
    EwsFolderIdFreeFunc fid_free;
    EwsGetFolderInfoFunc get_info;
    EwsGetFolderPermissionsFunc get_perms;
    EwsPermissionsFreeFunc perms_free;
    EwsSetMailboxFunc set_mailbox;
    EwsGetMailboxFunc get_mailbox;
    EwsFolderGetTypeFunc get_folder_type;
    EwsFolderSetIdFunc folder_set_id;
    EwsFolderSetNameFunc folder_set_name;
    EwsFolderSetFolderTypeFunc folder_set_folder_type;
    EwsFolderSetForeignMailFunc folder_set_foreign_mail;
    EwsFolderSetForeignFunc folder_set_foreign;
    EwsSubscribeSyncFunc subscribe_sync;
    gpointer conn = NULL;
    gpointer fid = NULL;
    gpointer folder = NULL;
    gchar *old_mailbox = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (E_IS_SOURCE (ews_source), FALSE);
    g_return_val_if_fail (email != NULL, FALSE);

    if (!E_IS_SHELL (shell))
        return FALSE;

    mail_backend = e_shell_get_backend_by_name (shell, "mail");
    if (!mail_backend)
        return FALSE;

    session = e_mail_backend_get_session (E_MAIL_BACKEND (mail_backend));
    if (!session)
        return FALSE;

    service = camel_session_ref_service ((CamelSession *) session, e_source_get_uid (ews_source));
    if (!service)
        return FALSE;

    ews_lib = g_module_open ("/usr/lib/evolution-ews/libevolution-ews.so", G_MODULE_BIND_LAZY);
    ews_priv = g_module_open ("/usr/lib/evolution-ews/libcamelews-priv.so", G_MODULE_BIND_LAZY);
    ews_module = g_module_open ("/usr/lib/evolution/modules/module-ews-configuration.so", G_MODULE_BIND_LAZY);

    if (!ews_lib || !ews_priv || !ews_module) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Could not load EWS libraries");
        goto cleanup;
    }

    g_module_symbol (ews_priv, "camel_ews_store_ref_connection", (gpointer *) &get_conn);
    g_module_symbol (ews_lib, "e_ews_folder_id_new", (gpointer *) &fid_new);
    g_module_symbol (ews_lib, "e_ews_folder_id_free", (gpointer *) &fid_free);
    g_module_symbol (ews_lib, "e_ews_connection_get_folder_info_sync", (gpointer *) &get_info);
    g_module_symbol (ews_lib, "e_ews_connection_get_folder_permissions_sync", (gpointer *) &get_perms);
    g_module_symbol (ews_lib, "e_ews_permissions_free", (gpointer *) &perms_free);
    g_module_symbol (ews_lib, "e_ews_connection_set_mailbox", (gpointer *) &set_mailbox);
    g_module_symbol (ews_lib, "e_ews_connection_get_mailbox", (gpointer *) &get_mailbox);
    g_module_symbol (ews_lib, "e_ews_folder_get_type", (gpointer *) &get_folder_type);
    g_module_symbol (ews_lib, "e_ews_folder_set_id", (gpointer *) &folder_set_id);
    g_module_symbol (ews_lib, "e_ews_folder_set_name", (gpointer *) &folder_set_name);
    g_module_symbol (ews_lib, "e_ews_folder_set_folder_type", (gpointer *) &folder_set_folder_type);
    g_module_symbol (ews_lib, "e_ews_folder_set_foreign_mail", (gpointer *) &folder_set_foreign_mail);
    g_module_symbol (ews_lib, "e_ews_folder_set_foreign", (gpointer *) &folder_set_foreign);
    g_module_symbol (ews_module, "e_ews_subscrive_foreign_folder_subscribe_sync", (gpointer *) &subscribe_sync);

    if (!get_conn || !fid_new || !get_info || !get_perms || !perms_free || !subscribe_sync ||
        !set_mailbox || !get_mailbox || !get_folder_type || !folder_set_id || !folder_set_name ||
        !folder_set_folder_type || !folder_set_foreign_mail || !folder_set_foreign) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Required EWS symbols not found");
        goto cleanup;
    }

    conn = get_conn (service);
    if (!conn) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Could not get EWS connection for store");
        goto cleanup;
    }

    old_mailbox = g_strdup (get_mailbox (conn));
    set_mailbox (conn, email);

    gboolean is_fallback = FALSE;
    fid = fid_new ("calendar", NULL, TRUE);

    if (!get_info (conn, G_PRIORITY_DEFAULT, email, fid, &folder, NULL, error)) {
        gchar *msg = (error && *error) ? (*error)->message : "";
        if (strstr (msg, "403") || strstr (msg, "Access Denied") ||
            strstr (msg, "Forbidden") || strstr (msg, "not found")) {
            if (error)
                g_clear_error (error);
            is_fallback = TRUE;
        } else {
            if (fid_free)
                fid_free (fid);
            goto cleanup;
        }
    } else {
        GSList *perms = NULL;
        GError *perm_error = NULL;

        if (!get_perms (conn, G_PRIORITY_DEFAULT, fid, &perms, NULL, &perm_error)) {
            gchar *msg = perm_error ? perm_error->message : "";
            if (strstr (msg, "403") || strstr (msg, "Access Denied") ||
                strstr (msg, "Forbidden") || strstr (msg, "not found")) {
                g_clear_error (&perm_error);
                is_fallback = TRUE;
                g_clear_object (&folder);
            } else {
                if (error)
                    g_propagate_error (error, perm_error);
                else
                    g_clear_error (&perm_error);
                if (fid_free)
                    fid_free (fid);
                goto cleanup;
            }
        } else {
            if (!ews_permissions_allow_read (perms, old_mailbox)) {
                is_fallback = TRUE;
                g_clear_object (&folder);
            }
            if (perms_free)
                perms_free (perms);
        }
    }

    if (fid_free)
        fid_free (fid);
    fid = NULL;

    set_mailbox (conn, old_mailbox);

    if (is_fallback) {
        gchar *tmp = g_strconcat ("freebusy-calendar", "::", email, NULL);
        folder = g_object_new (get_folder_type (), NULL);
        fid = fid_new (tmp, NULL, FALSE);
        folder_set_id (folder, fid);
        folder_set_name (folder, _("Availability"));
        folder_set_folder_type (folder, EWS_FOLDER_TYPE_CALENDAR);
        folder_set_foreign_mail (folder, email);
        folder_set_foreign (folder, TRUE);
        g_free (tmp);

        success = subscribe_sync (service, folder, NULL, email, "Availability", FALSE, NULL, error);
    } else {
        folder_set_foreign (folder, TRUE);
        folder_set_foreign_mail (folder, email);
        success = subscribe_sync (service, folder, NULL, email, "Calendar", FALSE, NULL, error);

        if (!success && error && *error) {
            gchar *msg = (*error)->message;
            if (strstr (msg, "403") || strstr (msg, "Access Denied") ||
                strstr (msg, "Forbidden") || strstr (msg, "not found")) {
                g_clear_error (error);
                g_clear_object (&folder);

                gchar *tmp = g_strconcat ("freebusy-calendar", "::", email, NULL);
                folder = g_object_new (get_folder_type (), NULL);
                fid = fid_new (tmp, NULL, FALSE);
                folder_set_id (folder, fid);
                folder_set_name (folder, _("Availability"));
                folder_set_folder_type (folder, EWS_FOLDER_TYPE_CALENDAR);
                folder_set_foreign_mail (folder, email);
                folder_set_foreign (folder, TRUE);
                g_free (tmp);

                success = subscribe_sync (service, folder, NULL, email, "Availability", FALSE, NULL, error);
            }
        }
    }

cleanup:
    if (conn && old_mailbox)
        set_mailbox (conn, old_mailbox);
    g_free (old_mailbox);
    if (conn)
        g_object_unref (conn);
    if (folder)
        g_object_unref (folder);
    if (service)
        g_object_unref (service);
    if (ews_lib)
        g_module_close (ews_lib);
    if (ews_priv)
        g_module_close (ews_priv);
    if (ews_module)
        g_module_close (ews_module);

    return success;
}
