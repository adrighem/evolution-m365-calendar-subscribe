// SPDX-License-Identifier: GPL-3.0-only

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

#ifndef EVOLUTION_EWS_LIBDIR
#define EVOLUTION_EWS_LIBDIR "/usr/lib/evolution-ews"
#endif

#ifndef EVOLUTION_PRIVLIBDIR
#define EVOLUTION_PRIVLIBDIR "/usr/lib/evolution"
#endif

#ifndef EVOLUTION_MODULEDIR
#define EVOLUTION_MODULEDIR "/usr/lib/evolution/modules"
#endif

typedef struct {
    gchar *search_text;
    GCancellable *cancellable;
    CalendarContactCallback callback;
    gpointer user_data;
    GSList *contacts;
    gint pending_books;
} CalendarSearchContext;

typedef struct {
    gchar *email;
} SubscribeTaskData;

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
    if (!ctx)
        return;

    g_free (ctx->search_text);
    g_clear_object (&ctx->cancellable);
    g_slist_free_full (ctx->contacts, (GDestroyNotify) calendar_contact_free);
    g_free (ctx);
}

static void
subscribe_task_data_free (SubscribeTaskData *data)
{
    if (!data)
        return;

    g_free (data->email);
    g_free (data);
}

static GModule *
load_module_with_fallback (const gchar *primary_dir, const gchar *secondary_dir, const gchar *filename)
{
    GModule *module = NULL;

    if (primary_dir && *primary_dir) {
        g_autofree gchar *path = g_build_filename (primary_dir, filename, NULL);
        module = g_module_open (path, G_MODULE_BIND_LAZY);
    }
    if (!module && secondary_dir && *secondary_dir) {
        g_autofree gchar *path = g_build_filename (secondary_dir, filename, NULL);
        module = g_module_open (path, G_MODULE_BIND_LAZY);
    }
    if (!module) {
        module = g_module_open (filename, G_MODULE_BIND_LAZY);
    }

    if (module)
        g_module_make_resident (module);

    return module;
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
    CalendarSearchContext *ctx = (CalendarSearchContext *) user_data;
    GSList *contacts = NULL;
    g_autoptr(GError) error = NULL;

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
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("M365 Calendar Subscribe: Failed to get contacts: %s", error ? error->message : "Unknown error");
    }

    ctx->pending_books--;
    if (ctx->pending_books <= 0) {
        if (!ctx->cancellable || !g_cancellable_is_cancelled (ctx->cancellable)) {
            ctx->contacts = g_slist_reverse (ctx->contacts);
            ctx->callback (ctx->contacts, ctx->user_data);
        }
        calendar_search_context_free (ctx);
    }
}

static void
calendar_search_connected_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    CalendarSearchContext *ctx = (CalendarSearchContext *) user_data;
    g_autoptr(GError) error = NULL;
    EBookClient *book_client = (EBookClient *) e_book_client_connect_finish (res, &error);

    if (ctx->cancellable && g_cancellable_is_cancelled (ctx->cancellable)) {
        if (book_client)
            g_object_unref (book_client);
        ctx->pending_books--;
        if (ctx->pending_books <= 0)
            calendar_search_context_free (ctx);
        return;
    }

    if (book_client) {
        EBookQuery *query = e_book_query_any_field_contains (ctx->search_text);
        g_autofree gchar *sexp = e_book_query_to_string (query);

        e_book_client_get_contacts (book_client, sexp, ctx->cancellable, calendar_search_got_contacts_cb, ctx);

        e_book_query_unref (query);
        g_object_unref (book_client);
    } else {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("M365 Calendar Subscribe: Failed to connect to book: %s", error ? error->message : "Unknown error");

        ctx->pending_books--;
        if (ctx->pending_books <= 0) {
            if (!ctx->cancellable || !g_cancellable_is_cancelled (ctx->cancellable))
                ctx->callback (ctx->contacts, ctx->user_data);
            calendar_search_context_free (ctx);
        }
    }
}

