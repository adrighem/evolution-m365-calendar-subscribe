# SPDX-License-Identifier: GPL-3.0-only

add_library(m365-calendar-subscribe-common OBJECT
    calendar-actions.c
    calendar-manager.c
    calendar-ui.c
    string-utils.c
)

target_compile_definitions(m365-calendar-subscribe-common PRIVATE
    EVOLUTION_EWS_LIBDIR="${EVOLUTION_EWS_LIBDIR}"
    EVOLUTION_PRIVLIBDIR="${EVO_PRIVLIBDIR}"
    EVOLUTION_MODULEDIR="${EVOLUTION_MODULEDIR}"
    $<$<BOOL:${FREEBUSY_ONLY}>:FREEBUSY_ONLY_DEFAULT=1>
)
set_property(TARGET m365-calendar-subscribe-common PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(m365-calendar-subscribe-common PRIVATE ${EVOLUTION_INCLUDE_DIRS})

add_library(org-gnome-evolution-m365-calendar-subscribe MODULE
    evolution-m365-calendar-subscribe.c
    $<TARGET_OBJECTS:m365-calendar-subscribe-common>
)

target_link_libraries(org-gnome-evolution-m365-calendar-subscribe PRIVATE PkgConfig::EVOLUTION)
target_include_directories(org-gnome-evolution-m365-calendar-subscribe PRIVATE ${EVOLUTION_INCLUDE_DIRS})

set_target_properties(org-gnome-evolution-m365-calendar-subscribe PROPERTIES PREFIX "lib")

configure_file(
    org-gnome-evolution-m365-calendar-subscribe.eplug.xml
    org-gnome-evolution-m365-calendar-subscribe.eplug
    @ONLY
)

install(TARGETS org-gnome-evolution-m365-calendar-subscribe
        DESTINATION ${EVOLUTION_PLUGIN_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org-gnome-evolution-m365-calendar-subscribe.eplug
        DESTINATION ${EVOLUTION_PLUGIN_DIR}
)
