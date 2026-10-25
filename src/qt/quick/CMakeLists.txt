# Gridcoin.Qml: the C++ side of the QML front end. Form: a STATIC backing
# library created first, with AUTOMOC set before qt_add_qml_module (Qt 6.4
# reads it immediately), and no STATIC keyword on qt_add_qml_module (it
# rejects one for an existing target).
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

qt_add_library(gridcoinqml STATIC)
set_target_properties(gridcoinqml PROPERTIES AUTOMOC ON)
qt_add_qml_module(gridcoinqml
    URI Gridcoin.Qml
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    PLUGIN_TARGET gridcoinqmlplugin
    SOURCES
        qmlcallrunner.cpp
        qmlcallrunner.h
        qmlengine.cpp
        qmlengine.h
        qmlfonts.cpp
        qmlfonts.h
        qmlfrontend.cpp
        qmlfrontend.h
        qmlprovider.cpp
        qmlprovider.h
        shelladapter.cpp
        shelladapter.h
        splashadapter.cpp
        splashadapter.h
)
target_include_directories(gridcoinqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gridcoinqml PUBLIC
    gridcoinqt
    ${QT}::Qml
    ${QT}::Quick
)

# The QML front end's executable: the GUI composition root (gridcoinguimain)
# with QmlFrontEnd and the carried QML modules. Built only with ENABLE_QML.
add_executable(gridcoinresearch-qml qml_main.cpp)
target_link_libraries(gridcoinresearch-qml PRIVATE
    ${RUNTIME_LIBS}
    gridcoinguimain
    gridcoinqml
    gridcoinqmlplugin
    gridcoinqmlapp
    gridcoinqmlappplugin
    gridcoinqmltheme
    gridcoinqmlthemeplugin
    gridcoinqmltime
    gridcoinqmltimeplugin
    ${QT}::Qml
    ${QT}::Quick
    ${QT}::Widgets
    CURL::libcurl
    gridcoin_util
    gridcoinqt
)
if(ENABLE_MULTIPROCESS)
    target_link_libraries(gridcoinresearch-qml PRIVATE gridcoin_ipc)
endif()
set_target_properties(gridcoinresearch-qml PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# macOS: gridcoinresearch-qml is an app bundle with its own executable name and
# identifier (share/qt/Info.plist.qml.cmake.in), and deploy_qml makes its DMG as
# deploy makes the Widgets one, with the QML modules the carried QML imports
# deployed into the bundle (macdeployqtplus -qmldir). It reads the Python,
# qtpaths and Qt directory variables that deploy's block in the parent
# directory sets before this directory is added.
if(APPLE)
    set(_qml_bundle_icon "${CMAKE_SOURCE_DIR}/src/qt/res/icons/gridcoin.icns")
    target_sources(gridcoinresearch-qml PRIVATE ${_qml_bundle_icon})
    set_target_properties(gridcoinresearch-qml PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/share/qt/Info.plist.qml.cmake.in"
        RESOURCE ${_qml_bundle_icon}
    )

    set(_qml_app_bundle "$<TARGET_BUNDLE_DIR:gridcoinresearch-qml>")
    set(_qml_dmg_name "gridcoinresearch-qml-${PROJECT_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}")
    set(_deploy_qml_cmd
        ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/contrib/macdeploy/macdeployqtplus"
        "${_qml_app_bundle}" Gridcoin-QML
        -binary gridcoinresearch-qml
        -dmg "${CMAKE_BINARY_DIR}/${_qml_dmg_name}"
        -qmldir "${CMAKE_SOURCE_DIR}/src/qt/qml"
    )
    if(_qt_plugins_dir AND EXISTS "${_qt_plugins_dir}")
        list(APPEND _deploy_qml_cmd -plugin-dir "${_qt_plugins_dir}")
    endif()
    if(_qt_translations_dir AND EXISTS "${_qt_translations_dir}")
        list(APPEND _deploy_qml_cmd -translations-dir "${_qt_translations_dir}")
    endif()
    if(DEFINED ENV{APPLE_SIGNING_IDENTITY})
        set(_qml_entitlements "${CMAKE_SOURCE_DIR}/contrib/macdeploy/entitlements.plist")
        if(EXISTS "${_qml_entitlements}")
            list(APPEND _deploy_qml_cmd -entitlements "${_qml_entitlements}")
        endif()
    endif()

    set(_deploy_qml_env "QTDIR=${_qt_prefix_dir}")
    if(QTPATHS_EXECUTABLE)
        list(APPEND _deploy_qml_env "QTPATHS=${QTPATHS_EXECUTABLE}")
    endif()

    # The daemon goes into Contents/Helpers before macdeployqtplus runs, as in
    # deploy, whose comment gives the reasons. macdeployqtplus stages its dist
    # directory in the directory it runs in and deletes it first, so deploy_qml
    # runs it in a directory of its own and deploy's staging is left alone; the
    # DMG still lands in the build directory.
    set(_deploy_qml_helper_commands)
    set(_deploy_qml_helper_depends)
    if(TARGET gridcoinresearchd)
        set(_deploy_qml_helper_commands
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_qml_app_bundle}/Contents/Helpers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:gridcoinresearchd>"
                    "${_qml_app_bundle}/Contents/Helpers/")
        set(_deploy_qml_helper_depends gridcoinresearchd)
    endif()

    add_custom_target(deploy_qml
        ${_deploy_qml_helper_commands}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/deploy_qml"
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/deploy_qml" ${CMAKE_COMMAND} -E env ${_deploy_qml_env} ${_deploy_qml_cmd}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Deploying ${_qml_dmg_name}.dmg via macdeployqtplus"
        DEPENDS gridcoinresearch-qml ${_deploy_qml_helper_depends}
    )
endif()

if(ENABLE_TESTS)
    add_subdirectory(test)
endif()
