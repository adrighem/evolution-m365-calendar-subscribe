add_library(m365-calendar-subscribe-common OBJECT
    calendar-actions.c
    calendar-manager.c
    calendar-ui.c
    string-utils.c
)

target_compile_options(m365-calendar-subscribe-common PRIVATE "-fno-sanitize=all")
target_include_directories(m365-calendar-subscribe-common PRIVATE ${EVOLUTION_INCLUDE_DIRS})

add_library(org-gnome-evolution-m365-calendar-subscribe MODULE
    evolution-m365-calendar-subscribe.c
    $<TARGET_OBJECTS:m365-calendar-subscribe-common>
)

target_compile_options(org-gnome-evolution-m365-calendar-subscribe PRIVATE "-fno-sanitize=all")
target_link_options(org-gnome-evolution-m365-calendar-subscribe PRIVATE "-fno-sanitize=all")
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

