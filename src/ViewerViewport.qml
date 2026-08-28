import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils
import QtQuick3D.Helpers

Item {
    id: root

    property url source
    property color stageColor: "#13141c"
    property color inkColor: "#a9b1d6"
    property color mutedColor: "#565f89"
    property color accentColor: "#7aa2f7"
    property bool gridVisible: true
    property bool axesVisible: true
    property bool overlaysVisible: true
    property bool autoFrameOnResize: false
    property bool interactive: true

    readonly property bool loaded: modelLoader.status === RuntimeLoader.Success
    readonly property string loadError: modelLoader.status === RuntimeLoader.Error
                                                ? modelLoader.errorString : ""
    property vector3d boundsCenter: Qt.vector3d(0, 0, 0)
    property real boundsDiameter: 100
    property bool frameReady: false
    readonly property real gridInterval: Math.max(0.001,
        Math.pow(10, Math.round(Math.log(Math.max(boundsDiameter, 0.001)) / Math.LN10) - 1))

    function frameModel() {
        if (modelLoader.status !== RuntimeLoader.Success)
            return false

        return frameBounds(modelLoader.bounds.minimum,
                           modelLoader.bounds.maximum)
    }

    function frameBounds(minimum, maximum) {
        var size = Qt.vector3d(
            maximum.x - minimum.x,
            maximum.y - minimum.y,
            maximum.z - minimum.z)
        var diameter = Math.max(size.x, size.y, size.z)
        if (!isFinite(diameter) || diameter <= 0) {
            frameReady = false
            return false
        }

        boundsCenter = Qt.vector3d(
            (maximum.x + minimum.x) / 2,
            (maximum.y + minimum.y) / 2,
            (maximum.z + minimum.z) / 2)
        boundsDiameter = diameter

        var aspect = Math.max(0.01, view.width / Math.max(1, view.height))
        var verticalHalfFov = camera.fieldOfView * Math.PI / 360
        var horizontalHalfFov = Math.atan(Math.tan(verticalHalfFov) * aspect)
        var tanVertical = Math.tan(Math.max(0.01, verticalHalfFov))
        var tanHorizontal = Math.tan(Math.max(0.01, horizontalHalfFov))
        var pitch = 12 * Math.PI / 180
        var yaw = -30 * Math.PI / 180
        var cosPitch = Math.cos(pitch)
        var sinPitch = Math.sin(pitch)
        var cosYaw = Math.cos(yaw)
        var sinYaw = Math.sin(yaw)
        var halfX = size.x / 2
        var halfY = size.y / 2
        var halfZ = size.z / 2
        var distance = 0
        var nearestDepth = -Infinity
        var farthestDepth = Infinity
        var fill = 0.86

        for (var xi = -1; xi <= 1; xi += 2) {
            for (var yi = -1; yi <= 1; yi += 2) {
                for (var zi = -1; zi <= 1; zi += 2) {
                    var x = xi * halfX
                    var y = yi * halfY
                    var z = zi * halfZ
                    var viewX = cosYaw * x + sinYaw * z
                    var yawZ = -sinYaw * x + cosYaw * z
                    var viewY = cosPitch * y - sinPitch * yawZ
                    var viewZ = sinPitch * y + cosPitch * yawZ
                    distance = Math.max(
                        distance,
                        viewZ + Math.abs(viewX) / (tanHorizontal * fill),
                        viewZ + Math.abs(viewY) / (tanVertical * fill))
                    nearestDepth = Math.max(nearestDepth, viewZ)
                    farthestDepth = Math.min(farthestDepth, viewZ)
                }
            }
        }
        distance = Math.max(distance, diameter * 0.05)

        orbitOrigin.position = boundsCenter
        orbitOrigin.eulerRotation = Qt.vector3d(-12, 30, 0)
        camera.position = Qt.vector3d(0, 0, Math.max(0.001, distance))
        camera.eulerRotation = Qt.vector3d(0, 0, 0)
        camera.clipNear = Math.max(0.0001,
                                   (distance - nearestDepth) * 0.25)
        camera.clipFar = Math.max(camera.clipNear + 1,
                                  distance - farthestDepth + diameter * 2)
        frameReady = true
        return true
    }

    function scheduleFrame() {
        frameTimer.restart()
    }

    function grabPreview(callback, targetSize) {
        return view.grabToImage(callback, targetSize)
    }

    View3D {
        id: view
        anchors.fill: parent
        camera: camera

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: root.stageColor
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High

            InfiniteGrid {
                visible: root.gridVisible
                gridAxes: root.axesVisible
                gridInterval: root.gridInterval
            }
        }

        Node {
            id: orbitOrigin

            PerspectiveCamera {
                id: camera
                position: Qt.vector3d(0, 0, 200)
                fieldOfView: 42
                clipNear: 0.1
                clipFar: 10000
            }
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-38, -32, 0)
            brightness: 1.15
            ambientColor: Qt.rgba(0.24, 0.24, 0.28, 1)
            castsShadow: true
            shadowFactor: 35
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(24, 145, 0)
            brightness: 0.45
            color: root.accentColor
            castsShadow: false
        }

        RuntimeLoader {
            id: modelLoader
            source: root.source
            onSourceChanged: root.frameReady = false
            onBoundsChanged: root.scheduleFrame()
            onStatusChanged: {
                if (status === RuntimeLoader.Success)
                    root.scheduleFrame()
            }
        }
    }

    Timer {
        id: frameTimer
        interval: 0
        onTriggered: root.frameModel()
    }

    onWidthChanged: {
        if (autoFrameOnResize && loaded)
            scheduleFrame()
    }

    onHeightChanged: {
        if (autoFrameOnResize && loaded)
            scheduleFrame()
    }

    OrbitCameraController {
        enabled: root.interactive
        anchors.fill: parent
        origin: orbitOrigin
        camera: camera
        panEnabled: true
        automaticClipping: true
    }

    // Focus brackets mark the interactive render area without adding a card.
    Canvas {
        id: focusFrame
        anchors.fill: parent
        visible: root.overlaysVisible
        property color strokeColor: root.accentColor
        onStrokeColorChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections {
            target: root
            function onLoadedChanged() { focusFrame.requestPaint() }
        }
        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = strokeColor
            ctx.globalAlpha = root.loaded ? 0.78 : 0.34
            ctx.lineWidth = 1
            ctx.beginPath()
            var m = 14
            var l = 25

            ctx.moveTo(m, m + l); ctx.lineTo(m, m); ctx.lineTo(m + l, m)
            ctx.moveTo(width - m - l, m); ctx.lineTo(width - m, m); ctx.lineTo(width - m, m + l)
            ctx.moveTo(m, height - m - l); ctx.lineTo(m, height - m); ctx.lineTo(m + l, height - m)
            ctx.moveTo(width - m - l, height - m); ctx.lineTo(width - m, height - m); ctx.lineTo(width - m, height - m - l)
            ctx.stroke()
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 26
        anchors.bottomMargin: 23
        visible: root.loaded && root.overlaysVisible
        text: "DRAG ORBIT   ·   CTRL+DRAG PAN   ·   SCROLL ZOOM"
        color: Qt.rgba(root.inkColor.r, root.inkColor.g, root.inkColor.b, 0.48)
        font.family: "monospace"
        font.pixelSize: 10
        font.letterSpacing: 0.8
        renderType: Text.NativeRendering
    }
}
