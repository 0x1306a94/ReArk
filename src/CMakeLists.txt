set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(reark_core STATIC
    core/HyleDecompiler.cpp
    core/HyleDecompiler.h
    core/PerformanceTrace.cpp
    core/PerformanceTrace.h
    core/CommandRunner.cpp
    core/CommandRunner.h
    device/HdcDeviceBackend.cpp
    device/HdcDeviceBackend.h
    device/HarmonyPackageRewriter.cpp
    device/HarmonyPackageRewriter.h
    device/InstallablePackageResolver.cpp
    device/InstallablePackageResolver.h
    device/UiAutomationBackend.cpp
    device/UiAutomationBackend.h
    model/DocumentContent.h
    signing/HarmonyHapSigner.cpp
    signing/HarmonyHapSigner.h
    signing/SigningMaterialInspector.cpp
    signing/SigningMaterialInspector.h
)
target_include_directories(reark_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(reark_core
    PUBLIC
        Qt6::Core
        hyle::hyle
    PRIVATE
        Qt6::Gui
        Qt6::Network
)

qt_add_executable(${PROJECT_NAME}
    main.cpp
    core/AppInitializer.cpp
    core/AppInitializer.h
    core/ApplicationController.cpp
    core/ApplicationController.h
    core/BuildInfoProvider.cpp
    core/BuildInfoProvider.h
    core/HyleDecompiler.h
    core/PerformanceTrace.h
    core/ResourcePreviewProvider.cpp
    core/ResourcePreviewProvider.h
    core/Translator.cpp
    core/Translator.h
    core/WindowChrome.cpp
    core/WindowChrome.h
    controller/DecompilerController.cpp
    controller/DecompilerController.h
    controller/AgentSettings.cpp
    controller/AgentSettings.h
    controller/ProtectedSettingsSecret.cpp
    controller/ProtectedSettingsSecret.h
    controller/SigningSettings.cpp
    controller/SigningSettings.h
    controller/SigningController.cpp
    controller/SigningController.h
    controller/AgentKnowledgeController.cpp
    controller/AgentKnowledgeController.h
    controller/AgentController.cpp
    controller/AgentController.h
    controller/AgentRequestRouter.cpp
    controller/AgentRequestRouter.h
    controller/DeviceRuntimeController.cpp
    controller/DeviceRuntimeController.h
    controller/PythonRuntimeResolver.cpp
    controller/PythonRuntimeResolver.h
    controller/LanguageController.cpp
    controller/LanguageController.h
    controller/SettingsController.cpp
    controller/SettingsController.h
    controller/UpdateController.cpp
    controller/UpdateController.h
    controller/ReleaseNotesLocalizer.cpp
    controller/ReleaseNotesLocalizer.h
    model/AgentMessageModel.cpp
    model/AgentMessageModel.h
    model/DocumentContent.h
    model/OpenFileTabsModel.cpp
    model/OpenFileTabsModel.h
    model/RecentFilesModel.cpp
    model/RecentFilesModel.h
    model/HexDocumentModel.cpp
    model/HexDocumentModel.h
    model/SourceTreeModel.cpp
    model/SourceTreeModel.h
    presentation/CodeTheme.cpp
    presentation/CodeTheme.h
    presentation/CodeEditorItem.cpp
    presentation/CodeEditorItem.h
    presentation/HexViewerItem.cpp
    presentation/HexViewerItem.h
    presentation/MarkdownRenderer.cpp
    presentation/MarkdownRenderer.h
    presentation/SyntaxThemeProvider.cpp
    presentation/SyntaxThemeProvider.h
    ${WIN32_RESOURCES}
)

set_source_files_properties(ui/qml/main.qml PROPERTIES
    QT_RESOURCE_ALIAS main.qml
)

qt_add_qml_module(${PROJECT_NAME}
    URI "com.reark.app"
    VERSION 1.0
    RESOURCE_PREFIX /
    IMPORTS
        ReArk
    QML_FILES
        ui/qml/main.qml
)

qt_add_translations(${PROJECT_NAME}
    TS_FILES ${TS_FILES}
    LUPDATE_OPTIONS -no-obsolete
)

set(REARK_BUILD_INFO_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.h")
add_custom_target(reark_build_info ALL
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_FILE="${REARK_BUILD_INFO_HEADER}"
        -P "${CMAKE_SOURCE_DIR}/cmake/WriteBuildInfo.cmake"
    BYPRODUCTS "${REARK_BUILD_INFO_HEADER}"
    COMMENT "Writing build metadata"
)
add_dependencies(${PROJECT_NAME} reark_build_info)

add_subdirectory(ui/qml/ReArk)

qt_add_resources(${PROJECT_NAME} "images"
    PREFIX "/images"
    BASE "${CMAKE_SOURCE_DIR}/resources/images"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/images/app_icon.ico"
        "${CMAKE_SOURCE_DIR}/resources/images/app_icon.png"
)

qt_add_resources(${PROJECT_NAME} "icons"
    PREFIX "/icons"
    BASE "${CMAKE_SOURCE_DIR}/resources/icons"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/icons/chevron-down.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/chevron-up.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/close-small.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/eye.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/eye-off.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/arrow-left.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/arrow-up.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/check.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/copy.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/gem.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/paperclip.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/pencil.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/fontawesome/square-plus.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/find-match-case.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/find-regex.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/find-whole-word.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/folder-open-white.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/github.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/github-white.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/issue.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/issue-white.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/trash-muted.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/upload-muted.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/website.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/website-white.svg"
)

qt_add_resources(${PROJECT_NAME} "legal"
    PREFIX "/legal"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        "${CMAKE_SOURCE_DIR}/LICENSE"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    reark_core
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Network
    hyle::hyle
    reark_qml_module
    reark_qml_moduleplugin
)

if(wuwe_FOUND)
    get_target_property(REARK_WUWE_INCLUDE_DIRS wuwe::wuwe INTERFACE_INCLUDE_DIRECTORIES)
    if(REARK_WUWE_INCLUDE_DIRS)
        target_include_directories(${PROJECT_NAME} BEFORE PRIVATE ${REARK_WUWE_INCLUDE_DIRS})
    endif()
    foreach(_reark_wuwe_header_prefix
        "${WUWE_INSTALL_PREFIX}/.."
        "${WUWE_INSTALL_PREFIX}")
        get_filename_component(_reark_wuwe_header_prefix_abs
            "${_reark_wuwe_header_prefix}" ABSOLUTE)
        if(EXISTS "${_reark_wuwe_header_prefix_abs}/include/wuwe/agent/llm/llm_provider_factory.h")
            target_include_directories(${PROJECT_NAME} BEFORE PRIVATE
                "${_reark_wuwe_header_prefix_abs}/include")
            break()
        endif()
    endforeach()
    get_target_property(REARK_WUWE_IMPORTED_LOCATION_DEBUG wuwe::wuwe IMPORTED_LOCATION_DEBUG)
    get_target_property(REARK_WUWE_IMPORTED_LOCATION_RELEASE wuwe::wuwe IMPORTED_LOCATION_RELEASE)
    if(NOT REARK_WUWE_IMPORTED_LOCATION_DEBUG
        AND EXISTS "${WUWE_INSTALL_PREFIX}/lib/wuwed.lib")
        set_property(TARGET wuwe::wuwe APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(wuwe::wuwe PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
            IMPORTED_LOCATION_DEBUG "${WUWE_INSTALL_PREFIX}/lib/wuwed.lib")
        set(REARK_WUWE_IMPORTED_LOCATION_DEBUG "${WUWE_INSTALL_PREFIX}/lib/wuwed.lib")
    endif()
    if(REARK_WUWE_IMPORTED_LOCATION_DEBUG AND REARK_WUWE_IMPORTED_LOCATION_RELEASE
        AND NOT REARK_WUWE_IMPORTED_LOCATION_DEBUG STREQUAL REARK_WUWE_IMPORTED_LOCATION_RELEASE)
        set(REARK_WUWE_CONFIG_EXPRESSION "1")
        message(STATUS "Wuwe Agent runtime enabled for all imported configurations")
    elseif(REARK_WUWE_IMPORTED_LOCATION_DEBUG)
        set(REARK_WUWE_CONFIG_EXPRESSION "$<CONFIG:Debug>")
        message(STATUS "Wuwe Agent runtime enabled for Debug")
    elseif(REARK_WUWE_IMPORTED_LOCATION_RELEASE)
        set(REARK_WUWE_CONFIG_EXPRESSION "$<NOT:$<CONFIG:Debug>>")
        message(STATUS "Wuwe Agent runtime enabled for non-Debug configurations")
    else()
        set(REARK_WUWE_CONFIG_EXPRESSION "0")
        message(STATUS "Wuwe Agent runtime disabled because the installed package exports no usable library")
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "$<${REARK_WUWE_CONFIG_EXPRESSION}:wuwe::wuwe>"
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<${REARK_WUWE_CONFIG_EXPRESSION}:REARK_HAS_WUWE=1>"
    )
    if(NOT REARK_ENABLE_WUWE_EXECUTION)
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            "$<${REARK_WUWE_CONFIG_EXPRESSION}:REARK_DISABLE_WUWE_EXECUTION=1>"
        )
    endif()
endif()

if(cmark-gfm_FOUND AND cmark-gfm-extensions_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        libcmark-gfm
        libcmark-gfm-extensions
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE REARK_HAS_CMARK_GFM=1)
endif()

if(unofficial-tree-sitter_FOUND)
    set(REARK_TREE_SITTER_GRAMMAR_ROOT "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-grammars")
    set(REARK_TREE_SITTER_JAVASCRIPT_ROOT "${REARK_TREE_SITTER_GRAMMAR_ROOT}/javascript")
    set(REARK_TREE_SITTER_TYPESCRIPT_ROOT "${REARK_TREE_SITTER_GRAMMAR_ROOT}/typescript/typescript")

    if(EXISTS "${REARK_TREE_SITTER_JAVASCRIPT_ROOT}/src/parser.c"
        AND EXISTS "${REARK_TREE_SITTER_TYPESCRIPT_ROOT}/src/parser.c")
        add_library(reark_tree_sitter_javascript STATIC
            "${REARK_TREE_SITTER_JAVASCRIPT_ROOT}/src/parser.c"
            "${REARK_TREE_SITTER_JAVASCRIPT_ROOT}/src/scanner.c"
        )
        target_include_directories(reark_tree_sitter_javascript PRIVATE
            "${REARK_TREE_SITTER_JAVASCRIPT_ROOT}/src"
        )
        set_target_properties(reark_tree_sitter_javascript PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED ON
            POSITION_INDEPENDENT_CODE ON
        )

        add_library(reark_tree_sitter_typescript STATIC
            "${REARK_TREE_SITTER_TYPESCRIPT_ROOT}/src/parser.c"
            "${REARK_TREE_SITTER_TYPESCRIPT_ROOT}/src/scanner.c"
        )
        target_include_directories(reark_tree_sitter_typescript PRIVATE
            "${REARK_TREE_SITTER_TYPESCRIPT_ROOT}/src"
        )
        set_target_properties(reark_tree_sitter_typescript PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED ON
            POSITION_INDEPENDENT_CODE ON
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            reark_tree_sitter_javascript
            reark_tree_sitter_typescript
        )
        if(MSVC)
            foreach(_reark_tree_sitter_grammar_target
                reark_tree_sitter_javascript
                reark_tree_sitter_typescript)
                target_compile_options(${_reark_tree_sitter_grammar_target} PRIVATE
                    /bigobj
                    /wd4244
                    /wd4267
                )
            endforeach()
        endif()
        message(STATUS "Tree-sitter JavaScript/TypeScript grammars enabled")
        target_compile_definitions(${PROJECT_NAME} PRIVATE REARK_HAS_TREE_SITTER_GRAMMARS=1)
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::tree-sitter::tree-sitter)
    target_compile_definitions(${PROJECT_NAME} PRIVATE REARK_HAS_TREE_SITTER=1)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/presentation>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)

