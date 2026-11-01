/*
    A view containing the wallet settings
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0
import Gridcoin.Qml

Rectangle{
    id: main
    color: MMPTheme.backgroundColor
    property bool settingsChanged: Settings.dirty
    // Read again each time the screen opens; what was staged and not saved
    // is dropped when it closes.
    Component.onCompleted: { Settings.reload(); Settings.sideStakes.refresh() }
    Component.onDestruction: Settings.discard()
    Rectangle {
        id: settingsTabMenuRect
        color: MMPTheme.headerColor
        height: 70
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        Rectangle {
            id: headerBottomBorderRect
            color: MMPTheme.themeSelect("transparent", MMPTheme.cBlack)
            height: 1
            width: parent.width
            anchors.bottom: parent.bottom
        }
        RowLayout {
            id: tabMenuRowLayout
            objectName: "settingsTabRow"
            anchors.centerIn: parent
            spacing: 30
            property Item currentItem: generalSettingsTabButton
            Shortcut {
                objectName: "nextSettingsPageShortcut"
                sequences: [StandardKey.MoveToNextPage]
                onActivated: {
                    switch(tabMenuRowLayout.currentItem) {
                    case generalSettingsTabButton:
                        tabMenuRowLayout.currentItem = networkSettingsTabButton
                        break
                    case networkSettingsTabButton:
                        tabMenuRowLayout.currentItem = windowSettingsTabButton
                        break
                    case windowSettingsTabButton:
                        tabMenuRowLayout.currentItem = displaySettingsTabButton
                        break
                    case displaySettingsTabButton:
                        break
                    case nodesSettingsTabButton:
                        break
                    }
                }
            }
            Shortcut {
                sequences: [StandardKey.MoveToPreviousPage]
                onActivated: {
                    switch(tabMenuRowLayout.currentItem) {
                    case generalSettingsTabButton:
                        break
                    case networkSettingsTabButton:
                        tabMenuRowLayout.currentItem = generalSettingsTabButton
                        break
                    case windowSettingsTabButton:
                        tabMenuRowLayout.currentItem = networkSettingsTabButton
                        break
                    case displaySettingsTabButton:
                        tabMenuRowLayout.currentItem = windowSettingsTabButton
                        break
                    case nodesSettingsTabButton:
                        tabMenuRowLayout.currentItem = displaySettingsTabButton
                        break
                    }
                }
            }

            MouseArea {
                id: generalSettingsTabButton
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==generalSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = generalSettingsTabButton
                Image {
                    id: generalSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_general_light.svg", "/icons/tabs/ic_tab_general_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: generalSettingsLabel
                    text: qsTr("General")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==generalSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: generalSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: networkSettingsTabButton
                objectName: "networkSettingsTabButton"
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==networkSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = networkSettingsTabButton
                Image {
                    id: networkSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_network_light.svg", "/icons/tabs/ic_tab_network_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: networkSettingsLabel
                    text: qsTr("Network")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==networkSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: networkSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: windowSettingsTabButton
                objectName: "windowSettingsTabButton"
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==windowSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = windowSettingsTabButton
                Image {
                    id: windowSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_window_light.svg", "/icons/tabs/ic_tab_window_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: windowSettingsLabel
                    text: qsTr("Window")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==windowSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: windowSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            MouseArea {
                id: displaySettingsTabButton
                objectName: "displaySettingsTabButton"
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==displaySettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = displaySettingsTabButton
                Image {
                    id: displaySettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_display_light.svg", "/icons/tabs/ic_tab_display_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: displaySettingsLabel
                    text: qsTr("Display")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==displaySettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: displaySettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }

            }
            // No Widgets counterpart yet; hidden until one exists.
            MouseArea {
                id: nodesSettingsTabButton
                visible: false
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: tabMenuRowLayout.currentItem==nodesSettingsTabButton ? 1 : 0.4
                onClicked: tabMenuRowLayout.currentItem = nodesSettingsTabButton
                Image {
                    id: nodesSettingsIcon
                    sourceSize: Qt.size(30,30)
                    source: MMPTheme.themeSelect("/icons/tabs/ic_tab_nodes_light.svg", "/icons/tabs/ic_tab_nodes_dark.svg")
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                }
                Text {
                    id: nodesSettingsLabel
                    text: qsTr("Nodes")
                    color: MMPTheme.textColor
                    font.weight:  tabMenuRowLayout.currentItem==nodesSettingsTabButton ? Font.DemiBold : Font.Medium
                    anchors {
                        top: nodesSettingsIcon.bottom
                        topMargin: 5
                        horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
        MouseArea {
            id: debugConsoleButton
            height: 50
            width: 50
            opacity: pressed ? 1 : 0.4
            onPressed: {
                var component = Qt.createComponent("DebugConsoleWindow.qml")
                var windowObj = component.createObject(window)
                windowObj.show()
            }

            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }

            Image {
                id: debugIcon
                sourceSize: Qt.size(25,25)
                source: MMPTheme.themeSelect("qrc:/icons/tabs/ic_tab_more_light.svg", "qrc:/icons/tabs/ic_tab_more_dark.svg")
                anchors.centerIn: parent
            }
        }
    }
    Rectangle {
        id: body
        color: MMPTheme.bodyColor
        radius: 4
        anchors {
            top: settingsTabMenuRect.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 10
        }
        Loader {
            id: settingsContentLoader
            sourceComponent: {
                switch(tabMenuRowLayout.currentItem) {
                case generalSettingsTabButton:
                    return generalSettingsView
                case networkSettingsTabButton:
                    return networkSettingsView
                case windowSettingsTabButton:
                    return windowSettingsView
                case displaySettingsTabButton:
                    return displaySettingsView
                case nodesSettingsTabButton:
                    return nodesSettingsView
                }
            }
            onSourceComponentChanged: if(item && item.defaultFocusItem) item.defaultFocusItem.forceActiveFocus()
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                bottom: bottomAccentRect.top
            }
        }
        Rectangle {
            id: bottomAccentRect
            height: 40
            color: MMPTheme.secondaryBodyColor
            radius: 4
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            Rectangle {
                id: accentFillRect
                color: MMPTheme.secondaryBodyColor
                width: parent.width
                height: 8
            }
            Button {
                id: saveButton
                objectName: "settingsSaveButton"
                text: qsTr("Save")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_save_light.svg", "qrc:/icons/buttons/ic_btn_save_dark.svg")
                enabled: settingsChanged
                onClicked: Settings.apply()
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: 20
                }
            }
            Button {
                id: discardButton
                objectName: "settingsDiscardButton"
                text: qsTr("Discard")
                icon.source: MMPTheme.themeSelect("qrc:/icons/buttons/ic_btn_discard_light.svg", "qrc:/icons/buttons/ic_btn_discard_dark.svg")
                enabled: settingsChanged
                onClicked: Settings.discard()
                anchors {
                    right: saveButton.left
                    verticalCenter: parent.verticalCenter
                    rightMargin: 10
                }
            }
            Label {
                id: settingsStatusLabel
                objectName: "settingsStatusLabel"
                text: Settings.lastError || (Settings.restartRequired ? qsTranslate("OptionsDialog", "This setting will take effect after restarting Gridcoin.") : "")
                wrapMode: Text.WordWrap
                color: MMPTheme.textColor
                anchors {
                    left: parent.left
                    right: discardButton.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: 20
                    rightMargin: 10
                }
            }
        }
    }

    Component {
        id: generalSettingsView
        ScrollView {
            id: generalSettingsScrollView
            property Item defaultFocusItem: reserveSpinbox
            anchors.fill: parent
            clip: true
            contentWidth: availableWidth
            contentHeight: contentColumn.implicitHeight + contentColumn.anchors.topMargin +
                           sideStakeRect.implicitHeight + sideStakeRect.anchors.topMargin +
                           secondContentGrid.implicitHeight + secondContentGrid.anchors.topMargin + 20
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            GridLayout {
                id: contentColumn
                rowSpacing: 20
                columnSpacing: 10
                columns: 3
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                // No Widgets counterpart yet; hidden until one exists.
                Text {
                    id: transactionFeeLabel
                    visible: false
                    text: qsTr("Pay Transaction Fee:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                // No Widgets counterpart yet; hidden until one exists.
                SpinBox {
                    id: transactionFeeSpinbox
                    visible: false
                    property int decimals: 8
                    property real factor: Math.pow(10, decimals)
                    KeyNavigation.tab: reserveSpinbox
                    Layout.preferredWidth: 120
                    editable: true
                    from: 0
                    to: factor * 10
                    value: factor/100
                    stepSize: factor/1000 //0.01
                    validator: DoubleValidator{
                        bottom: Math.min(transactionFeeSpinbox.from, transactionFeeSpinbox.to)
                        top:  Math.max(transactionFeeSpinbox.from, transactionFeeSpinbox.to)
                        notation: DoubleValidator.StandardNotation
                        decimals: 8
                    }
                    textFromValue: function(value, locale) {
                        return Number(value / factor).toLocaleString(locale, 'f', transactionFeeSpinbox.decimals)
                    }
                    valueFromText: function(text, locale) {
                        return Number.fromLocaleString(locale, text) * factor
                    }
                }
                // No Widgets counterpart yet; hidden until one exists.
                Item {
                    //This item sets the last column to fill the width
                    visible: false
                    Layout.fillWidth: true
                    HelpHover {
                        id: transactionFeeHelp
                        text: qsTr("Optional transaction fee per kB that helps ensure your transactions are processed quickly. A fee of 0.01 GRC/kB is recommmended.")
                        popupWidth: 200
                        horiontalPadding: 10
                        verticalPadding: 10
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    id: reserveLabel
                    text: qsTr("Reserve Amount:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                // The reserve as text in the display unit: Apply refuses what
                // the unit's parser rejects, so there is no validator here.
                TextField {
                    id: reserveSpinbox
                    objectName: "reserveField"
                    Layout.preferredWidth: 120
                    text: Settings.reserveBalance
                    onTextEdited: Settings.reserveBalance = text
                    KeyNavigation.tab: {
                        if (sideStakingCheckbox.checked && sideStakeListView.count>0) {
                            var it = sideStakeListView.itemAtIndex(0)
                            return it ? it.addressTextField : utxoOptimisationCheckbox
                        }
                        return utxoOptimisationCheckbox
                    }
                }
                HelpHover {
                    id: reserveHelp
                    text: qsTr("Reserve amount does not participate in staking and hence is spendable at any time")
                    popupWidth: 200
                    horiontalPadding: 10
                    verticalPadding: 10
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                }
                CheckBox {
                    id: enableStakingCheckbox
                    text: qsTranslate("OptionsDialog", "Enable Staking")
                    checked: Settings.enableStaking
                    onToggled: Settings.enableStaking = checked
                    Layout.columnSpan: 3
                }
                CheckBox {
                    id: sideStakingCheckbox
                    text: qsTr("Enable Sidestaking")
                    enabled: true
                    checked: Settings.enableSideStaking
                    onToggled: Settings.enableSideStaking = checked
                    KeyNavigation.tab: {
                        if (checked && sideStakeListView.count>0) {
                            var it = sideStakeListView.itemAtIndex(0)
                            return it ? it.addressTextField : utxoOptimisationCheckbox
                        }
                        return utxoOptimisationCheckbox
                    }
                }
                HelpHover {
                    id: sideStakingHelp
                    text: qsTr("Side staking allocates a proportion of your staking rewards to other addresses")
                    popupWidth: 200
                    horiontalPadding: 10
                    verticalPadding: 10
                    Layout.columnSpan: 2
                }
            }
            Rectangle {
                id: sideStakeRect
                color: "transparent"
                border.color: MMPTheme.lightBorderColor
                radius: 4
                implicitHeight: 25 + 25 + 6*25
                anchors {
                    top: contentColumn.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                TableHeader {
                    id: sideStakeHeader
                    x:1
                    y:1
                    height: 24
                    width: parent.width-2
                    radius: parent.radius
                    borderColor: sideStakeRect.border.color
                    model: [{text: qsTr("Side Stake Address"), width: sideStakeHeader.width*0.45},
                            {text: qsTr("Percentage"), width: sideStakeHeader.width*0.15},
                            {text: qsTranslate("SideStakeTableModel", "Description"), width: sideStakeHeader.width*0.25},
                            {text: qsTranslate("SideStakeTableModel", "Status"), width: sideStakeHeader.width*0.15}]
                }
                ListView {
                    id: sideStakeListView
                    objectName: "sideStakeListView"
                    clip: true
                    currentIndex: 0
                    interactive: true
                    anchors {
                        top: sideStakeHeader.bottom
                        left: parent.left
                        right: parent.right
                        bottom: sideStakeFooter.top
                        leftMargin: 1
                        rightMargin: 1
                    }
                    model: Settings.sideStakes
                    delegate: Rectangle {
                        width: sideStakeListView.width
                        height: 25
                        color: sideStakeListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                        radius: 4
                        property Item addressTextField: addressTextField
                        property Item percentageTextField: percentageTextField
                        property Item descriptionTextField: descriptionTextField
                        // A mandatory sidestake is shown but cannot be edited.
                        property bool editable: model.editable
                        Row {
                            height: parent.height
                            width: parent.width
                            Item {
                                id: addressItem
                                height: parent.height
                                width: 0.45 * parent.width
                                // An address is not editable, as in the Widgets edit dialog.
                                TextField {
                                    id: addressTextField
                                    objectName: "addressField"
                                    text: model.address
                                    readOnly: true
                                    onFocusChanged: {
                                        if (focus) {
                                            sideStakeListView.currentIndex = index
                                            forceActiveFocus()
                                        }
                                    }
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        verticalCenter: parent.verticalCenter
                                    }
                                    background: Item{}
                                    KeyNavigation.tab: percentageTextField
                                }
                            }
                            Item {
                                id: percentageItem
                                height: parent.height
                                width: 0.15 * parent.width
                                // No validator: the table reads a fractional allocation.
                                TextField {
                                    id: percentageTextField
                                    objectName: "allocationField"
                                    text: Number(model.allocation).toFixed(2)
                                    readOnly: !model.editable
                                    placeholderText: qsTr("Enter a percentage")
                                    onFocusChanged: {
                                        if (focus) {
                                            sideStakeListView.currentIndex = index
                                            forceActiveFocus()
                                        }
                                    }
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        verticalCenter: parent.verticalCenter
                                    }
                                    background: Item{}
                                    KeyNavigation.tab: descriptionTextField
                                }
                            }
                            Item {
                                id: descriptionItem
                                height: parent.height
                                width: 0.25 * parent.width
                                TextField {
                                    id: descriptionTextField
                                    text: model.description
                                    readOnly: !model.editable
                                    onFocusChanged: {
                                        if (focus) {
                                            sideStakeListView.currentIndex = index
                                            forceActiveFocus()
                                        }
                                    }
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        verticalCenter: parent.verticalCenter
                                    }
                                    background: Item{}
                                    KeyNavigation.tab: {
                                        if (index === sideStakeListView.count-1) return utxoOptimisationCheckbox
                                        var it = sideStakeListView.itemAtIndex(index+1)
                                        return it ? it.addressTextField : utxoOptimisationCheckbox
                                    }
                                }
                            }
                            Item {
                                id: statusItem
                                height: parent.height
                                width: 0.15 * parent.width
                                Text {
                                    id: statusText
                                    text: model.status
                                    color: MMPTheme.textColor
                                    elide: Text.ElideRight
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                    }
                }

                TableFooter {
                    id: sideStakeFooter
                    borderColor: sideStakeRect.border.color
                    RowLayout {
                        id: buttonRow
                        height: parent.height
                        width: parent.width
                        spacing: 0
                        Button {
                            id: addSSButton
                            objectName: "addSSButton"
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_add_light.svg","/icons/buttons/ic_btn_add_dark.svg")
                            background: Item{}
                            onClicked: {
                                if (Settings.sideStakes.add(newSideStakeAddressField.text,
                                                            newSideStakeAllocationField.text,
                                                            newSideStakeDescriptionField.text)) {
                                    newSideStakeAddressField.clear()
                                    newSideStakeAllocationField.clear()
                                    newSideStakeDescriptionField.clear()
                                }
                            }
                        }
                        Rectangle {
                            id: buttonSeparator
                            color: sideStakeRect.border.color
                            width: 1
                            Layout.fillHeight: true
                        }
                        // The footer sits outside the delegate, so it reads the
                        // current row through currentItem, not model.
                        Button {
                            id: removeSSButton
                            objectName: "removeSSButton"
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_remove_light.svg","/icons/buttons/ic_btn_remove_dark.svg")
                            background: Item{}
                            onClicked: Settings.sideStakes.remove(sideStakeListView.currentIndex)
                            enabled: sideStakeListView.currentItem !== null && sideStakeListView.currentItem.editable
                        }
                        Rectangle {
                            id: buttonSeparator2
                            color: sideStakeRect.border.color
                            width: 1
                            Layout.fillHeight: true
                        }
                        Button {
                            id: editSSButton
                            objectName: "editSSButton"
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_save_light.svg","/icons/buttons/ic_btn_save_dark.svg")
                            background: Item{}
                            onClicked: Settings.sideStakes.edit(sideStakeListView.currentIndex,
                                                                sideStakeListView.currentItem.percentageTextField.text,
                                                                sideStakeListView.currentItem.descriptionTextField.text)
                            enabled: sideStakeListView.currentItem !== null && sideStakeListView.currentItem.editable
                        }
                        Rectangle {
                            id: buttonSeparator3
                            color: sideStakeRect.border.color
                            width: 1
                            Layout.fillHeight: true
                        }
                        TextField {
                            id: newSideStakeAddressField
                            objectName: "newSideStakeAddressField"
                            Layout.preferredWidth: 0.35 * sideStakeFooter.width
                            Layout.fillHeight: true
                            placeholderText: qsTr("Enter a Gridcoin Address")
                            background: Item{}
                        }
                        TextField {
                            id: newSideStakeAllocationField
                            objectName: "newSideStakeAllocationField"
                            Layout.preferredWidth: 0.15 * sideStakeFooter.width
                            Layout.fillHeight: true
                            placeholderText: qsTr("Enter a percentage")
                            background: Item{}
                        }
                        TextField {
                            id: newSideStakeDescriptionField
                            objectName: "newSideStakeDescriptionField"
                            Layout.preferredWidth: 0.2 * sideStakeFooter.width
                            Layout.fillHeight: true
                            placeholderText: qsTranslate("SideStakeTableModel", "Description")
                            background: Item{}
                        }
                        Label {
                            id: sideStakeStatusLabel
                            objectName: "sideStakeStatusLabel"
                            text: Settings.sideStakes.lastError
                            wrapMode: Text.WordWrap
                            color: MMPTheme.textColor
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            GridLayout {
                id: secondContentGrid
                columns: 2
                rowSpacing: 20
                columnSpacing: 10
                anchors {
                    top: sideStakeRect.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                }
                CheckBox {
                    id: utxoOptimisationCheckbox
                    text: qsTr("Stake Splitting")
                    checked: Settings.enableStakeSplit
                    onToggled: Settings.enableStakeSplit = checked
                    KeyNavigation.tab: targetEfficiencyTextField
                }
                Item {
                    Layout.fillWidth: true
                    HelpHover {
                        id: utxoOptimisationHelpHover
                        text: qsTr("Optimises stakes to minimise cooldown times")
                        popupWidth: 200
                        horiontalPadding: 10
                        verticalPadding: 10
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }


                Text {
                    id: targetEfficiencyLabel
                    text: qsTr("Target Efficiency:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                TextField {
                    id: targetEfficiencyTextField
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.preferredWidth: 60
                    enabled: utxoOptimisationCheckbox.checked
                    KeyNavigation.tab: minSizeTextField
                    text: Settings.stakingEfficiency
                    onTextEdited: Settings.stakingEfficiency = text
                    validator: IntValidator{
                        bottom: 75
                        top: 98
                    }
                }
                Text {
                    id: minSizeLabel
                    text: qsTr("Minimum UTXO Size:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                TextField {
                    id: minSizeTextField
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.preferredWidth: 60
                    enabled: utxoOptimisationCheckbox.checked
                    KeyNavigation.tab: contractChangeCheckbox
                    text: Settings.minStakeSplitValue
                    onTextEdited: Settings.minStakeSplitValue = text
                    validator: IntValidator{
                        bottom: 800
                    }
                }
                CheckBox {
                    id: contractChangeCheckbox
                    text: qsTranslate("OptionsDialog", "Return change to an input address for contract transactions")
                    checked: Settings.contractChangeToInput
                    onToggled: Settings.contractChangeToInput = checked
                    Layout.columnSpan: 2
                    KeyNavigation.tab: startAtLoginCheckbox
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                }
                CheckBox {
                    id: startAtLoginCheckbox
                    objectName: "startAtLoginCheckbox"
                    text: qsTr("Start at system login")
                    visible: Settings.startupOptionsAvailable
                    checked: Settings.startAtStartup
                    onToggled: Settings.startAtStartup = checked
                    Layout.columnSpan: 2
                    KeyNavigation.tab: disableUpdateCheckBox
                }
                CheckBox {
                    id: startMinimizedCheckbox
                    text: qsTranslate("OptionsDialog", "Start minimized")
                    visible: Settings.startupOptionsAvailable
                    checked: Settings.startAtStartupMinimized
                    onToggled: Settings.startAtStartupMinimized = checked
                    Layout.columnSpan: 2
                }
                CheckBox {
                    id: disableUpdateCheckBox
                    objectName: "disableUpdateCheckBox"
                    text: qsTr("Disable update checks")
                    visible: Settings.updateCheckOptionAvailable
                    checked: Settings.disableUpdateCheck
                    onToggled: Settings.disableUpdateCheck = checked
                    KeyNavigation.tab: reserveSpinbox
                }
                HelpHover {
                    id: updateChecksHelpHover
                    visible: disableUpdateCheckBox.visible
                    text: qsTr("Increases user interface performance by reducing updates")
                    popupWidth: 200
                    horiontalPadding: 10
                    verticalPadding: 10
                }
                Button {
                    id: openConfigButton
                    text: qsTranslate("BitcoinGUI", "Open config &file...").replace("&", "")
                    enabled: Settings.hostPathsAvailable
                    onClicked: Settings.openConfigFile()
                    Layout.columnSpan: 2
                }
            }
        }
    }
    Component {
        id: networkSettingsView
        Item {
            property Item defaultFocusItem: upnpCheckbox
            GridLayout {
                id: contentColumn
                rowSpacing: 20
                columnSpacing: 10
                columns: 2
                width: Math.min(implicitWidth, parent.width-40)
                anchors {
                    top: parent.top
                    left: parent.left
                    topMargin: 20
                    leftMargin: 20
                }
                CheckBox {
                    id: upnpCheckbox
                    objectName: "upnpCheckbox"
                    text: qsTr("Map port using UPnP")
                    enabled: Settings.upnpAvailable
                    checked: Settings.mapPortUpnp
                    onToggled: Settings.mapPortUpnp = checked
                    KeyNavigation.tab: socksCheckbox
                }
                Item {
                    Layout.fillWidth: true
                    HelpHover {
                        id: upnpHelpHover
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                CheckBox {
                    id: socksCheckbox
                    text: qsTr("Connect through SOCKS proxy")
                    checked: Settings.proxyUse
                    onToggled: Settings.proxyUse = checked
                    KeyNavigation.tab: checked ? proxyIPTextField : upnpCheckbox
                }
                HelpHover {
                    id: socksHelpHover
                }
                RowLayout {
                    id: socksRow
                    Layout.columnSpan: 2
                    spacing: 10
                    Text {
                        id: proxyIPLabel
                        text: qsTr("Proxy IP: ")
                        color: MMPTheme.textColor
                    }
                    TextField {
                        id: proxyIPTextField
                        Layout.preferredWidth: 140
                        placeholderText: "255.255.255.255"
                        enabled: socksCheckbox.checked
                        text: Settings.proxyIp
                        onTextEdited: Settings.proxyIp = text
                    }
                    Text {
                        id: portLabel
                        text: qsTr("Port: ")
                        color: MMPTheme.textColor
                    }
                    TextField {
                        id: portTextField
                        placeholderText: "9050"
                        Layout.preferredWidth: 60
                        enabled: socksCheckbox.checked
                        text: Settings.proxyPort
                        // A port that is not a whole number stages 0, which
                        // Apply refuses.
                        onTextEdited: Settings.proxyPort = /^[0-9]{1,5}$/.test(text.trim()) ? parseInt(text.trim(), 10) : 0
                        KeyNavigation.tab: upnpCheckbox
                    }
                    // No Widgets counterpart yet; hidden until one exists.
                    Text {
                        id: socksVersionLabel
                        visible: false
                        text: qsTr("SOCKS version: ")
                        color: MMPTheme.textColor
                    }
                    // No Widgets counterpart yet; hidden until one exists.
                    ComboBox {
                        id: socksVersionComboBox
                        visible: false
                        Layout.preferredWidth: 40
                        model: [5, 4]
                        enabled: socksCheckbox.checked
                        KeyNavigation.tab: upnpCheckbox
                    }
                }
            }
        }
    }
    Component {
        id: windowSettingsView
        Item {
            property Item defaultFocusItem: minimiseToTrayCheckbox
            ColumnLayout {
                spacing: 20
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                CheckBox {
                    id: minimiseToTrayCheckbox
                    objectName: "minimiseToTrayCheckbox"
                    text: qsTr("Minimise to tray instead of the taskbar")
                    enabled: Settings.trayAvailable
                    checked: Settings.minimizeToTray
                    onToggled: Settings.minimizeToTray = checked
                    KeyNavigation.tab: minimiseOnCloseCheckbox
                }
                CheckBox {
                    id: minimiseOnCloseCheckbox
                    text: qsTr("Minimise on close")
                    enabled: Settings.trayAvailable
                    checked: Settings.minimizeOnClose
                    onToggled: Settings.minimizeOnClose = checked
                    KeyNavigation.tab: confirmOnCloseCheckbox
                }
                CheckBox {
                    id: confirmOnCloseCheckbox
                    objectName: "confirmOnCloseCheckbox"
                    text: qsTr("Confirm on close")
                    enabled: Settings.confirmOnCloseAvailable
                    checked: Settings.confirmOnClose
                    onToggled: Settings.confirmOnClose = checked
                    KeyNavigation.tab: disableTransactionNotificationCheckbox
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                }
                CheckBox {
                    id: disableTransactionNotificationCheckbox
                    text: qsTr("Disable transaction notifications")
                    checked: Settings.disableTrxNotifications
                    onToggled: Settings.disableTrxNotifications = checked
                    KeyNavigation.tab: disablePollNotificationCheckbox
                }
                CheckBox {
                    id: disablePollNotificationCheckbox
                    text: qsTr("Disable poll notifications")
                    checked: Settings.disablePollNotifications
                    onToggled: Settings.disablePollNotifications = checked
                    KeyNavigation.tab: minimiseToTrayCheckbox
                }
            }
        }
    }
    Component {
        id: displaySettingsView
        Item {
            property Item defaultFocusItem: languageCombobox
            GridLayout {
                id: contentGridView
                columns: 2
                rowSpacing: 20
                columnSpacing: 10
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    topMargin: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                Text {
                    id: languageLabel
                    text: qsTr("User interface language:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                ComboBox {
                    id: languageCombobox
                    model: Settings.languages
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: indexOfValue(Settings.language)
                    onActivated: Settings.language = currentValue
                    KeyNavigation.tab: themeCombobox
                }
                Text {
                    id: themeLabel
                    text: qsTr("Theme:")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                ComboBox {
                    id: themeCombobox
                    objectName: "themeCombobox"
                    currentIndex: Settings.theme === "light" ? 0 : 1
                    model: [qsTr("Light"), qsTr("Dark")]
                    onActivated: Settings.theme = (currentIndex === 0 ? "light" : "dark")
                    KeyNavigation.tab: unitCombobox
                }
                Text {
                    id: unitLabel
                    text: qsTranslate("OptionsDialog", "&Unit to show amounts in:").replace("&", "")
                    color: MMPTheme.textColor
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                ComboBox {
                    id: unitCombobox
                    model: Settings.displayUnits
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: indexOfValue(Settings.displayUnit)
                    onActivated: Settings.displayUnit = currentValue
                    KeyNavigation.tab: addressesInTransactionsCheckbox
                }
                Rectangle {
                    color: MMPTheme.separatorColor
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                }
                //                CheckBox {
                //                    id: displayTransactionsAfter
                //                    text: qsTr("Only display transactions from after date:")
                //                }
                //TODO Adapt DatePicker to select a single date instead of a range
                //                Button {
                //                    id: showDateSelectorButton
                //                    text: qsTr("Select Date")
                //                    icon.source: MMPTheme.themeSelect("qrc:/icons/generic/ic_date_light.svg","qrc:/icons/generic/ic_date_dark.svg")
                //                }
                CheckBox {
                    id: addressesInTransactionsCheckbox
                    text: qsTr("Display addresses in transactions list")
                    checked: Settings.displayAddresses
                    onToggled: Settings.displayAddresses = checked
                    Layout.columnSpan: 2
                    KeyNavigation.tab: coinControlCheckbox
                }
                CheckBox {
                    id: coinControlCheckbox
                    text: qsTr("Display advanced coin control features")
                    checked: Settings.coinControlFeatures
                    onToggled: Settings.coinControlFeatures = checked
                    Layout.columnSpan: 2
                    KeyNavigation.tab: languageCombobox
                }
                //Set the second column to fill the width
                Item {
                    Layout.preferredHeight: 1
                }
                Item {
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                }
            }
        }
    }
    Component {
        id: nodesSettingsView
        Item {

            Text {
                id: addNodeInfoLabel
                text: qsTr("Add nodes are nodes the wallet connects to on startup. It is not usually necessary to change these")
                wrapMode: Text.WordWrap
                color: MMPTheme.textColor
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    topMargin: 20
                    leftMargin: 10
                    rightMargin: 10
                }
            }
            Rectangle {
                id: sideStakeRect
                color: "transparent"
                border.color: MMPTheme.lightBorderColor
                radius: 4
                anchors {
                    left: parent.left
                    right: parent.right
                    top: addNodeInfoLabel.bottom
                    bottom: parent.bottom
                    margins: 10
                }
                ListView {
                    id: addNodeListView
                    currentIndex: 0
                    clip: true
                    anchors {
                        top: parent.top
                        right: parent.right
                        left: parent.left
                        bottom: addNodeFooter.top
                        margins: 1
                        bottomMargin: 0
                    }
                    model: ListModel {
                        id: addNodeListModel
                        ListElement{address: "www.google.com"}
                    }
                    ScrollIndicator.vertical: ScrollIndicator {
                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            right: parent.right
                            rightMargin: 1
                        }
                    }
                    delegate: Rectangle {
                        width: addNodeListView.width
                        height: 25
                        color: addNodeListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                        radius: 4
                        property Item addressTextField: addressTextField
                        TextField {
                            id: addressTextField
                            anchors.fill: parent
                            background: Item{}
                            text: model.address
                            placeholderText: qsTr("Enter Node Address")
                            onTextEdited: addNodeListView.model.set(index, {address: text})
                            onFocusChanged: {
                                if (focus) {
                                    addNodeListView.currentIndex = index
                                    forceActiveFocus()
                                }
                            }
                        }
                    }
                }
                TableFooter{
                    id: addNodeFooter
                    RowLayout {
                        id: buttonRow
                        height: parent.height
                        spacing: 0
                        Button {
                            id: addNodeButton
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_add_light.svg","/icons/buttons/ic_btn_add_dark.svg")
                            background: Item{}
                            onClicked: {
                                addNodeListModel.append({address: ""})
                                addNodeListView.currentIndex = addNodeListModel.count-1
                            }
                        }
                        Rectangle {
                            id: buttonSeparator
                            color: MMPTheme.lightBorderColor
                            width: 1
                            Layout.fillHeight: true
                        }
                        Button {
                            id: removeNodeButton
                            Layout.fillHeight: true
                            implicitWidth: 30
                            icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_remove_light.svg","/icons/buttons/ic_btn_remove_dark.svg")
                            background: Item{}
                            onClicked: addNodeListModel.remove(addNodeListView.currentIndex)
                            enabled: addNodeListModel.count > 0
                        }
                        Rectangle {
                            id: buttonSeparator2
                            color: MMPTheme.lightBorderColor
                            width: 1
                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }
    }
}
