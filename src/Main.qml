import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 650; height: 320
    minimumWidth: 300; minimumHeight: 280
    visible: !startHidden
    title: qsTr("Phonelink")
    color: "#14181e"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    property bool helpVisible: false
    property bool captureMode: false
    property int selectedTab: 0
    property bool navigationExpanded: false
    property var live: backend.state
    property var prefs: backend.preferences
    onSelectedTabChanged: if (selectedTab === 1) loadSettings()
    function loadSettings() {
        const p = backend.preferences
        serial.text = p.serial
        endpoint.text = p.endpoint
        transport.currentIndex = ["auto", "usb", "wireless"].indexOf(p.transport)
        recovery.checked = p.retry
        boot.checked = p.bootstrap
        audio.checked = p.audio
        autoOpen.checked = p.autoOpen
        showPanel.checked = p.showPanel
    }
    Component.onCompleted: loadSettings()
    onClosing: function(close) { close.accepted = false; hide() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: win.hide() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.hide() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.selectedTab === 0; onActivated: win.live.running || win.live.wanted ? backend.Stop() : backend.Start() }
    Shortcut { sequence: "Ctrl+R"; context: Qt.ApplicationShortcut; onActivated: backend.Retry() }
    Shortcut { sequence: "Ctrl+B"; context: Qt.ApplicationShortcut; onActivated: win.navigationExpanded = !win.navigationExpanded }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }

    component PlainLabel: Label {
        textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#dce3ed"
    }
    component CompactButton: Button {
        implicitHeight: 32
        topInset: 0; bottomInset: 0
        leftPadding: 11; rightPadding: 11; topPadding: 5; bottomPadding: 5
        font.pixelSize: 12
        contentItem: Text {
            text: parent.text; font: parent.font; textFormat: Text.PlainText
            color: !parent.enabled ? "#788391" : parent.highlighted ? theme.accentForeground : "#dce3ed"
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            implicitHeight: 32; radius: theme.radius
            color: parent.highlighted && parent.enabled ? theme.accent : parent.down ? "#354152" : parent.hovered ? "#2c3643" : "#20262e"
            opacity: parent.enabled ? 1 : 0.6
            border.color: parent.visualFocus ? theme.accent : "#354050"
        }
    }
    component NavigationButton: Button {
        id: navigationButton
        property int page: -1
        Layout.fillWidth: true; implicitHeight: 36
        leftPadding: 8; rightPadding: 8; topInset: 0; bottomInset: 0
        contentItem: Item {
            implicitWidth: win.navigationExpanded ? 130 : 20; implicitHeight: 20
            Image { id: navigationIcon; objectName: "navigationIcon"; source: navigationButton.icon.source; sourceSize.width: 20; sourceSize.height: 20; width: 20; height: 20; anchors.verticalCenter: parent.verticalCenter; fillMode: Image.PreserveAspectFit }
            PlainLabel { anchors.left: navigationIcon.right; anchors.leftMargin: 8; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; visible: win.navigationExpanded; text: navigationButton.text; font.pixelSize: 12; wrapMode: Text.NoWrap; elide: Text.ElideRight }
        }
        display: win.navigationExpanded ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
        icon.width: 20; icon.height: 20; icon.color: "transparent"
        font.pixelSize: 12
        onClicked: if (page >= 0) win.selectedTab = page
        ToolTip.visible: hovered && !win.navigationExpanded && !win.captureMode
        ToolTip.text: text
        Accessible.name: text
        background: Rectangle {
            radius: theme.radius
            color: win.selectedTab === parent.page ? "#303e50" : parent.hovered ? "#252e3b" : "transparent"
            Rectangle { width: 2; height: 16; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; color: theme.accent; visible: win.selectedTab === parent.parent.page }
        }
    }
    component StatusTile: Rectangle {
        property string heading
        property string value
        property string detail
        property bool ready: false
        Layout.fillWidth: true; Layout.minimumWidth: 0
        implicitWidth: 142; implicitHeight: 108
        color: "#20262e"; border.color: ready ? "#36534b" : "#333c49"; radius: theme.radius
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 10; spacing: 4
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                Rectangle { width: 6; height: 6; radius: 3; color: ready ? "#6ed6a7" : "#7c8797" }
                PlainLabel { text: heading; font.pixelSize: 11; color: "#a0adbf"; Layout.fillWidth: true; wrapMode: Text.NoWrap }
            }
            PlainLabel { text: value; font.pixelSize: 15; font.weight: Font.DemiBold; Layout.fillWidth: true; maximumLineCount: 2; elide: Text.ElideRight }
            Item { Layout.fillHeight: true }
            PlainLabel { text: detail; font.pixelSize: 11; color: "#9caabd"; Layout.fillWidth: true; maximumLineCount: 2; elide: Text.ElideRight }
        }
        HoverHandler { id: tileHover }
        ToolTip.visible: tileHover.hovered && !win.captureMode
        ToolTip.text: heading + ": " + value + "\n" + detail
    }
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        RowLayout {
            anchors.fill: parent; spacing: 0
            Rectangle {
                Layout.fillHeight: true; Layout.preferredWidth: win.navigationExpanded ? 164 : 44
                color: "#1a2028"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 4; spacing: 4
                    NavigationButton {
                        objectName: "navigationToggle"; page: -1
                        text: win.navigationExpanded ? qsTr("Collapse sidebar") : qsTr("Expand sidebar")
                        icon.source: "qrc:/nav-menu.svg"
                        onClicked: win.navigationExpanded = !win.navigationExpanded
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#303844" }
                    NavigationButton { objectName: "connectionNavigation"; page: 0; text: qsTr("Connection"); icon.source: "qrc:/phonelink.svg" }
                    NavigationButton { objectName: "settingsNavigation"; page: 1; text: qsTr("Settings"); icon.source: "qrc:/nav-settings.svg" }
                    NavigationButton { objectName: "historyNavigation"; page: 2; text: qsTr("History"); icon.source: "qrc:/nav-history.svg" }
                    Item { Layout.fillHeight: true }
                    NavigationButton { page: -2; text: qsTr("Keyboard shortcuts"); icon.source: "qrc:/nav-help.svg"; onClicked: win.helpVisible = true }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.margins: 12; spacing: 10
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    PlainLabel { text: qsTr("Phonelink"); font.pixelSize: 18; font.weight: Font.DemiBold }
                    PlainLabel { text: [qsTr("Connection"), qsTr("Settings"), qsTr("History")][win.selectedTab]; font.pixelSize: 12; color: "#9caabd"; Layout.fillWidth: true; wrapMode: Text.NoWrap; elide: Text.ElideRight }
                    BusyIndicator { running: backend.busy; visible: running; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                }
                Flow {
                    id: actions; objectName: "connectionActions"
                    Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height
                    visible: win.selectedTab === 0; spacing: 6
                    CompactButton { objectName: "screenButton"; text: win.live.running || win.live.wanted ? qsTr("Stop Screen") : qsTr("Open Screen"); highlighted: true; onClicked: win.live.running || win.live.wanted ? backend.Stop() : backend.Start() }
                    CompactButton { text: qsTr("Retry"); enabled: !backend.busy; onClicked: backend.Retry() }
                    CompactButton {
                        text: qsTr("Restore Wi-Fi"); enabled: win.live.usbReady && !backend.busy && !win.live.running
                        onClicked: backend.RestoreWireless()
                        ToolTip.visible: hovered && !win.captureMode; ToolTip.text: qsTr("Restore wireless debugging through an authorized USB connection.")
                    }
                }
                StackLayout {
                    currentIndex: win.selectedTab; Layout.fillWidth: true; Layout.fillHeight: true
                    ScrollView {
                        id: connectionScroll; objectName: "connectionScroll"
                        contentWidth: availableWidth; clip: true
                        ColumnLayout {
                            width: connectionScroll.availableWidth; spacing: 10
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                PlainLabel { text: win.live.model || qsTr("Waiting for phone"); font.pixelSize: 13; font.weight: Font.DemiBold; wrapMode: Text.NoWrap; elide: Text.ElideRight; Layout.fillWidth: true }
                                PlainLabel { text: win.live.android ? "Android " + win.live.android : ""; font.pixelSize: 11; color: "#9caabd" }
                            }
                            GridLayout {
                                id: statusGrid; objectName: "statusGrid"
                                Layout.fillWidth: true
                                columns: Math.max(1, Math.min(3, Math.floor((width + 8) / 150)))
                                columnSpacing: 8; rowSpacing: 8; uniformCellWidths: true
                                StatusTile {
                                    heading: qsTr("USB"); ready: win.live.usbReady
                                    value: win.live.usbReady ? qsTr("Connected") : win.live.usb === "unauthorized" ? qsTr("Approve on phone") : win.live.usbPresent ? qsTr("Debugging off") : qsTr("No cable")
                                    detail: win.live.usbId || (win.live.usbPresent ? qsTr("Enable USB debugging") : qsTr("USB data cable"))
                                }
                                StatusTile {
                                    heading: qsTr("WIRELESS"); ready: win.live.wirelessReady
                                    value: win.live.wirelessReady ? qsTr("Connected") : win.live.wireless === "connecting" ? qsTr("Connecting…") : win.live.wireless === "identity not verified" ? qsTr("Unverified phone") : qsTr("Not connected")
                                    detail: win.live.endpoint || qsTr("Not discovered")
                                }
                                StatusTile {
                                    heading: qsTr("SCREEN"); ready: win.live.running
                                    value: win.live.running ? win.live.mirror === "Mirroring over USB" ? qsTr("USB") : qsTr("Wi-Fi") : win.live.wanted ? qsTr("Waiting / retrying") : qsTr("Stopped")
                                    detail: win.live.running ? win.prefs.audio ? qsTr("Audio on this PC") : qsTr("Audio on phone") : qsTr("Open Screen to start")
                                }
                            }
                            PlainLabel { text: win.live.error; color: "#f3ba8b"; visible: text.length > 0; Layout.fillWidth: true; font.pixelSize: 12 }
                            PlainLabel { text: win.live.serial || qsTr("Set your phone serial in Settings"); color: "#8493a6"; font.family: "monospace"; font.pixelSize: 11; Layout.fillWidth: true }
                        }
                    }
                ScrollView {
                    contentWidth: availableWidth; clip: true
                    ColumnLayout {
                        width: parent.width; spacing: 8
                        PlainLabel { text: qsTr("Phone and connection"); font.pixelSize: 17; font.weight: Font.DemiBold }
                        PlainLabel { text: qsTr("Find the phone serial with adb devices. Saved endpoints are verified against that serial before mirroring."); color: "#9caabd"; Layout.fillWidth: true }
                        TextField { id: serial; objectName: "serialField"; text: ""; placeholderText: qsTr("Phone serial"); Layout.fillWidth: true; selectByMouse: true }
                        TextField { id: endpoint; text: ""; placeholderText: qsTr("Wireless IP:port (optional)"); Layout.fillWidth: true; selectByMouse: true }
                        PlainLabel { text: qsTr("Preferred connection") }
                        ComboBox { id: transport; model: [qsTr("Automatic · prefer wireless"), qsTr("USB only"), qsTr("Wireless only")]; currentIndex: 0; Layout.fillWidth: true }
                        CheckBox { id: recovery; text: qsTr("Recover scrcpy after a crash (up to 5 attempts)"); checked: false; Layout.fillWidth: true; contentItem: Text { text: parent.text; font: parent.font; color: "#dce3ed"; wrapMode: Text.Wrap; textFormat: Text.PlainText; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter } }
                        CheckBox { id: boot; text: qsTr("Restore wireless through USB when opening the screen"); checked: false; Layout.fillWidth: true; contentItem: Text { text: parent.text; font: parent.font; color: "#dce3ed"; wrapMode: Text.Wrap; textFormat: Text.PlainText; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter } }
                        CheckBox { id: audio; text: qsTr("Forward phone audio to this PC"); checked: false; Layout.fillWidth: true; contentItem: Text { text: parent.text; font: parent.font; color: "#dce3ed"; wrapMode: Text.Wrap; textFormat: Text.PlainText; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter } }
                        CheckBox { id: autoOpen; text: qsTr("Start phone mirroring when Phonelink starts"); checked: false; Layout.fillWidth: true; contentItem: Text { text: parent.text; font: parent.font; color: "#dce3ed"; wrapMode: Text.Wrap; textFormat: Text.PlainText; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter } }
                        CheckBox { id: showPanel; text: qsTr("Show status panel when Phonelink starts"); checked: false; Layout.fillWidth: true; contentItem: Text { text: parent.text; font: parent.font; color: "#dce3ed"; wrapMode: Text.Wrap; textFormat: Text.PlainText; leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter } }
                        CompactButton { text: qsTr("Save Settings"); enabled: !backend.busy && !win.live.running; highlighted: true; onClicked: backend.savePreferences(serial.text.trim(),endpoint.text.trim(),["auto","usb","wireless"][transport.currentIndex],recovery.checked,audio.checked,boot.checked,autoOpen.checked,showPanel.checked) }
                        PlainLabel { text: win.live.error; color: "#f3ba8b"; Layout.fillWidth: true }
                        PlainLabel { text: qsTr("The tray starts at login. These options separately control the status panel and phone mirroring. Stop mirroring before changing settings."); color: "#9caabd"; Layout.fillWidth: true }
                    }
                }
                ColumnLayout {
                    spacing: 8
                    PlainLabel { text: qsTr("Connection history"); font.pixelSize: 17; font.weight: Font.DemiBold }
                    PlainLabel { text: qsTr("Dated event logs record connections, errors and retries. phones.json retains phone details and every verified endpoint with first and last seen times."); Layout.fillWidth: true; color: "#9caabd" }
                    CompactButton { text: qsTr("Open History Folder"); onClicked: backend.openHistory() }
                    PlainLabel { text: backend.historyDirectory; font.family: "monospace"; font.pixelSize: 11; Layout.fillWidth: true; color: "#9caabd" }
                    ScrollView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        TextArea { text: backend.recentLog || qsTr("No events in this service session yet."); readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText; font.family: "monospace"; font.pixelSize: 11; background: Rectangle { color: "#20262e"; radius: theme.radius } }
                    }
                }
                }
            }
        }
    }
    Popup {
        anchors.centerIn: parent; width: Math.min(win.width - 32, 420); modal: true; visible: win.helpVisible
        onClosed: win.helpVisible = false
        ColumnLayout {
            width: parent.width; spacing: 10
            PlainLabel { text: qsTr("Keyboard shortcuts"); font.pixelSize: 18 }
            PlainLabel { text: qsTr("Space — open or stop the screen\nCtrl+R — retry connection\nCtrl+B — toggle sidebar\nQ / Escape — hide this panel\n? — keyboard shortcuts"); Layout.fillWidth: true }
            CompactButton { text: qsTr("Close"); onClicked: win.helpVisible = false }
        }
    }
}
