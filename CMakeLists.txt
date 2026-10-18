cmake_minimum_required(VERSION 3.20)

project(ShellExtContextMenuHandler
    VERSION 1.0.0
    DESCRIPTION "Configurable Windows Explorer context menu shell extension"
    LANGUAGES CXX RC
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A shell extension is loaded by Explorer, not by a redistributable installer.
# Link the CRT statically so a missing VC++ runtime cannot silently disable the
# extension on a clean machine.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(TARGET_NAME CppShellExtContextMenuHandler)

set(SHELL_EXT_SOURCES
    src/extension/dllmain.cpp
    src/extension/ClassFactory.cpp
    src/extension/FileContextMenuExt.cpp
    src/context/ContextBuilder.cpp
    src/context/ShellTarget.cpp
    src/gates/GateRegistry.cpp
    src/gates/JsonFilterGate.cpp
    src/gates/DemoHideTempItemGate.cpp
    src/gates/DemoReadOnlyPresentationGate.cpp
    src/actions/ActionPlaceholders.cpp
    src/actions/ExecutorRegistry.cpp
    src/actions/MessageBoxExecutor.cpp
    src/actions/LaunchExecutor.cpp
    src/actions/DemoActionLogExecutor.cpp
    src/platform/OsVersion.cpp
    src/platform/DpiProvider.cpp
    src/icons/IconBitmapUtils.cpp
    src/icons/IconProviderRegistry.cpp
    src/icons/SvgIconProvider.cpp
    src/icons/BitmapFileIconProvider.cpp
    src/icons/DefaultResourceIconProvider.cpp
    src/menu/MenuActionHandler.cpp
    src/menu/MenuConfig.cpp
    src/menu/MenuGateChains.cpp
    src/menu/MenuProvider.cpp
    src/menu/PathHelpers.cpp
    src/menu/ShellLog.cpp
    src/registry/Reg.cpp
    src/registry/RegistrationConfig.cpp
)

add_library(${TARGET_NAME} SHARED ${SHELL_EXT_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME ${TARGET_NAME}
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shell_ext
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanosvg
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/src/extension
    ${CMAKE_CURRENT_SOURCE_DIR}/src/context
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gates
    ${CMAKE_CURRENT_SOURCE_DIR}/src/actions
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/icons
    ${CMAKE_CURRENT_SOURCE_DIR}/src/menu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/registry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources
)

target_compile_definitions(${TARGET_NAME} PRIVATE
    UNICODE
    _UNICODE
    WIN32
    _WINDOWS
    _USRDLL
    CPPSHELLEXTCONTEXTMENUHANDLER_EXPORTS
    _WIN32_WINNT=0x0601
    WINVER=0x0601
)

target_sources(${TARGET_NAME} PRIVATE
    src/resources/ShellExtContextMenuHandler.rc
    src/resources/Version.rc
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W3)
    target_compile_options(${TARGET_NAME} PRIVATE /wd4244 /wd4267 /wd4996)
    target_link_options(${TARGET_NAME} PRIVATE
        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/resources/GlobalExportFunctions.def"
    )
endif()

target_link_libraries(${TARGET_NAME} PRIVATE Shlwapi)

# Select the regsvr32 that matches the DLL bitness. A 32-bit build must use
# SysWOW64 on a 64-bit OS; a 64-bit build always uses System32.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SHELLEXT_REGSVR "%SystemRoot%\\System32\\regsvr32.exe")
else()
    set(SHELLEXT_REGSVR "%SystemRoot%\\SysWOW64\\regsvr32.exe")
endif()

configure_file(tools/register.bat.in "${CMAKE_CURRENT_BINARY_DIR}/register.bat" @ONLY)
configure_file(tools/unregister.bat.in "${CMAKE_CURRENT_BINARY_DIR}/unregister.bat" @ONLY)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/menu.json"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/menu.json"
    COMMENT "Copying menu.json to output directory"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/registration.json"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/registration.json"
    COMMENT "Copying registration.json to output directory"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/icons"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config/icons"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/icons"
    COMMENT "Copying icon assets to output directory"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/register.bat"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/register.bat"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/unregister.bat"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/unregister.bat"
    COMMENT "Copying registration scripts to output directory"
)

install(TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)

install(FILES config/menu.json DESTINATION .)
install(FILES config/registration.json DESTINATION .)
install(DIRECTORY config/icons DESTINATION .)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/register.bat" DESTINATION .)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/unregister.bat" DESTINATION .)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION .)

option(SHELLEXT_BUILD_TESTS "Build the unit test suite" OFF)
if(SHELLEXT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()