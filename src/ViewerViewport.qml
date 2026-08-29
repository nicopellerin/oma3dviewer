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
    property color gridMinorColor: Qt.rgba(mutedColor.r, mutedColor.g, mutedColor.b, 0.34)
    property color gridMajorColor: Qt.rgba(mutedColor.r, mutedColor.g, mutedColor.b, 0.48)
    // Blender's default theme axis colors. Qt Quick 3D is Y-up, so the
    // shader's internal Z line is presented as Blender's green ground axis.
    property color xAxisColor: "#ff3352"
    property color zAxisColor: "#8bdc00"
    property bool gridVisible: true
    property bool axesVisible: true
    property bool lightingEnabled: true
    property bool overlaysVisible: true
    property bool autoFrameOnResize: false
    property bool interactive: true

    readonly property bool loaded: modelLoader.status === RuntimeLoader.Success
    readonly property string loadError: modelLoader.status === RuntimeLoader.Error
                                                ? modelLoader.errorString : ""
    property vector3d boundsCenter: Qt.vector3d(0, 0, 0)
    property real boundsDiameter: 100
    property bool frameReady: false
    property int frameRetryCount: 0
    readonly property real defaultCameraPitch: 25
    readonly property real defaultCameraYaw: 35
    readonly property real defaultFrameFill: 0.64
    readonly property int maximumFrameRetries: 120
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
        var pitch = defaultCameraPitch * Math.PI / 180
        var yaw = -defaultCameraYaw * Math.PI / 180
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
        var fill = defaultFrameFill

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
        orbitOrigin.eulerRotation = Qt.vector3d(-defaultCameraPitch,
                                                defaultCameraYaw, 0)
        camera.position = Qt.vector3d(0, 0, Math.max(0.001, distance))
        camera.eulerRotation = Qt.vector3d(0, 0, 0)
        camera.clipNear = Math.max(0.0001,
                                   (distance - nearestDepth) * 0.25)
        var modelClipFar = distance - farthestDepth + diameter * 2
        var gridClipFar = (gridVisible || axesVisible) ? distance * 100 : 0
        camera.clipFar = Math.max(camera.clipNear + 1,
                                  modelClipFar,
                                  gridClipFar)
        frameReady = true
        return true
    }

    function scheduleFrame() {
        frameRetryCount = 0
        frameTimer.restart()
    }

    function applyFallbackMaterials(object) {
        if (!object)
            return

        if (object.hasOwnProperty("materials")
                && object.materials.length === 0) {
            // glTF's implicit material is fully metallic, so its brightness
            // can shift as the view moves. Keep authored materials intact and
            // give only material-less meshes a stable neutral surface.
            object.materials.push(fallbackMaterial)
        }

        for (var childIndex = 0;
                childIndex < object.children.length;
                ++childIndex) {
            applyFallbackMaterials(object.children[childIndex])
        }
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
            specularAAEnabled: true
            tonemapMode: SceneEnvironment.TonemapModeAces
            // Screen-space AO changes with projected model size, making the
            // same material appear lighter or darker while zooming.
            aoEnabled: false
        }

        DefaultMaterial {
            id: fallbackMaterial
            diffuseColor: "#b8b8b8"
            specularAmount: 0
        }

        Node {
            id: orbitOrigin
            eulerRotation: Qt.vector3d(-root.defaultCameraPitch,
                                       root.defaultCameraYaw, 0)

            PerspectiveCamera {
                id: camera
                position: Qt.vector3d(0, 0, 200)
                fieldOfView: 42
                clipNear: 0.1
                clipFar: 10000

                // Camera-relative lights provide matcap-like form shading.
                // Their high ambient fill keeps rear surfaces readable.
                DirectionalLight {
                    visible: !root.lightingEnabled
                    eulerRotation: Qt.vector3d(-18, 24, 0)
                    brightness: 0.48
                    color: "#f4f4f4"
                    ambientColor: Qt.rgba(0.32, 0.32, 0.34, 1)
                    castsShadow: false
                }

                DirectionalLight {
                    visible: !root.lightingEnabled
                    eulerRotation: Qt.vector3d(22, -145, 0)
                    brightness: 0.16
                    color: "#d8d8dc"
                    castsShadow: false
                }
            }
        }

        // A view-centred plane covers the visible ground while the shader uses
        // world coordinates, so the grid and axes remain fixed at the origin.
        Model {
            id: infiniteGrid
            visible: root.gridVisible || root.axesVisible
            source: "#Rectangle"
            readonly property real viewDistance: Math.max(
                camera.clipNear * 2,
                Math.sqrt(camera.position.x * camera.position.x
                        + camera.position.y * camera.position.y
                        + camera.position.z * camera.position.z))
            position: Qt.vector3d(orbitOrigin.position.x,
                                  -viewDistance * 0.0005,
                                  orbitOrigin.position.z)
            eulerRotation.x: -90

            // Keeping this proportional to zoom distance avoids precision loss
            // from interpolating across a needlessly huge quad at close range.
            readonly property real extent: viewDistance * 100
            scale: Qt.vector3d(extent / 100, extent / 100, 1)
            castsShadows: false
            receivesShadows: false

            materials: CustomMaterial {
                shadingMode: CustomMaterial.Unshaded
                cullMode: CustomMaterial.NoCulling
                depthDrawMode: CustomMaterial.NeverDepthDraw
                sourceBlend: CustomMaterial.SrcAlpha
                destinationBlend: CustomMaterial.OneMinusSrcAlpha

                property real gridSpacing: root.gridInterval
                property real gridPixelWidth: 1.0
                property real gridEnabled: root.gridVisible ? 1.0 : 0.0
                property real axesEnabled: root.axesVisible ? 1.0 : 0.0
                property color minorGridColor: root.gridMinorColor
                property color majorGridColor: root.gridMajorColor
                property color xGridAxisColor: root.xAxisColor
                property color zGridAxisColor: root.zAxisColor

                vertexShader: "shaders/infinitegrid.vert"
                fragmentShader: "shaders/infinitegrid.frag"
            }
        }

        DirectionalLight {
            visible: root.lightingEnabled
            eulerRotation: Qt.vector3d(-42, -35, 0)
            brightness: 0.95
            color: "#fff8f0"
            ambientColor: Qt.rgba(0.07, 0.075, 0.09, 1)
            castsShadow: true
            shadowFactor: 52
            shadowMapQuality: Light.ShadowMapQualityHigh
            // Keep the model inside the shadow map as the orbit camera dollies.
            shadowMapFar: Math.max(1,
                                   camera.position.z + root.boundsDiameter * 4)
            shadowBias: Math.max(0.0001, root.boundsDiameter * 0.001)
            softShadowQuality: Light.PCF16
            pcfFactor: Math.max(0.001, root.boundsDiameter * 0.008)
        }

        DirectionalLight {
            visible: root.lightingEnabled
            eulerRotation: Qt.vector3d(-12, 58, 0)
            brightness: 0.28
            color: "#e8f0ff"
            castsShadow: false
        }

        DirectionalLight {
            visible: root.lightingEnabled
            eulerRotation: Qt.vector3d(28, 155, 0)
            brightness: 0.14
            color: "#ffffff"
            castsShadow: false
        }

        RuntimeLoader {
            id: modelLoader
            source: root.source
            onSourceChanged: {
                frameTimer.stop()
                materialTimer.stop()
                root.frameRetryCount = 0
                root.frameReady = false
            }
            onBoundsChanged: root.scheduleFrame()
            onChildrenChanged: {
                if (status === RuntimeLoader.Success)
                    materialTimer.restart()
            }
            onStatusChanged: {
                if (status === RuntimeLoader.Success) {
                    root.scheduleFrame()
                    materialTimer.restart()
                }
            }
        }
    }

    Timer {
        id: materialTimer
        interval: 0
        onTriggered: root.applyFallbackMaterials(modelLoader)
    }

    Timer {
        id: frameTimer
        interval: 16
        onTriggered: {
            if (root.frameModel()) {
                root.frameRetryCount = 0
                return
            }

            if (modelLoader.status === RuntimeLoader.Success
                    && root.frameRetryCount < root.maximumFrameRetries) {
                root.frameRetryCount += 1
                restart()
            }
        }
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

    Row {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 18
        anchors.bottomMargin: 14
        visible: root.loaded && root.overlaysVisible
        spacing: 18

        property color hintColor: Qt.rgba(root.inkColor.r,
                                           root.inkColor.g,
                                           root.inkColor.b, 0.48)

        BottomLabelPair {
            keyText: "DRAG"
            valueText: "ORBIT"
            textColor: parent.hintColor
        }

        BottomLabelPair {
            keyText: "CTRL+DRAG"
            valueText: "PAN"
            textColor: parent.hintColor
        }

        BottomLabelPair {
            keyText: "SCROLL"
            valueText: "ZOOM"
            textColor: parent.hintColor
        }
    }
}
