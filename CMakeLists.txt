cmake_minimum_required(VERSION 3.15)
project(evolution-m365-calendar-subscribe VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)

pkg_check_modules(EVOLUTION REQUIRED IMPORTED_TARGET
    evolution-shell-3.0
    evolution-mail-3.0
    libedataserver-1.2
    camel-1.2
    libebook-1.2
    libebook-contacts-1.2
    libedataserverui-1.2
    glib-2.0
    gmodule-2.0
    gio-2.0
    gtk+-3.0
)

pkg_get_variable(EVO_PRIVLIBDIR evolution-shell-3.0 privlibdir)

set(EVOLUTION_PLUGIN_DIR "${EVO_PRIVLIBDIR}/plugins" CACHE PATH "Path to Evolution plugins directory")

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

