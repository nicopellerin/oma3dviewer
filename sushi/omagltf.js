imports.gi.versions.Gtk = '3.0';

const {Gio, GLib, GObject, Gtk} = imports.gi;

const Renderer = imports.ui.renderer;

let activeRenderers = 0;
let closeSourceId = 0;
let controllerWindow = null;
let controllerWasVisible = false;
let navigationBridge = null;

const NAVIGATION_BRIDGE_XML = `
<node>
  <interface name="io.nicopellerin.OmagltfBridge">
    <method name="Select">
      <arg type="u" name="direction" direction="in"/>
    </method>
  </interface>
</node>`;

function ensureNavigationBridge() {
    if (navigationBridge)
        return;

    const implementation = {
        Select(direction) {
            Gio.DBus.session.emit_signal(
                null,
                '/org/gnome/NautilusPreviewer',
                'org.gnome.NautilusPreviewer2',
                'SelectionEvent',
                new GLib.Variant('(u)', [direction]));
        },
    };
    navigationBridge = Gio.DBusExportedObject.wrapJSObject(
        NAVIGATION_BRIDGE_XML, implementation);
    navigationBridge.export(
        Gio.DBus.session,
        '/org/gnome/NautilusPreviewer/OmagltfBridge');
}

function executablePath() {
    return GLib.find_program_in_path('omagltf');
}

function invokePreview(arguments_) {
    const executable = executablePath();
    if (!executable)
        return false;

    try {
        Gio.Subprocess.new(
            [executable, ...arguments_],
            Gio.SubprocessFlags.STDOUT_SILENCE |
                Gio.SubprocessFlags.STDERR_SILENCE);
        return true;
    } catch (error) {
        logError(error, 'Could not launch the Omagltf live preview');
        return false;
    }
}

function cancelScheduledClose() {
    if (closeSourceId === 0)
        return;
    GLib.source_remove(closeSourceId);
    closeSourceId = 0;
}

function hideControllerWindow(widget) {
    if (activeRenderers === 0)
        return GLib.SOURCE_REMOVE;

    const window = widget.get_toplevel();
    if (!(window instanceof Gtk.Window))
        return GLib.SOURCE_REMOVE;

    if (controllerWindow !== window) {
        controllerWindow = window;
        controllerWasVisible = true;
    }
    window.hide();
    return GLib.SOURCE_REMOVE;
}

function restoreControllerWindow() {
    if (!controllerWindow)
        return;
    try {
        if (controllerWasVisible) {
            controllerWindow.show_all();
            controllerWindow.present();
        }
    } catch (error) {
        // The Sushi window may already have been destroyed.
    }
    controllerWindow = null;
    controllerWasVisible = false;
}

function schedulePreviewClose() {
    cancelScheduledClose();
    closeSourceId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 300, () => {
        closeSourceId = 0;
        if (activeRenderers === 0) {
            restoreControllerWindow();
            invokePreview(['--preview-close']);
        }
        return GLib.SOURCE_REMOVE;
    });
}

var Klass = GObject.registerClass({
    Implements: [Renderer.Renderer],
    Properties: {
        fullscreen: GObject.ParamSpec.boolean(
            'fullscreen', '', '', GObject.ParamFlags.READABLE, false),
        ready: GObject.ParamSpec.boolean(
            'ready', '', '', GObject.ParamFlags.READABLE, false),
    },
}, class OmagltfRenderer extends Gtk.DrawingArea {
    get ready() {
        return !!this._ready;
    }

    get fullscreen() {
        return !!this._fullscreen;
    }

    get hasToolbar() {
        return false;
    }

    get canFullscreen() {
        return false;
    }

    get moveOnClick() {
        return false;
    }

    get resizable() {
        return false;
    }

    get resizePolicy() {
        return Renderer.ResizePolicy.NAT_SIZE;
    }

    _init(file) {
        super._init();

        this._bridgeActive = false;
        this.set_size_request(1, 1);
        this.connect('realize', () => hideControllerWindow(this));
        this.connect('map', () => hideControllerWindow(this));
        this.connect('destroy', this._onDestroy.bind(this));

        const sourcePath = file.get_path();
        if (!sourcePath || !invokePreview(['--preview', sourcePath])) {
            log('Omagltf live preview requires a local model and executable.');
            this.isReady();
            return;
        }

        this._bridgeActive = true;
        activeRenderers++;
        cancelScheduledClose();
        ensureNavigationBridge();
        GLib.idle_add(
            GLib.PRIORITY_DEFAULT_IDLE,
            () => hideControllerWindow(this));
        this.isReady();
    }

    vfunc_get_preferred_width() {
        return [1, 1];
    }

    vfunc_get_preferred_height() {
        return [1, 1];
    }

    vfunc_draw() {
        return false;
    }

    _onDestroy() {
        if (!this._bridgeActive)
            return;
        this._bridgeActive = false;
        activeRenderers = Math.max(0, activeRenderers - 1);
        if (activeRenderers === 0)
            schedulePreviewClose();
    }
});

var mimeTypes = [
    'model/gltf-binary',
    'model/gltf+json',
    'model/obj',
    'model/x-fbx',
    'application/x-blender',
];
