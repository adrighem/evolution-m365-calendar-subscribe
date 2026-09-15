# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.15)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_FROM_FILE LIMIT_COUNT 1)
string(STRIP "${PROJECT_VERSION_FROM_FILE}" PROJECT_VERSION_FROM_FILE)

project(evolution-m365-calendar-subscribe
    VERSION "${PROJECT_VERSION_FROM_FILE}"
    DESCRIPTION "GNOME Evolution plugin for Microsoft 365 and Exchange calendar subscriptions"
    LANGUAGES C)

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

pkg_get_variable(EVO_LIBDIR evolution-shell-3.0 libdir)
pkg_get_variable(EVO_PRIVLIBDIR evolution-shell-3.0 privlibdir)
pkg_get_variable(EVO_MODULEDIR evolution-shell-3.0 moduledir)

set(EVOLUTION_PLUGIN_DIR "${EVO_PRIVLIBDIR}/plugins" CACHE PATH "Path to Evolution plugins directory")
set(EVOLUTION_EWS_LIBDIR "${EVO_LIBDIR}/evolution-ews" CACHE PATH "Path to Evolution EWS private libraries")
set(EVOLUTION_MODULEDIR "${EVO_MODULEDIR}" CACHE PATH "Path to Evolution modules directory")

option(FREEBUSY_ONLY "Only attempt free/busy availability calendars and bypass standard calendars" ON)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
