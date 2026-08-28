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
    visible: true
    color: systemTheme.stageColor
    title: backend.fileName === "" ? "Omaviewer preview" : backend.fileName
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowStaysOnTopHint

    readonly property string visibleError: backend.errorMessage !== ""
        ? backend.errorMessage : viewport.loadError

    // Gtk.DirectionType values used by Sushi's SelectionEvent signal.
    readonly property int selectUp: 2
    readonly property int selectDown: 3
    readonly property int selectLeft: 4
    readonly property int selectRight: 5

    function mixColors(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1)
    }

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
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: backend.closePreview()
    }

    Shortcut {
        sequence: "Escape"
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
                color: previewWindow.mixColors(systemTheme.stageColor,
                                               systemTheme.pageColor, 0.55)

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: previewWindow.mixColors(systemTheme.stageColor,
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
                        text: "OPEN WITH OMAVIEWER"
                        focusPolicy: Qt.NoFocus
                        outlined: true
                        inkColor: systemTheme.inkColor
                        mutedColor: systemTheme.mutedColor
                        accentColor: systemTheme.accentColor
                        surfaceColor: systemTheme.pageColor
                        onClicked: backend.openFullViewer()
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
                    stageColor: systemTheme.stageColor
                    inkColor: systemTheme.inkColor
                    mutedColor: systemTheme.mutedColor
                    accentColor: systemTheme.accentColor
                    gridVisible: false
                    axesVisible: false
                    overlaysVisible: false
                    autoFrameOnResize: true
                    interactive: false
                }

                Rectangle {
                    anchors.fill: parent
                    visible: backend.busy || !viewport.frameReady
                    // Keep the covered View3D rendering so RuntimeLoader can
                    // finish calculating bounds needed by automatic framing.
                    color: Qt.rgba(systemTheme.stageColor.r,
                                   systemTheme.stageColor.g,
                                   systemTheme.stageColor.b, 0.99)

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        BusyIndicator {
                            anchors.horizontalCenter: parent.horizontalCenter
                            running: parent.parent.visible
                            implicitWidth: 30
                            implicitHeight: 30
                            Material.accent: systemTheme.accentColor
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.busy ? backend.statusText : "Loading model…"
                            color: systemTheme.mutedColor
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
                    color: previewWindow.mixColors(systemTheme.stageColor,
                                                   systemTheme.errorColor, 0.14)
                    border.width: 1
                    border.color: systemTheme.errorColor
                    radius: 2

                    Text {
                        id: errorText
                        anchors.fill: parent
                        anchors.margins: 12
                        text: previewWindow.visibleError
                        color: systemTheme.inkColor
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
