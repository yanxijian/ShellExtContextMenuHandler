# Unit tests for the shell extension.
#
# Each test compiles the real production sources directly (via #include of the
# .cpp files) so the shipped parsing / expansion / launch logic is exercised
# rather than a copy of it. Stubs are provided for process-wide dependencies
# (ShellLog, DpiProvider) that are not relevant to the logic under test.

set(SHELLEXT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

function(shellext_test_target target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
    target_include_directories(${target} PRIVATE
        ${SHELLEXT_SOURCE_DIR}/include/shell_ext
        ${SHELLEXT_SOURCE_DIR}/third_party
        ${SHELLEXT_SOURCE_DIR}/src/actions
        ${SHELLEXT_SOURCE_DIR}/src/context
        ${SHELLEXT_SOURCE_DIR}/src/extension
        ${SHELLEXT_SOURCE_DIR}/src/menu
        ${SHELLEXT_SOURCE_DIR}/src/platform
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W3)
    endif()
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>"
    )
endfunction()

# --- menu.json parsing (nlohmann/json based) ---------------------------------
add_executable(shellext_test_menu_config
    test_menu_config.cpp
    ../src/menu/MenuGateChains.cpp
    ../src/context/ShellTarget.cpp
)
shellext_test_target(shellext_test_menu_config)
add_test(NAME menu_config COMMAND shellext_test_menu_config)
set_tests_properties(menu_config PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:shellext_test_menu_config>"
)
add_custom_command(TARGET shellext_test_menu_config POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHELLEXT_SOURCE_DIR}/config/menu.json"
        "$<TARGET_FILE_DIR:shellext_test_menu_config>/menu.json"
    COMMENT "Copying menu.json for menu_config test"
)

# --- action placeholder expansion ---------------------------------------------
add_executable(shellext_test_placeholders test_placeholders.cpp)
shellext_test_target(shellext_test_placeholders)
target_link_libraries(shellext_test_placeholders PRIVATE Shlwapi)
add_test(NAME placeholders COMMAND shellext_test_placeholders)

# --- context building / DragQueryFileW long paths -----------------------------
add_executable(shellext_test_context_builder test_context_builder.cpp)
shellext_test_target(shellext_test_context_builder)
target_link_libraries(shellext_test_context_builder PRIVATE Ole32 Shell32 Advapi32)
add_test(NAME context_builder COMMAND shellext_test_context_builder)

# --- launch executor / current-directory hijack regression --------------------
add_executable(shellext_marker_helper marker_helper.cpp)
shellext_test_target(shellext_marker_helper)

add_executable(shellext_test_launch_executor test_launch_executor.cpp)
shellext_test_target(shellext_test_launch_executor)
target_link_libraries(shellext_test_launch_executor PRIVATE Shlwapi)
target_compile_definitions(shellext_test_launch_executor PRIVATE
    "MARKER_HELPER_PATH=L\"$<TARGET_FILE:shellext_marker_helper>\""
)
add_test(NAME launch_executor COMMAND shellext_test_launch_executor)