qt_import_qml_plugins(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    REARK_VERSION="${PROJECT_VERSION}"
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Crypt32)

    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE $<$<CONFIG:Release>:TRUE>
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )

    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
    find_program(REARK_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${_qt_bin_dir}"
        REQUIRED
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${REARK_WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            $<$<CONFIG:Debug>:--debug>
            $<$<NOT:$<CONFIG:Debug>>:--release>
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/ui/qml"
            "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Deploying Qt runtime plugins"
    )

    if(TARGET OpenSSL::Crypto)
        get_target_property(_openssl_crypto_runtime
            OpenSSL::Crypto IMPORTED_LOCATION)
        if(NOT _openssl_crypto_runtime)
            get_target_property(_openssl_crypto_runtime
                OpenSSL::Crypto IMPORTED_LOCATION_RELEASE)
        endif()
        if(_openssl_crypto_runtime)
            get_filename_component(_openssl_crypto_runtime_ext
                "${_openssl_crypto_runtime}" EXT)
            if(NOT _openssl_crypto_runtime_ext STREQUAL ".dll")
                set(_openssl_crypto_implib "${_openssl_crypto_runtime}")
                unset(_openssl_crypto_runtime)
            endif()
        endif()
        if(NOT _openssl_crypto_runtime)
            if(NOT _openssl_crypto_implib)
                get_target_property(_openssl_crypto_implib
                    OpenSSL::Crypto IMPORTED_IMPLIB)
            endif()
            if(NOT _openssl_crypto_implib)
                get_target_property(_openssl_crypto_implib
                    OpenSSL::Crypto IMPORTED_IMPLIB_RELEASE)
            endif()
            if(_openssl_crypto_implib)
                get_filename_component(_openssl_root
                    "${_openssl_crypto_implib}/../.." ABSOLUTE)
                file(GLOB _openssl_crypto_candidates
                    "${_openssl_root}/bin/libcrypto-*.dll"
                    "${_openssl_root}/bin/libeay32.dll")
                if(_openssl_crypto_candidates)
                    list(GET _openssl_crypto_candidates 0
                        _openssl_crypto_runtime)
                endif()
            endif()
        endif()

        if(_openssl_crypto_runtime)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_openssl_crypto_runtime}"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )
        endif()
    endif()
