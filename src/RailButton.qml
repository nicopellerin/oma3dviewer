import QtQuick
import QtQuick.Controls

Button {
    id: control

    property color inkColor: "white"
    property color mutedColor: "#888888"
    property color accentColor: "#7aa2f7"
    property color surfaceColor: "#222222"
    property bool outlined: false

    implicitHeight: 38
    implicitWidth: Math.max(58, contentItem.implicitWidth + 24)
    leftPadding: 8
    rightPadding: 8
    topPadding: 8
    bottomPadding: 8
    focusPolicy: Qt.StrongFocus

    contentItem: Text {
        text: control.text
        color: control.checked ? control.accentColor
                               : (control.enabled ? control.inkColor : control.mutedColor)
        font.family: "JetBrainsMono Nerd Font"
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.letterSpacing: 0.7
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        color: control.down
            ? Qt.rgba(control.accentColor.r, control.accentColor.g,
                      control.accentColor.b, 0.16)
            : (control.hovered
               ? Qt.rgba(control.inkColor.r, control.inkColor.g,
                         control.inkColor.b, 0.07)
               : "transparent")
        border.width: control.activeFocus || control.checked ? 1 : 0
        border.color: control.accentColor
    }
}
