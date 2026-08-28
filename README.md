# Omagltf

A focused 3D model viewer for Omarchy. Omagltf opens GLB, glTF, and OBJ
assets directly through Qt Quick 3D. FBX files are converted to a temporary
GLB with Assimp, then loaded through the same rendering path.

## Features

- Open files from the picker, command line, or drag and drop
- Orbit, pan, zoom, and automatically frame model bounds
- Toggle the infinite grid and XYZ axes
- Preview GLB, glTF, OBJ, and FBX files live from Nautilus
- Follow the active Omarchy theme and accent color live
- Convert FBX asynchronously without blocking the interface
- Bundle JetBrains Mono Nerd Font for consistent typography
- Remember the last window geometry

## Requirements

Install the native dependencies on Omarchy:

```bash
omarchy pkg add qt6-base qt6-declarative qt6-quick3d assimp sushi
```

## Build and run

```bash
./bin/build
./bin/run
./bin/run path/to/model.glb
```

The build produces a single executable at `build/omagltf`; its QML interface
is embedded through Qt resources.

## Controls

| Input | Action |
|---|---|
| Drag | Orbit |
| Ctrl+drag | Pan |
| Scroll | Zoom |
| Ctrl+O | Open a model |
| F or R | Frame the model |
| G | Toggle the grid |
| A | Toggle the axes |
| Ctrl+Q | Quit |

Press Space on a supported model in Nautilus to open a compact live Qt Quick 3D
preview. The preview does not take keyboard focus, so the arrow keys continue
to navigate files through Sushi and Nautilus. Use "Open With Omagltf" for
orbit, pan, zoom, grid, axes, and file controls.

## Test

```bash
./bin/test
```

The test command validates shell scripts, desktop metadata, XML resources,
and, when Qt Quick 3D is installed, runs `qmllint` and performs a full build.

## Install as an Arch package

```bash
./bin/install
```

The included PKGBUILD intentionally builds the current local checkout. Before
publishing a release, replace that local-source arrangement with a tagged
source archive and a pinned checksum.

## Architecture

```text
GLB / glTF / OBJ -------------------+
                                    +--> RuntimeLoader --> View3D
FBX --> Assimp subprocess --> GLB --+

Nautilus Space --> Sushi navigation bridge --> omagltf --preview --> View3D
```

The QML side owns presentation and camera interaction. The C++ backend owns
file validation, command-line opening, metadata, window state, and temporary
FBX conversion. RuntimeLoader and Assimp parse complex third-party formats;
only open model files from sources you trust.

The Sushi renderer remains the keyboard and selection bridge while its model
surface is hidden. A persistent, non-focusable Qt Quick 3D window renders the
selected model directly; changing selection reuses that window instead of
capturing or caching a PNG. Non-model files continue to use Sushi normally.

## License

Omagltf is MIT licensed. The bundled JetBrains Mono Nerd Font is distributed
under the SIL Open Font License 1.1; see
`THIRD_PARTY_LICENSES/JetBrainsMonoNerd-OFL.txt`.