endif()

set(REARK_BUNDLED_HDC_FILES
    "${CMAKE_SOURCE_DIR}/plugin/hdc/hdc.exe"
    "${CMAKE_SOURCE_DIR}/plugin/hdc/libusb_shared.dll"
)
if(WIN32)
    foreach(_reark_hdc_file IN LISTS REARK_BUNDLED_HDC_FILES)
        if(NOT EXISTS "${_reark_hdc_file}")
            message(FATAL_ERROR "Bundled HDC runtime file is required: ${_reark_hdc_file}")
        endif()
    endforeach()
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/plugin/hdc/hdc.exe")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugin/hdc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${REARK_BUNDLED_HDC_FILES}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugin/hdc"
        COMMENT "Copying bundled HDC runtime"
    )
endif()

set(REARK_HARMONY_TOOL_FILES
    "${CMAKE_SOURCE_DIR}/plugin/harmony-tools/lib/hap-sign-tool.jar"
    "${CMAKE_SOURCE_DIR}/plugin/harmony-tools/lib/app_packing_tool.jar"
)
foreach(_reark_harmony_tool_file IN LISTS REARK_HARMONY_TOOL_FILES)
    if(NOT EXISTS "${_reark_harmony_tool_file}")
        message(FATAL_ERROR "Bundled Harmony signing tool is required: ${_reark_harmony_tool_file}")
    endif()
