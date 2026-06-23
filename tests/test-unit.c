#include <glib.h>
#include <libebook/libebook.h>

#include "../src/calendar-manager.h"
#include "../src/string-utils.h"

static void
test_fuzzy_match (void)
{
    g_assert_true (calendar_fuzzy_match ("test", "test"));
    g_assert_true (calendar_fuzzy_match ("test", "this is a test string"));
    g_assert_true (calendar_fuzzy_match ("TEST", "test"));
    g_assert_true (calendar_fuzzy_match ("test", "TEST"));
    g_assert_true (calendar_fuzzy_match ("jsmith", "John Smith <john.smith@example.com>"));
    g_assert_true (calendar_fuzzy_match ("jnoe", "Jane Doe <jane@example.com>"));
    g_assert_true (calendar_fuzzy_match ("jnedoe", "Jane Doe <jane@example.com>"));
    g_assert_true (calendar_fuzzy_match ("abc", "axbycz"));

    g_assert_false (calendar_fuzzy_match ("jsmith", "Jane Doe <jane@example.com>"));
    g_assert_false (calendar_fuzzy_match ("abc", "ab"));
    g_assert_false (calendar_fuzzy_match ("abc", "cba"));
}

static void
test_subscribe_calendar_null_source (void)
{
    GError *error = NULL;

    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*E_IS_SOURCE*");
    g_assert_false (m365_calendar_subscribe (NULL, "test@example.com", &error));
    g_test_assert_expected_messages ();
}

static void
test_ews_contact_filter_logic (void)
{
    ESource *source_ews = e_source_new (NULL, NULL, NULL);
    ESourceAddressBook *ab_ews = e_source_get_extension (source_ews, E_SOURCE_EXTENSION_ADDRESS_BOOK);
    e_source_backend_set_backend_name (E_SOURCE_BACKEND (ab_ews), "ews");

    ESource *source_m365 = e_source_new (NULL, NULL, NULL);
    ESourceAddressBook *ab_m365 = e_source_get_extension (source_m365, E_SOURCE_EXTENSION_ADDRESS_BOOK);
    e_source_backend_set_backend_name (E_SOURCE_BACKEND (ab_m365), "microsoft365");

    ESource *source_local = e_source_new (NULL, NULL, NULL);
    ESourceAddressBook *ab_local = e_source_get_extension (source_local, E_SOURCE_EXTENSION_ADDRESS_BOOK);
    e_source_backend_set_backend_name (E_SOURCE_BACKEND (ab_local), "local");

    GList *sources = NULL;
    sources = g_list_append (sources, source_ews);
    sources = g_list_append (sources, source_local);
    sources = g_list_append (sources, source_m365);

    GList *filtered = NULL;
    for (GList *l = sources; l; l = l->next) {
        ESource *source = E_SOURCE (l->data);
        if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
            ESourceAddressBook *address_book = e_source_get_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
            const gchar *backend_name = e_source_backend_get_backend_name (E_SOURCE_BACKEND (address_book));
            if (backend_name &&
                (g_strcmp0 (backend_name, "ews") == 0 ||
                 g_strcmp0 (backend_name, "microsoft365") == 0)) {
                filtered = g_list_append (filtered, g_object_ref (source));
            }
        }
    }

    g_assert_cmpint (g_list_length (filtered), ==, 2);
    g_assert_true (E_SOURCE (g_list_nth_data (filtered, 0)) == source_ews);
    g_assert_true (E_SOURCE (g_list_nth_data (filtered, 1)) == source_m365);

    g_list_free_full (filtered, g_object_unref);
    g_list_free_full (sources, g_object_unref);
}

static void
dummy_contacts_callback (const GSList *contacts, gpointer user_data)
{
    gboolean *called = user_data;
    *called = TRUE;
}

static void
test_contact_loading_no_shell (void)
{
    gboolean callback_called = FALSE;

    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*No EWS or Microsoft 365 address books available*");
    m365_calendar_load_all_contacts (dummy_contacts_callback, &callback_called);
    g_assert_true (callback_called);
    g_test_assert_expected_messages ();

    callback_called = FALSE;
    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*No EWS or Microsoft 365 address books available*");
    m365_calendar_search_contacts ("test", dummy_contacts_callback, &callback_called);
    g_assert_true (callback_called);
    g_test_assert_expected_messages ();
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/utils/fuzzy-match", test_fuzzy_match);
    g_test_add_func ("/manager/subscribe-calendar-null-source", test_subscribe_calendar_null_source);
    g_test_add_func ("/manager/ews-contact-filter", test_ews_contact_filter_logic);
    g_test_add_func ("/manager/contact-loading-no-shell", test_contact_loading_no_shell);

    return g_test_run ();
}