static void
calendar_load_all_connected_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    CalendarSearchContext *ctx = (CalendarSearchContext *) user_data;
    g_autoptr(GError) error = NULL;
    EBookClient *book_client = (EBookClient *) e_book_client_connect_finish (res, &error);

    if (ctx->cancellable && g_cancellable_is_cancelled (ctx->cancellable)) {
        if (book_client)
            g_object_unref (book_client);
        ctx->pending_books--;
        if (ctx->pending_books <= 0)
            calendar_search_context_free (ctx);
        return;
    }

    if (book_client) {
        e_book_client_get_contacts (book_client, "(contains \"x-evolution-any-field\" \"\")", ctx->cancellable, calendar_search_got_contacts_cb, ctx);
        g_object_unref (book_client);
    } else {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("M365 Calendar Subscribe: Failed to connect to book for bulk load: %s", error ? error->message : "Unknown error");

        ctx->pending_books--;
        if (ctx->pending_books <= 0) {
            if (!ctx->cancellable || !g_cancellable_is_cancelled (ctx->cancellable))
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
m365_calendar_search_contacts (const gchar *search_text,
                               GCancellable *cancellable,
                               CalendarContactCallback callback,
                               gpointer user_data)
{
    GList *filtered_sources = calendar_list_ews_or_m365_address_books ();

    if (!filtered_sources) {
        g_warning ("M365 Calendar Subscribe: No EWS or Microsoft 365 address books available.");
        callback (NULL, user_data);
        return;
    }

    CalendarSearchContext *ctx = g_new0 (CalendarSearchContext, 1);
    ctx->search_text = g_strdup (search_text);
    if (cancellable)
        ctx->cancellable = g_object_ref (cancellable);
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->pending_books = g_list_length (filtered_sources);

    for (GList *l = filtered_sources; l; l = l->next) {
        ESource *source = E_SOURCE (l->data);
        e_book_client_connect (source, 30, cancellable, calendar_search_connected_cb, ctx);
    }

    g_list_free_full (filtered_sources, g_object_unref);
}

void
m365_calendar_load_all_contacts (GCancellable *cancellable, CalendarContactCallback callback, gpointer user_data)
{
    GList *filtered_sources = calendar_list_ews_or_m365_address_books ();

    if (!filtered_sources) {
        g_warning ("M365 Calendar Subscribe: No EWS or Microsoft 365 address books available.");
        callback (NULL, user_data);
        return;
    }

    CalendarSearchContext *ctx = g_new0 (CalendarSearchContext, 1);
    if (cancellable)
        ctx->cancellable = g_object_ref (cancellable);
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->pending_books = g_list_length (filtered_sources);

    for (GList *l = filtered_sources; l; l = l->next) {
        ESource *source = E_SOURCE (l->data);
        e_book_client_connect (source, 30, cancellable, calendar_load_all_connected_cb, ctx);
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
        EEwsPermission *perm = (EEwsPermission *) l->data;

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
typedef const gchar * (*EwsGetMailboxFunc) (gpointer cnc);

typedef GType (*EwsFolderGetTypeFunc) (void);
typedef void (*EwsFolderSetIdFunc) (gpointer folder, gpointer fid);
typedef void (*EwsFolderSetNameFunc) (gpointer folder, const gchar *name);
typedef void (*EwsFolderSetFolderTypeFunc) (gpointer folder, gint type);
typedef void (*EwsFolderSetForeignMailFunc) (gpointer folder, const gchar *email);
typedef void (*EwsFolderSetForeignFunc) (gpointer folder, gboolean is_foreign);

typedef gboolean (*EwsSubscribeSyncFunc) (gpointer ews_store,
                                          gpointer folder,
                                          const gchar *user_displayname,
                                          const gchar *user_email,
                                          const gchar *fallback_folder_name,
                                          gboolean include_subfolders,
                                          GCancellable *cancellable,
                                          GError **error);

typedef struct {
    CamelEwsStoreGetConnFunc get_conn;
    EwsFolderIdNewFunc fid_new;
    EwsFolderIdFreeFunc fid_free;
    EwsGetFolderInfoFunc get_info;
    EwsGetFolderPermissionsFunc get_perms;
    EwsPermissionsFreeFunc perms_free;
    EwsGetMailboxFunc get_mailbox;
    EwsFolderGetTypeFunc get_folder_type;
    EwsFolderSetIdFunc folder_set_id;
    EwsFolderSetNameFunc folder_set_name;
    EwsFolderSetFolderTypeFunc folder_set_folder_type;
    EwsFolderSetForeignMailFunc folder_set_foreign_mail;
    EwsFolderSetForeignFunc folder_set_foreign;
    EwsSubscribeSyncFunc subscribe_sync;
    gboolean loaded;
} EwsSymbols;

static EwsSymbols global_ews_syms;
static gsize ews_symbols_init_state = 0;

static gboolean
ensure_ews_symbols (GError **error)
{
    if (g_once_init_enter (&ews_symbols_init_state)) {
        GModule *ews_lib = load_module_with_fallback (EVOLUTION_EWS_LIBDIR, EVOLUTION_PRIVLIBDIR "-ews", "libevolution-ews.so");
        GModule *ews_priv = load_module_with_fallback (EVOLUTION_EWS_LIBDIR, EVOLUTION_PRIVLIBDIR "-ews", "libcamelews-priv.so");
        GModule *ews_module = load_module_with_fallback (EVOLUTION_MODULEDIR, EVOLUTION_PRIVLIBDIR "/modules", "module-ews-configuration.so");

        if (ews_lib && ews_priv && ews_module) {
            g_module_symbol (ews_priv, "camel_ews_store_ref_connection", (gpointer *) &global_ews_syms.get_conn);
            g_module_symbol (ews_lib, "e_ews_folder_id_new", (gpointer *) &global_ews_syms.fid_new);
            g_module_symbol (ews_lib, "e_ews_folder_id_free", (gpointer *) &global_ews_syms.fid_free);
            g_module_symbol (ews_lib, "e_ews_connection_get_folder_info_sync", (gpointer *) &global_ews_syms.get_info);
            g_module_symbol (ews_lib, "e_ews_connection_get_folder_permissions_sync", (gpointer *) &global_ews_syms.get_perms);
            g_module_symbol (ews_lib, "e_ews_permissions_free", (gpointer *) &global_ews_syms.perms_free);
            g_module_symbol (ews_lib, "e_ews_connection_get_mailbox", (gpointer *) &global_ews_syms.get_mailbox);
            g_module_symbol (ews_lib, "e_ews_folder_get_type", (gpointer *) &global_ews_syms.get_folder_type);
            g_module_symbol (ews_lib, "e_ews_folder_set_id", (gpointer *) &global_ews_syms.folder_set_id);
            g_module_symbol (ews_lib, "e_ews_folder_set_name", (gpointer *) &global_ews_syms.folder_set_name);
            g_module_symbol (ews_lib, "e_ews_folder_set_folder_type", (gpointer *) &global_ews_syms.folder_set_folder_type);
            g_module_symbol (ews_lib, "e_ews_folder_set_foreign_mail", (gpointer *) &global_ews_syms.folder_set_foreign_mail);
            g_module_symbol (ews_lib, "e_ews_folder_set_foreign", (gpointer *) &global_ews_syms.folder_set_foreign);
            g_module_symbol (ews_module, "e_ews_subscrive_foreign_folder_subscribe_sync", (gpointer *) &global_ews_syms.subscribe_sync);

            if (global_ews_syms.get_conn && global_ews_syms.fid_new && global_ews_syms.fid_free &&
                global_ews_syms.get_info && global_ews_syms.get_perms && global_ews_syms.perms_free &&
                global_ews_syms.subscribe_sync && global_ews_syms.get_mailbox &&
                global_ews_syms.get_folder_type && global_ews_syms.folder_set_id &&
                global_ews_syms.folder_set_name && global_ews_syms.folder_set_folder_type &&
                global_ews_syms.folder_set_foreign_mail && global_ews_syms.folder_set_foreign) {
                global_ews_syms.loaded = TRUE;
            }
        }
        g_once_init_leave (&ews_symbols_init_state, 1);
    }

    if (!global_ews_syms.loaded) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Could not load or resolve Evolution EWS libraries/symbols");
        return FALSE;
    }
    return TRUE;
}

static gboolean
m365_calendar_subscribe_internal (ESource *ews_source,
                                  const gchar *email,
                                  GCancellable *cancellable,
                                  GError **error)
{
    EShell *shell = e_shell_get_default ();
    EShellBackend *mail_backend;
    EMailSession *session;
    CamelService *service = NULL;
    gpointer conn = NULL;
    gpointer fid = NULL;
    gpointer folder = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (E_IS_SOURCE (ews_source), FALSE);
    g_return_val_if_fail (email != NULL, FALSE);

    if (g_cancellable_set_error_if_cancelled (cancellable, error))
        return FALSE;

    if (!E_IS_SHELL (shell)) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No active Evolution shell instance");
        return FALSE;
    }

    mail_backend = e_shell_get_backend_by_name (shell, "mail");
    if (!mail_backend) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Evolution mail backend not found");
        return FALSE;
    }

    session = e_mail_backend_get_session (E_MAIL_BACKEND (mail_backend));
    if (!session) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Evolution mail session not found");
        return FALSE;
    }

    service = camel_session_ref_service ((CamelSession *) session, e_source_get_uid (ews_source));
    if (!service) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Camel service for EWS source not found");
        return FALSE;
    }

    if (!ensure_ews_symbols (error))
        goto cleanup;

    conn = global_ews_syms.get_conn (service);
    if (!conn) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Could not get EWS connection for store");
        goto cleanup;
    }

