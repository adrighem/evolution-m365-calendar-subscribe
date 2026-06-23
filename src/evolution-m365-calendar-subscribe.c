#include "calendar-actions.h"

#include <e-util/e-util.h>
#include <glib/gi18n.h>
#include <shell/e-shell-view.h>

G_MODULE_EXPORT gint e_plugin_lib_enable (EPlugin *ep, gint enable);
G_MODULE_EXPORT gboolean m365_calendar_subscribe_shell_view_init (EUIManager *ui_manager, EShellView *shell_view);

static const EUIActionEntry calendar_entries[] = {
    { "calendar-m365-quick-subscribe",
      "contact-new",
      N_("Quick _Subscribe..."),
      NULL,
      N_("Quickly subscribe to another person's calendar"),
      action_calendar_quick_subscribe_cb, NULL, NULL, NULL }
};

static const gchar *calendar_eui =
    "<eui>"
      "<menu id='main-menu'>"
        "<placeholder id='custom-menus'>"
          "<submenu id='file-menu' action='file-menu'>"
            "<placeholder id='file-actions'>"
              "<item action='calendar-m365-quick-subscribe'/>"
            "</placeholder>"
          "</submenu>"
        "</placeholder>"
      "</menu>"
      "<toolbar id='main-toolbar-without-headerbar'>"
        "<placeholder id='toolbar-actions'>"
          "<item action='calendar-m365-quick-subscribe'/>"
        "</placeholder>"
      "</toolbar>"
      "<toolbar id='main-toolbar-with-headerbar'>"
        "<placeholder id='toolbar-actions'>"
          "<item action='calendar-m365-quick-subscribe'/>"
        "</placeholder>"
      "</toolbar>"
    "</eui>";

G_MODULE_EXPORT gboolean
m365_calendar_subscribe_shell_view_init (EUIManager *ui_manager, EShellView *shell_view)
{
    e_ui_manager_add_actions_with_eui_data (
        ui_manager, "calendar", NULL,
        calendar_entries, G_N_ELEMENTS (calendar_entries), shell_view, calendar_eui);

    return TRUE;
}

G_MODULE_EXPORT gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
    return 0;
}

