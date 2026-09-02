import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Controls.Material

Window {
    id: previewWindow

    width: 720
    height: 480
    minimumWidth: 720
    maximumWidth: 720
    minimumHeight: 480
    maximumHeight: 480
    visible: backend.previewVisible
    color: systemTheme.stageColor
    title: backend.fileName === "" ? "Oma3DViewer preview" : backend.fileName
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowStaysOnTopHint

    readonly property string visibleError: backend.errorMessage !== ""
        ? backend.errorMessage : viewport.loadError
    // Same fixed Blender-like stage as the main viewer so the model reads
    // identically on every theme.
    readonly property color stageColor: "#3d3d3d"
    readonly property color stageInkColor: "#e0e0e0"

    // Gtk.DirectionType values used by Sushi's SelectionEvent signal.
    readonly property int selectUp: 2
    readonly property int selectDown: 3
    readonly property int selectLeft: 4
    readonly property int selectRight: 5

    Shortcut {
        sequence: "Up"
        context: Qt.ApplicationShortcut
        onActivated: backend.navigatePreview(previewWindow.selectUp)
    }

    Shortcut {
        sequence: "Down"
        context: Qt.ApplicationShortcut
        onActivated: backend.navigatePreview(previewWindow.selectDown)
    }

    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        onActivated: backend.navigatePreview(previewWindow.selectLeft)
    }

    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        onActivated: backend.navigatePreview(previewWindow.selectRight)
    }

    Shortcut {
        sequences: ["Space", "Escape"]
        context: Qt.ApplicationShortcut
        onActivated: backend.closePreview()
    }

    Rectangle {
        anchors.fill: parent
        color: systemTheme.stageColor
        border.width: 1
        border.color: systemTheme.accentColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                color: systemTheme.mix(systemTheme.stageColor,
                                               systemTheme.pageColor, 0.55)

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: systemTheme.mix(systemTheme.stageColor,
                                                   systemTheme.inkColor, 0.16)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 10
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: backend.fileName === "" ? "Loading model…"
                                                       : backend.fileName
                        color: systemTheme.inkColor
                        elide: Text.ElideMiddle
                        horizontalAlignment: Text.AlignLeft
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        renderType: Text.NativeRendering
                    }

                    RailButton {
                        text: "OPEN WITH OMA3DVIEWER"
                        visible: backend.canOpenFullViewer
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.openFullViewer()
                    }

                    RailButton {
                        text: "OPEN WITH BLENDER"
                        visible: backend.canOpenInBlender
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.openInBlender()
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ViewerViewport {
                    id: viewport
                    anchors.fill: parent
                    source: backend.modelUrl
                    stageColor: previewWindow.stageColor
                    inkColor: previewWindow.stageInkColor
                    accentColor: systemTheme.accentColor
                    gridVisible: true
                    axesVisible: true
                    overlaysVisible: false
                    autoFrameOnResize: true
                    frameOnOrigin: true
                    interactive: true
                }

                Rectangle {
                    id: loadingCover
                    anchors.fill: parent
                    visible: backend.busy || !viewport.frameReady
                    // Keep the covered View3D rendering so RuntimeLoader can
                    // finish calculating bounds needed by automatic framing.
                    color: Qt.rgba(previewWindow.stageColor.r,
                                   previewWindow.stageColor.g,
                                   previewWindow.stageColor.b, 0.99)

                    // Most models frame within a few hundred milliseconds;
                    // only show the spinner once a load is taking a while,
                    // so quick loads do not flash it.
                    property bool indicatorShown: false
                    onVisibleChanged: indicatorShown = false

                    // Bound to visibility rather than started from
                    // onVisibleChanged: the cover is already visible when it
                    // is created, so that handler never fires for the first
                    // load and the spinner would never appear.
                    Timer {
                        id: indicatorTimer
                        interval: 400
                        running: loadingCover.visible
                        onTriggered: loadingCover.indicatorShown = true
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 12
                        visible: loadingCover.indicatorShown

                        BusyIndicator {
                            anchors.horizontalCenter: parent.horizontalCenter
                            running: loadingCover.indicatorShown
                            implicitWidth: 30
                            implicitHeight: 30
                            Material.accent: systemTheme.accentColor
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.busy ? backend.statusText : "Loading model…"
                            color: Qt.rgba(previewWindow.stageInkColor.r,
                                           previewWindow.stageInkColor.g,
                                           previewWindow.stageInkColor.b, 0.7)
                            font.family: "JetBrainsMono Nerd Font"
                            font.pixelSize: 10
                            font.letterSpacing: 0.7
                            renderType: Text.NativeRendering
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 14
                    height: errorText.implicitHeight + 24
                    visible: previewWindow.visibleError !== ""
                    color: systemTheme.mix(previewWindow.stageColor,
                                           systemTheme.errorColor, 0.14)
                    border.width: 1
                    border.color: systemTheme.errorColor
                    radius: 2

                    Text {
                        id: errorText
                        anchors.fill: parent
                        anchors.margins: 12
                        text: previewWindow.visibleError
                        color: previewWindow.stageInkColor
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        renderType: Text.NativeRendering
                    }
                }
            }
        }
    }
}