#if defined(FREEBUSY_ONLY_DEFAULT) && FREEBUSY_ONLY_DEFAULT
    gboolean is_fallback = TRUE;
#else
    const gchar *my_mailbox = global_ews_syms.get_mailbox (conn);
    gboolean is_fallback = FALSE;
    fid = global_ews_syms.fid_new ("calendar", NULL, TRUE);

    if (!global_ews_syms.get_info (conn, G_PRIORITY_DEFAULT, email, fid, &folder, cancellable, error)) {
        const gchar *msg = (error && *error) ? (*error)->message : "";
        if (strstr (msg, "403") || strstr (msg, "Access Denied") ||
            strstr (msg, "Forbidden") || strstr (msg, "not found")) {
            if (error)
                g_clear_error (error);
            is_fallback = TRUE;
        } else {
            if (global_ews_syms.fid_free)
                global_ews_syms.fid_free (fid);
            goto cleanup;
        }
    } else {
        GSList *perms = NULL;
        GError *perm_error = NULL;

        if (!global_ews_syms.get_perms (conn, G_PRIORITY_DEFAULT, fid, &perms, cancellable, &perm_error)) {
            const gchar *msg = perm_error ? perm_error->message : "";
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
                if (global_ews_syms.fid_free)
                    global_ews_syms.fid_free (fid);
                goto cleanup;
            }
        } else {
            if (!ews_permissions_allow_read (perms, my_mailbox)) {
                is_fallback = TRUE;
                g_clear_object (&folder);
            }
            if (global_ews_syms.perms_free)
                global_ews_syms.perms_free (perms);
        }
    }

    if (global_ews_syms.fid_free)
        global_ews_syms.fid_free (fid);
    fid = NULL;
