import QtQuick

Row {
    id: root

    required property string keyText
    required property string valueText
    property color textColor: "white"
    property int pixelSize: 12

    spacing: 6

    Text {
        text: root.keyText
        color: root.textColor
        font.family: "JetBrainsMono Nerd Font"
        font.pixelSize: root.pixelSize
        font.weight: Font.ExtraBold
        font.letterSpacing: 0.8
        renderType: Text.NativeRendering
    }

    Text {
        text: root.valueText
        color: root.textColor
        font.family: "JetBrainsMono Nerd Font"
        font.pixelSize: root.pixelSize
        font.weight: Font.Normal
        font.letterSpacing: 0.8
        renderType: Text.NativeRendering
    }
}
