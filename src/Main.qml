import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win

    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: backend.displayName === "" ? "oma3dviewer"
                                      : backend.displayName + " — oma3dviewer"
    color: systemTheme.pageColor

    Material.theme: systemTheme.darkMode ? Material.Dark : Material.Light
    Material.accent: systemTheme.accentColor

    readonly property color stageColor: "#3d3d3d"
    readonly property color stageInkColor: "#e0e0e0"
    property bool gridVisible: true
    property bool axesVisible: true
    property bool dropReady: false
    property bool errorDismissed: false
    property rect normalGeometry: Qt.rect(x, y, width, height)
    property bool wasMaximized: false
    readonly property bool lightingEnabled: litModeButton.checked
    readonly property string visibleError: backend.errorMessage !== ""
        ? backend.errorMessage
        : (!errorDismissed ? viewport.loadError : "")

    function trackNormalGeometry() {
        if (visibility === Window.Windowed)
            normalGeometry = Qt.rect(x, y, width, height)
    }

    FileDialog {
        id: openDialog
        title: "Open a 3D model"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "3D models (" + backend.supportedExtensions.map(
                                ext => "*." + ext).join(" ") + ")",
            "glTF models (*.glb *.gltf)",
            "Wavefront models (*.obj)",
            "FBX models (*.fbx)"
        ]
        onAccepted: backend.openFile(selectedFile)
    }

    ButtonGroup {
        id: lightingModeGroup
        exclusive: true
    }

    Shortcut {
        sequence: "Ctrl+O"
        onActivated: openDialog.open()
    }

    Connections {
        target: backend
        function onModelUrlChanged() { win.errorDismissed = false }
        function onErrorMessageChanged() {
            if (backend.errorMessage !== "")
                win.errorDismissed = false
        }
    }
    Shortcut {
        sequences: ["F", "R"]
        onActivated: viewport.frameModel()
    }
    Shortcut {
        sequence: "G"
        onActivated: win.gridVisible = !win.gridVisible
    }
    Shortcut {
        sequence: "A"
        onActivated: win.axesVisible = !win.axesVisible
    }
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: win.close()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: systemTheme.pageColor

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: systemTheme.mix(systemTheme.pageColor, systemTheme.inkColor, 0.12)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 12
                spacing: 12

                Text {
                    text: "OMA3DVIEWER"
                    color: systemTheme.inkColor
                    font.family: "JetBrainsMono Nerd Font"
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    font.letterSpacing: 1.6
                    renderType: Text.NativeRendering
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 18
                    color: systemTheme.mix(systemTheme.pageColor, systemTheme.inkColor, 0.18)
                }

                Text {
                    Layout.fillWidth: true
                    text: backend.displayName === "" ? "NO MODEL"
                                                      : backend.displayName
                    color: backend.displayName === "" ? systemTheme.mutedColor
                                                       : systemTheme.inkColor
                    elide: Text.ElideMiddle
                    font.pixelSize: 13
                    renderType: Text.NativeRendering
                }

                RailButton {
                    id: litModeButton
                    text: "LIT"
                    checkable: true
                    checked: true
                    ButtonGroup.group: lightingModeGroup
                }

                RailButton {
                    text: "UNLIT"
                    checkable: true
                    ButtonGroup.group: lightingModeGroup
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 18
                    color: systemTheme.mix(systemTheme.pageColor,
                                         systemTheme.inkColor, 0.18)
                }

                RailButton {
                    text: "GRID"
                    checkable: true
                    checked: win.gridVisible
                    onClicked: win.gridVisible = checked
                }

                RailButton {
                    text: "AXES"
                    checkable: true
                    checked: win.axesVisible
                    onClicked: win.axesVisible = checked
                }

                RailButton {
                    text: "FRAME"
                    enabled: viewport.loaded
                    onClicked: viewport.frameModel()
                }

                RailButton {
                    text: "OPEN"
                    onClicked: openDialog.open()
                }
            }
        }

        Item {
            id: stage
            Layout.fillWidth: true
            Layout.fillHeight: true

            ViewerViewport {
                id: viewport
                anchors.fill: parent
                source: backend.modelUrl
                // Fixed Blender-like neutral gray so the stage, grid and
                // hints read the same on every theme, light or dark.
                stageColor: win.stageColor
                inkColor: win.stageInkColor
                accentColor: systemTheme.accentColor
                gridVisible: win.gridVisible
                axesVisible: win.axesVisible
                lightingEnabled: win.lightingEnabled
                frameOnOrigin: true
            }

            DropArea {
                anchors.fill: parent
                onEntered: function(drag) {
                    win.dropReady = drag.hasUrls && drag.urls.length > 0
                                    && backend.acceptsUrl(drag.urls[0])
                    drag.accepted = win.dropReady
                }
                onExited: win.dropReady = false
                onDropped: function(drop) {
                    win.dropReady = false
                    if (drop.hasUrls && drop.urls.length > 0
                            && backend.acceptsUrl(drop.urls[0])) {
                        backend.openFile(drop.urls[0])
                        drop.acceptProposedAction()
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: win.dropReady
                color: Qt.rgba(systemTheme.accentColor.r, systemTheme.accentColor.g,
                               systemTheme.accentColor.b, 0.11)
                border.width: 1
                border.color: systemTheme.accentColor

                Text {
                    anchors.centerIn: parent
                    text: "DROP TO OPEN"
                    color: systemTheme.accentColor
                    font.family: "JetBrainsMono Nerd Font"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    font.letterSpacing: 1.5
                    renderType: Text.NativeRendering
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 14
                visible: backend.modelUrl.toString() === "" && !backend.busy

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Drop a 3D model"
                    color: systemTheme.inkColor
                    font.pixelSize: 22
                    font.weight: Font.Medium
                    renderType: Text.NativeRendering
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: backend.supportedExtensions.join("  ·  ").toUpperCase()
                    color: systemTheme.mutedColor
                    font.family: "JetBrainsMono Nerd Font"
                    font.pixelSize: 10
                    font.letterSpacing: 1
                    renderType: Text.NativeRendering
                }

                RailButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "OPEN MODEL"
                    onClicked: openDialog.open()
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: backend.busy

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: backend.busy
                    implicitWidth: 36
                    implicitHeight: 36
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: backend.statusText
                    color: systemTheme.inkColor
                    font.family: "JetBrainsMono Nerd Font"
                    font.pixelSize: 11
                    font.letterSpacing: 0.8
                    renderType: Text.NativeRendering
                }
            }

            Row {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 18
                anchors.bottomMargin: stage.width < 900 ? 36 : 14
                visible: viewport.loaded && backend.modelStatsAvailable
                         && win.visibleError === ""
                spacing: 18

                property color statsColor: Qt.rgba(win.stageInkColor.r,
                                                    win.stageInkColor.g,
                                                    win.stageInkColor.b, 0.48)
                readonly property var statsLocale: Qt.locale("en_US")

                function formatCount(value) {
                    return Number(value).toLocaleString(statsLocale, "f", 0)
                }

                BottomLabelPair {
                    keyText: "MESHES"
                    valueText: parent.formatCount(backend.meshCount)
                    textColor: parent.statsColor
                }

                BottomLabelPair {
                    keyText: "VERTICES"
                    valueText: parent.formatCount(backend.vertexCount)
                    textColor: parent.statsColor
                }

                BottomLabelPair {
                    keyText: "TRIANGLES"
                    valueText: parent.formatCount(backend.triangleCount)
                    textColor: parent.statsColor
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 14
                height: errorRow.implicitHeight + 18
                visible: win.visibleError !== ""
                color: systemTheme.mix(win.stageColor, systemTheme.errorColor, 0.12)
                border.width: 1
                border.color: systemTheme.errorColor
                radius: 2

                RowLayout {
                    id: errorRow
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 10

                    Text {
                        text: "LOAD ERROR"
                        color: systemTheme.errorColor
                        font.family: "JetBrainsMono Nerd Font"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 1
                        renderType: Text.NativeRendering
                    }

                    Text {
                        Layout.fillWidth: true
                        text: win.visibleError
                        color: systemTheme.inkColor
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        renderType: Text.NativeRendering
                    }

                    RailButton {
                        text: "CLOSE"
                        accentColor: systemTheme.errorColor
                        onClicked: {
                            backend.clearError()
                            win.errorDismissed = true
                        }
                    }
                }
            }
        }
    }

    onXChanged: trackNormalGeometry()
    onYChanged: trackNormalGeometry()
    onWidthChanged: trackNormalGeometry()
    onHeightChanged: trackNormalGeometry()
    onVisibilityChanged: {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            wasMaximized = true
        else if (visibility === Window.Windowed)
            wasMaximized = false
    }

    Component.onCompleted: {
        var geometry = backend.windowGeometry()
        if (geometry.valid) {
            x = geometry.x
            y = geometry.y
            width = geometry.width
            height = geometry.height
            if (geometry.maximized)
                showMaximized()
        }
    }

    onClosing: backend.saveWindowGeometry(
        normalGeometry.x, normalGeometry.y,
        normalGeometry.width, normalGeometry.height, wasMaximized)
}