#endif

    if (is_fallback) {
        g_autofree gchar *tmp = g_strconcat ("freebusy-calendar", "::", email, NULL);
        folder = g_object_new (global_ews_syms.get_folder_type (), NULL);
        fid = global_ews_syms.fid_new (tmp, NULL, FALSE);
        global_ews_syms.folder_set_id (folder, fid);
        global_ews_syms.folder_set_name (folder, _("Availability"));
        global_ews_syms.folder_set_folder_type (folder, EWS_FOLDER_TYPE_CALENDAR);
        global_ews_syms.folder_set_foreign_mail (folder, email);
        global_ews_syms.folder_set_foreign (folder, TRUE);

        success = global_ews_syms.subscribe_sync (service, folder, NULL, email, "Availability", FALSE, cancellable, error);
    } else {
        global_ews_syms.folder_set_foreign (folder, TRUE);
        global_ews_syms.folder_set_foreign_mail (folder, email);
        success = global_ews_syms.subscribe_sync (service, folder, NULL, email, "Calendar", FALSE, cancellable, error);

        if (!success && error && *error) {
            const gchar *msg = (*error)->message;
            if (strstr (msg, "403") || strstr (msg, "Access Denied") ||
                strstr (msg, "Forbidden") || strstr (msg, "not found")) {
                g_clear_error (error);
                g_clear_object (&folder);

                g_autofree gchar *tmp = g_strconcat ("freebusy-calendar", "::", email, NULL);
                folder = g_object_new (global_ews_syms.get_folder_type (), NULL);
                fid = global_ews_syms.fid_new (tmp, NULL, FALSE);
                global_ews_syms.folder_set_id (folder, fid);
                global_ews_syms.folder_set_name (folder, _("Availability"));
                global_ews_syms.folder_set_folder_type (folder, EWS_FOLDER_TYPE_CALENDAR);
                global_ews_syms.folder_set_foreign_mail (folder, email);
                global_ews_syms.folder_set_foreign (folder, TRUE);

                success = global_ews_syms.subscribe_sync (service, folder, NULL, email, "Availability", FALSE, cancellable, error);
            }
        }
    }

