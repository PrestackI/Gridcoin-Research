# The QML front end carried from Ziggy Schulz's qml branch of
# ZiggySchulz/Gridcoin-Research. His branch defined MMPTheme and CurrentTime
# in src/qt/CMakeLists.txt and served its root files from qml/qml.qrc;
# neither file is carried. This file ports the two module definitions, and
# gathers the root files into a third module, Gridcoin.App, in the form the
# Gridcoin.Qml module uses: a STATIC backing library created first, with
# AUTOMOC set before qt_add_qml_module (Qt 6.4 reads it immediately), and no
# STATIC keyword on qt_add_qml_module (it rejects one for an existing target).
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(COMMAND qt_policy AND QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# MMPTheme: the theme singleton and the controls it styles.
set_source_files_properties(plugins/MMPTheme/MMPTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(gridcoinqmltheme STATIC)
set_target_properties(gridcoinqmltheme PROPERTIES AUTOMOC ON)
qt_add_qml_module(gridcoinqmltheme
    URI MMPTheme
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    PLUGIN_TARGET gridcoinqmlthemeplugin
    QML_FILES
        plugins/MMPTheme/MMPTheme.qml
        MMPControls/Button.qml
        MMPControls/CheckBox.qml
        MMPControls/ComboBox.qml
        MMPControls/GroupBox.qml
        MMPControls/RadioButton.qml
        MMPControls/ScrollBar.qml
        MMPControls/ScrollIndicator.qml
        MMPControls/SpinBox.qml
        MMPControls/TextArea.qml
        MMPControls/TextField.qml
)

# CurrentTime: the clock singleton.
set_source_files_properties(plugins/CurrentTime/CurrentTime.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(gridcoinqmltime STATIC)
set_target_properties(gridcoinqmltime PROPERTIES AUTOMOC ON)
qt_add_qml_module(gridcoinqmltime
    URI CurrentTime
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    PLUGIN_TARGET gridcoinqmltimeplugin
    QML_FILES
        plugins/CurrentTime/CurrentTime.qml
)

# Gridcoin.App: the root (WindowManager.qml) and the screens and components it creates.
qt_add_library(gridcoinqmlapp STATIC)
set_target_properties(gridcoinqmlapp PROPERTIES AUTOMOC ON)
qt_add_qml_module(gridcoinqmlapp
    URI Gridcoin.App
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    PLUGIN_TARGET gridcoinqmlappplugin
    QML_FILES
        AboutWindow.qml
        Circle.qml
        ClickableLink.qml
        ColorizableImage.qml
        CoreMessageWindow.qml
        DebugConsoleWindow.qml
        EditAddressDialog.qml
        FavoritesView.qml
        HelpHover.qml
        HistoryView.qml
        InfoPopup.qml
        MainWindow.qml
        MenuButton.qml
        OnboardingButton.qml
        OnboardingPageIndicator.qml
        OnboardingWindow.qml
        OverviewView.qml
        PanelTitle.qml
        PollCreationWindow.qml
        PollDetailsWindow.qml
        PollTabButton.qml
        PollView.qml
        PollVoteWindow.qml
        QRCodeWindow.qml
        ReceiveView.qml
        RecentTransactionItem.qml
        SearchBox.qml
        SendView.qml
        SettingsView.qml
        SplashScreen.qml
        StatusFooter.qml
        TabMenu.qml
        TableFooter.qml
        TableHeader.qml
        UnlockDialog.qml
        WindowManager.qml
)

# The icons the screens name by path under icons/, and the fonts they name by
# family, served from assets/ at the resource root (the fonts under :/fonts/,
# where the front end registers its fonts from). The licence texts beside the
# fonts are resources too, so the binary carries them; nothing displays them
# yet.
file(GLOB_RECURSE GRIDCOIN_QML_ASSETS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.otf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.txt"
)
list(SORT GRIDCOIN_QML_ASSETS)
qt_add_resources(gridcoinqmlapp "qml_assets"
    PREFIX "/"
    BASE assets
    FILES ${GRIDCOIN_QML_ASSETS}
)

# Against a static Qt, such as the depends build's, the QML plugins of the
# modules these files import (QtQuick and its controls, among others) are
# linked only when a CMake call names them, and the engine cannot resolve
# those imports otherwise. qt_import_qml_plugins scans the QML files in the
# source directory of the target it is given and links the plugins it finds
# into that target, INTERFACE for a static library. The scan targets are the
# Gridcoin.App and MMPTheme libraries, whose QML files live here;
# gridcoinresearch-qml and test_gridcoin-qml link both and so inherit the
# plugins. The executable is not a scan target, because its own source
# directory, src/qt/quick, holds none of these files. The calls are deferred
# to the end of this directory so that they run after the qmldir writes that
# qt_add_qml_module defers. A shared Qt loads its plugins from its own install
# and needs neither call.
get_target_property(GRIDCOIN_QT_CORE_TYPE ${QT}::Core TYPE)
if(GRIDCOIN_QT_CORE_TYPE STREQUAL "STATIC_LIBRARY")
    cmake_language(DEFER CALL qt_import_qml_plugins gridcoinqmlapp)
    cmake_language(DEFER CALL qt_import_qml_plugins gridcoinqmltheme)
endif()
