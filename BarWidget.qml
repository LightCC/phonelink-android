import QtQuick
import Quickshell.Io
import qs.Commons as Commons
import qs.Ui as Ui

Ui.BarWidget {
    id: root
    moduleName: "io.github.lightcc.phonelink"
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight
    property var snapshot: ({available: false, code: "checking"})
    property string output: ""
    property bool pendingOpen: false
    readonly property string helperPath: decodeURIComponent(String(Qt.resolvedUrl("plugin/bridge.py")).slice(7))
    readonly property string stateText: {
        if (!snapshot.available) return strings.t(snapshot.code || "unavailable")
        if (snapshot.state.running) return strings.t("mirroring")
        if (snapshot.state.wirelessReady) return strings.t("wirelessReady")
        if (snapshot.state.usbReady) return strings.t("usbReady")
        return strings.t("waiting")
    }
    readonly property string iconFile: snapshot.available && snapshot.state.running ? "phonelink-active.svg"
        : snapshot.available && (snapshot.state.usbReady || snapshot.state.wirelessReady) ? "phonelink.svg" : "phonelink-offline.svg"

    Commons.Translations { id: strings; directory: Qt.resolvedUrl("plugin/i18n") }

    function refresh() {
        if (request.running) return
        root.output = ""
        if (!root.snapshot.available) root.snapshot = {available: false, code: "unavailable"}
        request.command = ["python3", root.helperPath, "status"]
        request.running = true
    }
    function open() {
        if (request.running) { root.pendingOpen = true; return }
        root.output = ""
        request.command = ["python3", root.helperPath, "open", strings.t("noticeTitle"), strings.t("setupHint")]
        request.running = true
    }
    // The widget opens the existing tiled app, not an additional shell popup.
    function toggle() { open() }
    function close() {}
    readonly property bool opened: false

    Process {
        id: request
        stdout: StdioCollector { waitForEnd: true; onStreamFinished: root.output = text }
        onExited: function(exitCode) {
            try {
                const parsed = JSON.parse(root.output)
                if (parsed && typeof parsed.available === "boolean" && typeof parsed.code === "string") root.snapshot = parsed
                else root.snapshot = {available: false, code: "unavailable"}
            } catch (e) { root.snapshot = {available: false, code: "unavailable"} }
            if (root.pendingOpen) { root.pendingOpen = false; Qt.callLater(root.open) }
        }
    }
    Timer { interval: 8000; running: true; repeat: true; triggeredOnStart: true; onTriggered: root.refresh() }
    Ui.WidgetButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        text: ""
        hasVisualContent: true
        labelVisible: false
        fixedWidth: root.barSize
        tooltipText: strings.t("title") + " — " + root.stateText + "\n" + strings.t("clickHint")
        onPressed: function(code) {
            if (code === Qt.LeftButton) root.open()
            else if (code === Qt.RightButton) root.refresh()
        }
        Image {
            anchors.centerIn: parent
            width: Commons.Style.space(18); height: width
            source: Qt.resolvedUrl("pkgbuild/" + root.iconFile)
            sourceSize.width: Math.ceil(width * 2); sourceSize.height: Math.ceil(height * 2)
        }
    }
}