cleanup:
    if (conn)
        g_object_unref (conn);
    if (folder)
        g_object_unref (folder);
    if (service)
        g_object_unref (service);

    return success;
}

static void
m365_calendar_subscribe_thread (GTask *task,
                                gpointer source_object,
                                gpointer task_data,
                                GCancellable *cancellable)
{
    ESource *ews_source = E_SOURCE (source_object);
    SubscribeTaskData *data = (SubscribeTaskData *) task_data;
    GError *error = NULL;

    if (g_task_return_error_if_cancelled (task))
        return;

    gboolean success = m365_calendar_subscribe_internal (ews_source, data->email, cancellable, &error);
    if (success)
        g_task_return_boolean (task, TRUE);
    else
        g_task_return_error (task, error);
}

void
m365_calendar_subscribe_async (ESource *ews_source,
                               const gchar *email,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    g_return_if_fail (E_IS_SOURCE (ews_source));
    g_return_if_fail (email != NULL);

    GTask *task = g_task_new (ews_source, cancellable, callback, user_data);
    g_task_set_source_tag (task, m365_calendar_subscribe_async);

    SubscribeTaskData *data = g_new0 (SubscribeTaskData, 1);
    data->email = g_strdup (email);
    g_task_set_task_data (task, data, (GDestroyNotify) subscribe_task_data_free);

    g_task_run_in_thread (task, m365_calendar_subscribe_thread);
    g_object_unref (task);
}

gboolean
m365_calendar_subscribe_finish (ESource *ews_source,
                                GAsyncResult *result,
                                GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, ews_source), FALSE);
    return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
m365_calendar_subscribe (ESource *ews_source, const gchar *email, GError **error)
{
    return m365_calendar_subscribe_internal (ews_source, email, NULL, error);
}
