add_executable(test-unit
    test-unit.c
    $<TARGET_OBJECTS:m365-calendar-subscribe-common>
)
target_link_libraries(test-unit PRIVATE PkgConfig::EVOLUTION)

add_executable(test-calendar-ui
    test-calendar-ui.c
    $<TARGET_OBJECTS:m365-calendar-subscribe-common>
)
target_link_libraries(test-calendar-ui PRIVATE PkgConfig::EVOLUTION)

add_test(NAME unit COMMAND test-unit)
add_test(NAME calendar-ui COMMAND test-calendar-ui)