endforeach()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugin/harmony-tools/lib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${REARK_HARMONY_TOOL_FILES}
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugin/harmony-tools/lib"
    COMMENT "Copying bundled Harmony signing tools"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
        $<$<CONFIG:MinSizeRel>:/O1>
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2>
        $<$<CONFIG:MinSizeRel>:-Os>
    )
endif()

set(REARK_ABC_EVIDENCE_SAMPLE "${CMAKE_SOURCE_DIR}/samples/Bridge.hap")
if(NOT EXISTS "${REARK_ABC_EVIDENCE_SAMPLE}")
    set(REARK_ABC_EVIDENCE_SAMPLE "${CMAKE_SOURCE_DIR}/samples/遥遥领先.hap")
endif()
set(REARK_HARMONY_PACKING_TOOL "${CMAKE_SOURCE_DIR}/plugin/harmony-tools/lib/app_packing_tool.jar")

if(BUILD_TESTING AND EXISTS "${REARK_ABC_EVIDENCE_SAMPLE}")
    qt_add_executable(reark_hyle_abc_evidence_test
        tests/HyleAbcEvidenceTest.cpp
    )
    target_include_directories(reark_hyle_abc_evidence_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_definitions(reark_hyle_abc_evidence_test PRIVATE
        REARK_ABC_EVIDENCE_SAMPLE="${REARK_ABC_EVIDENCE_SAMPLE}"
    )
    target_link_libraries(reark_hyle_abc_evidence_test PRIVATE
        reark_core
    )
    if(WIN32)
        add_custom_command(TARGET reark_hyle_abc_evidence_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_hyle_abc_evidence_test>
                $<TARGET_FILE_DIR:reark_hyle_abc_evidence_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_hyle_abc_evidence_test
        COMMAND reark_hyle_abc_evidence_test "${REARK_ABC_EVIDENCE_SAMPLE}")

    qt_add_executable(reark_abc_evidence_dump
        tests/AbcEvidenceDump.cpp
    )
    target_include_directories(reark_abc_evidence_dump PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_abc_evidence_dump PRIVATE
        reark_core
    )
    if(WIN32)
        add_custom_command(TARGET reark_abc_evidence_dump POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_abc_evidence_dump>
                $<TARGET_FILE_DIR:reark_abc_evidence_dump>
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()

if(BUILD_TESTING)
    qt_add_executable(reark_command_runner_test
        tests/CommandRunnerTest.cpp
    )
    target_include_directories(reark_command_runner_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_command_runner_test PRIVATE
        reark_core
    )
    if(WIN32)
        add_custom_command(TARGET reark_command_runner_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_command_runner_test>
                $<TARGET_FILE_DIR:reark_command_runner_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_command_runner_test
        COMMAND reark_command_runner_test)

    qt_add_executable(reark_release_notes_localizer_test
        tests/ReleaseNotesLocalizerTest.cpp
        controller/ReleaseNotesLocalizer.cpp
        controller/ReleaseNotesLocalizer.h
    )
    target_include_directories(reark_release_notes_localizer_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_release_notes_localizer_test PRIVATE
        Qt6::Core
    )
    if(WIN32)
        add_custom_command(TARGET reark_release_notes_localizer_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_release_notes_localizer_test>
                $<TARGET_FILE_DIR:reark_release_notes_localizer_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_release_notes_localizer_test
        COMMAND reark_release_notes_localizer_test)

    qt_add_executable(reark_agent_settings_test
        tests/AgentSettingsTest.cpp
        controller/AgentSettings.cpp
        controller/AgentSettings.h
        controller/ProtectedSettingsSecret.cpp
        controller/ProtectedSettingsSecret.h
    )
    target_include_directories(reark_agent_settings_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_agent_settings_test PRIVATE
        Qt6::Core
    )
    if(WIN32)
        target_link_libraries(reark_agent_settings_test PRIVATE Crypt32)
        add_custom_command(TARGET reark_agent_settings_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_agent_settings_test>
                $<TARGET_FILE_DIR:reark_agent_settings_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_agent_settings_test
        COMMAND reark_agent_settings_test)

    qt_add_executable(reark_agent_prompt_contract_test
        tests/AgentPromptContractTest.cpp
    )
    target_include_directories(reark_agent_prompt_contract_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_agent_prompt_contract_test PRIVATE
        Qt6::Core
    )
    target_compile_definitions(reark_agent_prompt_contract_test PRIVATE
        REARK_AGENT_CONTROLLER_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/controller/AgentController.cpp"
        REARK_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )
    if(WIN32)
        add_custom_command(TARGET reark_agent_prompt_contract_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_agent_prompt_contract_test>
                $<TARGET_FILE_DIR:reark_agent_prompt_contract_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_agent_prompt_contract_test
        COMMAND reark_agent_prompt_contract_test)

    qt_add_executable(reark_agent_request_router_test
        tests/AgentRequestRouterTest.cpp
        controller/AgentRequestRouter.cpp
        controller/AgentRequestRouter.h
    )
    target_include_directories(reark_agent_request_router_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_agent_request_router_test PRIVATE
        Qt6::Core
    )
    if(WIN32)
        add_custom_command(TARGET reark_agent_request_router_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_agent_request_router_test>
                $<TARGET_FILE_DIR:reark_agent_request_router_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_agent_request_router_test
        COMMAND reark_agent_request_router_test)

    qt_add_executable(reark_hdc_device_backend_test
        tests/HdcDeviceBackendTest.cpp
    )
    target_include_directories(reark_hdc_device_backend_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_hdc_device_backend_test PRIVATE
        reark_core
    )
    if(WIN32)
        add_custom_command(TARGET reark_hdc_device_backend_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_hdc_device_backend_test>
                $<TARGET_FILE_DIR:reark_hdc_device_backend_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_hdc_device_backend_test
        COMMAND reark_hdc_device_backend_test)

    qt_add_executable(reark_ui_automation_backend_test
        tests/UiAutomationBackendTest.cpp
    )
    target_include_directories(reark_ui_automation_backend_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_ui_automation_backend_test PRIVATE
        reark_core
    )
    if(WIN32)
        add_custom_command(TARGET reark_ui_automation_backend_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_ui_automation_backend_test>
                $<TARGET_FILE_DIR:reark_ui_automation_backend_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_ui_automation_backend_test
        COMMAND reark_ui_automation_backend_test)

    qt_add_executable(reark_device_runtime_controller_test
        tests/DeviceRuntimeControllerTest.cpp
        controller/DeviceRuntimeController.cpp
        controller/DeviceRuntimeController.h
        controller/ProtectedSettingsSecret.cpp
        controller/ProtectedSettingsSecret.h
        controller/SigningSettings.cpp
        controller/SigningSettings.h
    )
    target_include_directories(reark_device_runtime_controller_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_device_runtime_controller_test PRIVATE
        reark_core
        Qt6::Concurrent
    )
    if(WIN32)
        target_link_libraries(reark_device_runtime_controller_test PRIVATE Crypt32)
    endif()
    if(WIN32)
        add_custom_command(TARGET reark_device_runtime_controller_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_device_runtime_controller_test>
                $<TARGET_FILE_DIR:reark_device_runtime_controller_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    if(EXISTS "${REARK_ABC_EVIDENCE_SAMPLE}" AND EXISTS "${REARK_HARMONY_PACKING_TOOL}")
        add_test(NAME reark_device_runtime_controller_test
            COMMAND reark_device_runtime_controller_test
                "${REARK_ABC_EVIDENCE_SAMPLE}"
                "${REARK_HARMONY_PACKING_TOOL}")
    endif()

    qt_add_executable(reark_harmony_hap_signer_test
        tests/HarmonyHapSignerTest.cpp
    )
    target_include_directories(reark_harmony_hap_signer_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_harmony_hap_signer_test PRIVATE
        reark_core
    )
    if(WIN32)
        add_custom_command(TARGET reark_harmony_hap_signer_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_harmony_hap_signer_test>
                $<TARGET_FILE_DIR:reark_harmony_hap_signer_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME reark_harmony_hap_signer_test
        COMMAND reark_harmony_hap_signer_test)

    qt_add_executable(reark_harmony_package_rewriter_test
        tests/HarmonyPackageRewriterTest.cpp
    )
    target_include_directories(reark_harmony_package_rewriter_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(reark_harmony_package_rewriter_test PRIVATE
        reark_core
    )
    if(WIN32)
        add_custom_command(TARGET reark_harmony_package_rewriter_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:reark_harmony_package_rewriter_test>
                $<TARGET_FILE_DIR:reark_harmony_package_rewriter_test>
            COMMAND_EXPAND_LISTS
        )
    endif()
    if(EXISTS "${REARK_ABC_EVIDENCE_SAMPLE}" AND EXISTS "${REARK_HARMONY_PACKING_TOOL}")
        add_test(NAME reark_harmony_package_rewriter_test
            COMMAND reark_harmony_package_rewriter_test
                "${REARK_ABC_EVIDENCE_SAMPLE}"
                "${REARK_HARMONY_PACKING_TOOL}")
    endif()
endif()
