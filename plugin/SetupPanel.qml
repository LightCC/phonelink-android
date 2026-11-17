pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import qs.Commons as Commons
import qs.Ui as Ui

Ui.Panel {
    id: root
    moduleName: "io.github.lightcc.phonelink"
    ipcTarget: "phonelinkSetup"
    property var anchorItem: null
    property var hostWidget: null
    property var plan: ({})
    property var job: ({phase: "idle", log: ""})
    property string errorCode: ""
    property string output: ""
    property bool enableLogin: true
    property bool details: false
    property bool locationsVisible: false
    property string pending: ""
    readonly property var uiFont: Commons.Style.font
    readonly property bool jobActive: ["idle", "done", "failed", "cancelled", "interrupted"].indexOf(job.phase) === -1
    readonly property string helperPath: decodeURIComponent(String(Qt.resolvedUrl("setup.py")).slice(7))
    readonly property bool busy: request.running
    Commons.Translations { id: strings; directory: Qt.resolvedUrl("i18n") }
    signal completed()

    function open() { controller.show(); invoke("inspect") }
    function invoke(action) {
        if (request.running) { if (action === "cancel" || (action === "inspect" && root.pending !== "cancel")) root.pending = action; return }
        root.output = ""
        if (action !== "status") root.errorCode = ""
        request.action = action
        request.command = ["python3", "-B", root.helperPath, action]
        if (action === "begin") request.command = request.command.concat(["--confirmed", "--login", root.enableLogin ? "yes" : "no"])
        request.running = true
    }
    function copyInstructions() {
        const text = strings.t("agentIntro") + "\n" + "https://github.com/LightCC/phonelink-android\n" +
            String(root.plan.instructions || strings.t("dependencyHelp")) + "\n" +
            strings.t("consent") + (root.job.directory ? "\nSetup job and retained backups: " + root.job.directory : "")
        Quickshell.execDetached(["wl-copy", "--", text])
    }
    Process {
        id: request
        property string action: ""
        stdout: StdioCollector { waitForEnd: true; onStreamFinished: root.output = text }
        onExited: function(code) {
            const wasActive = root.jobActive
            try {
                const result = JSON.parse(root.output)
                if (result.plan) {
                    root.plan = result.plan
                    if (request.action === "inspect" && !wasActive) root.enableLogin = result.plan.enabled === true
                }
                if (result.job) root.job = result.job
                if (result.error) root.errorCode = String(result.error)
            } catch (e) { root.errorCode = "helper" }
            if (wasActive && !root.jobActive) {
                root.completed()
                root.pending = "inspect"
            }
            if (root.pending !== "") {
                const action = root.pending; root.pending = ""; Qt.callLater(function() { root.invoke(action) })
            }
        }
    }
    Timer {
        interval: 1000; repeat: true; running: root.opened || root.jobActive
        onTriggered: if (!request.running) root.invoke("status")
    }
    component PlainText: Text {
        color: Commons.Color.foreground
        font.family: root.uiFont.family
        font.pixelSize: root.uiFont.body
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Ui.KeyboardPanel {
        id: popup
        anchorItem: root.anchorItem
        owner: root.hostWidget || root
        bar: root.bar
        open: root.opened
        focusTarget: keys
        contentWidth: fittedContentWidth(Commons.Style.space(420))
        contentHeight: fittedContentHeight(Commons.Style.space(480))
        FocusScope {
            id: keys
            anchors.fill: parent
            focus: true
            Keys.onEscapePressed: root.close()
            ScrollView {
                id: scroll
                anchors.fill: parent
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: scroll.availableWidth
                    spacing: Commons.Style.space(10)
                    PlainText { text: strings.t("serviceSetup"); font.pixelSize: root.uiFont.subtitle; font.bold: true }
                    PlainText { text: strings.t("installExplanation") }
                    PlainText {
                        text: strings.t("detected") + ": " + (root.plan.mode ? strings.t("mode_" + root.plan.mode) : "…") +
                            "\n" + strings.t("version") + ": " + (root.plan.version || "—") + " → " + (root.plan.targetVersion || "…")
                    }
                    PlainText {
                        visible: !!root.plan.blocked
                        color: Commons.Color.urgent
                        text: strings.t("blocked_" + (root.plan.blocked || "")) +
                            (root.plan.missing && root.plan.missing.length ? "\n" + root.plan.missing.join(", ") : "")
                    }
                    PlainText { visible: root.errorCode !== ""; color: Commons.Color.urgent; text: strings.t("setupError_" + root.errorCode) }
                    PlainText { visible: root.plan.blocked === "dependencies"; text: root.plan.dependencyCommand || ""; font.family: "monospace" }
                    Ui.Button {
                        text: strings.t("startAtLogin")
                        iconText: root.enableLogin ? "✓" : "○"
                        selected: root.enableLogin
                        focusable: true
                        enabled: !root.jobActive
                        Accessible.role: Accessible.CheckBox
                        Accessible.name: text
                        Accessible.checked: root.enableLogin
                        onClicked: root.enableLogin = !root.enableLogin
                    }
                    Ui.Button {
                        text: root.locationsVisible ? strings.t("hideLocations") : strings.t("showLocations")
                        focusable: true
                        onClicked: root.locationsVisible = !root.locationsVisible
                    }
                    PlainText { visible: root.locationsVisible; text: root.plan.locations ? root.plan.locations.join("\n") : ""; font.family: "monospace" }
                    PlainText {
                        text: strings.t("phase_" + (root.job.phase || "idle"))
                        font.bold: true
                        color: root.job.phase === "failed" || root.job.phase === "interrupted" ? Commons.Color.urgent : Commons.Color.foreground
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Commons.Style.space(6)
                        Ui.Button {
                            text: root.plan.action === "enable" ? strings.t("enableService") : strings.t("confirmInstall")
                            enabled: root.plan.canInstall === true && !root.jobActive && !root.busy
                            opacity: enabled ? 1 : 0.45
                            selected: true
                            focusable: true
                            onClicked: root.invoke("begin")
                        }
                        Ui.Button { text: strings.t("refreshSetup"); enabled: !root.jobActive && !root.busy; focusable: true; onClicked: root.invoke("inspect") }
                        Ui.Button { text: strings.t("cancelInstall"); visible: root.jobActive; focusable: true; onClicked: root.invoke("cancel") }
                    }
                    Ui.Button { text: strings.t("copyForAgent"); focusable: true; onClicked: root.copyInstructions() }
                    Ui.Button { text: root.details ? strings.t("hideDetails") : strings.t("showDetails"); visible: root.job.log !== ""; focusable: true; onClicked: root.details = !root.details }
                    ScrollView {
                        visible: root.details
                        Layout.fillWidth: true
                        Layout.preferredHeight: Commons.Style.space(140)
                        TextArea {
                            text: root.job.log || ""
                            textFormat: TextEdit.PlainText
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            color: Commons.Color.foreground
                            font.family: "monospace"
                            font.pixelSize: root.uiFont.bodySmall
                            background: Rectangle { color: "transparent" }
                        }
                    }
                    PlainText { text: strings.t("setupFooter") }
                    Ui.Button { text: strings.t("closeSetup"); focusable: true; onClicked: root.close() }
                }
            }
        }
    }
}
