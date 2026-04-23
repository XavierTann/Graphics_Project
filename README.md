# Fire Simulator

## Overview

This project is a C++17 OpenGL real-time fire sandbox with two modes:

1. **Main simulation mode**
   - A configurable fire emitter with buoyancy, turbulence, wind, fuel, smoke, lighting, and burnable scene objects.
   - Interactive Dear ImGui tooling for presets, emitter tuning, wind/tornado parameters, lighting, and mesh placement.

2. **Secret boss mode**
   - A first-person mini-game unlocked by a secret code.
   - You fight `netherwing_pollux.glb` using short-lived projectiles, a temporary `HSR - Aventurine.glb` shield, and FPS-style controls.

Codbase includes:

- CPU-side particle simulation
- instanced billboard rendering for flames and smoke
- a custom GLB loader and mesh cache
- burn propagation and object consumption

## Current Features

### Main Fire Simulation

- Fire particles with buoyancy, turbulence, additive blending, and color progression.
- Smoke rendering with softer alpha blending and lighting response.
- Optional wind and tornado-style flow fields.
- Fuel system with infinite-fuel toggle, manual top-up, and burn-rate control.
- Burnable scene objects that:
  - ignite from the main emitter or nearby burning objects
  - emit their own flames while burning
  - visually burn away and disappear when fully consumed
  - disturb nearby particles while burning
- Mesh placement from the `data/` folder via the left panel.
- Preset fire settings for:
  - `Lighter`
  - `Campfire`
  - `Wildfire`

### Secret Boss Mode

- Enter secret code **`33550336`** in the main app to activate the mode.
- First-person player controller with mouse-look and WASD movement.
- Boss: `netherwing_pollux.glb`
- Player attack:
  - orange/yellow fireballs
  - intentionally short lifetime, so you must be within range
- Boss attack:
  - purple fire projectiles
  - split mid-flight toward the player
  - longer lifetime than the player's shots
- Shield mechanic:
  - hold block to spawn `HSR - Aventurine.glb` in front of the player
  - shield ignites and burns away if hit repeatedly
- Win/lose logic:
  - player dies after **3 hits**
  - boss dies when the model has burned away completely
  - repeated hits accelerate the boss burn
- Secret mode presentation:
  - hides both ImGui side panels
  - hides the decorative campfire mesh
  - keeps `Esc` as full app exit
  - uses `Shift + Esc` to return to the main simulation

## Controls

### Main Simulation Controls

| Input | Action |
| :--- | :--- |
| `Left drag` on empty space | Orbit camera |
| `Left drag` on object | Drag selected object on the ground plane |
| `Middle drag` | Pan camera |
| `Mouse wheel` | Zoom |
| `W A S D` | Move selected object on X/Y |
| `Q E` | Move selected object on Z |
| `R` | Restart simulation |
| `Esc` | Exit application |
| Type `33550336` | Enter secret boss mode |

### Secret Boss Mode Controls

| Input | Action |
| :--- | :--- |
| `Mouse` | Look around |
| `W A S D` | Move player |
| `Right click` | Shoot |
| `Left click (hold)` | Block / spawn shield |
| `Shift + Esc` | Exit secret mode and return to simulation |
| `Esc` | Exit application |

## Build And Run

### Requirements

- C++17 compiler
- OpenGL 3.3 Core support
- GLFW 3
- CMake 3.10+ for the CMake workflow

Bundled in the repository:

- GLAD
- GLM
- Dear ImGui
- stb_image

The post-build step copies `data/` next to the executable so mesh loading continues to work.

### Windows

Recommended:

- Open `FireSimulator.sln` in Visual Studio
- Select a build configuration
- Build and run

Alternative CMake workflow:

```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=PATH_TO_VCPKG\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
.\Release\FireSimulator.exe
```

### macOS

```bash
brew install cmake glfw
mkdir -p build && cd build
cmake ..
cmake --build .
./FireSimulator
```

### Linux

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libglfw3-dev libgl1-mesa-dev
mkdir -p build && cd build
cmake ..
cmake --build .
./FireSimulator
```

### Docker

The repository also includes a `Dockerfile` for containerized builds. GUI execution still requires host display and GPU setup.

```bash
docker build -t flamesimulator .
docker run -it --rm --name flamesimulator flamesimulator
```

## Runtime Architecture

### Application Flow

- `src/main.cpp`
  - creates the window and OpenGL context
  - initializes ImGui, shaders, renderer, scene, and camera
  - runs the main loop
  - routes input between normal simulation and secret-mode gameplay

### Scene / Gameplay Layer

- `src/Scene.h` / `src/Scene.cpp`
  - owns global simulation state
  - updates the particle system
  - updates burnable objects
  - manages wind/tornado/fuel settings
  - implements secret boss mode state, projectiles, shield behavior, and win/lose conditions

- `src/SceneObject.h` / `src/SceneObject.cpp`
  - represents a burnable mesh object
  - tracks local fuel, ash, alpha fade, ignition, and particle emission while burning

### Rendering Layer

- `src/Renderer.h` / `src/Renderer.cpp`
  - draws the grid, axis lines, marker point, meshes, billboards, and decorative meshes
  - manages the mesh shader and billboard draw pipeline

- `src/BillboardRenderer.h` / `src/BillboardRenderer.cpp`
  - uploads per-instance billboard data
  - renders instanced quads for fire and smoke particles

- `src/shaderSource.h`
  - stores the inline GLSL shader source for:
    - flame billboards
    - smoke billboards

### Camera / Input

- `src/Camera.h` / `src/Camera.cpp`
  - supports the orbit camera used in the sandbox
  - also supports FPS camera behavior for secret mode
  - stores yaw, pitch, radius, target, and derived view/projection matrices

### Assets / Mesh Loading

- `src/MeshLoader.h` / `src/MeshLoader.cpp`
  - scans the `data/` folder for `.glb` files
  - lazily loads and caches meshes
  - includes a custom GLB parser
  - stores CPU-side positions/indices for surface sampling and bounds

### UI Layer

- `src/UI.h` / `src/UI.cpp`
  - draws the custom-themed ImGui interface
  - provides:
    - asset browser
    - placed object list
    - simulation controls
    - status panel
    - tabs for fire, particles, wind, and lighting
  - automatically hides the side panels in secret mode

## Simulation Details

### Fire And Smoke

The fire system is built around a CPU-updated particle buffer with instanced billboard rendering:

- fire uses additive blending
- smoke uses alpha blending
- particle motion includes:
  - buoyancy
  - wind
  - curl-style turbulence
  - optional tornado swirl/inflow/updraft
  - local disturbances caused by burning objects

Particles transition visually from bright flame to darker smoke over their lifetime.

### Burning Objects

Placed meshes can burn and be consumed:

- ignition comes from the main emitter or from nearby burning objects
- burning objects emit additional flame particles from sampled surface points
- burn progression affects color, ash amount, transparency, and lifetime
- objects are removed once fully consumed

### Lighting

The fire and smoke shaders use a fire-light color/intensity/range model:

- changing flame color affects the fire and smoke colors:
  - fire color is set in the `globals.fireColor` uniform
  - smoke color is set in the `globals.smokeColor` uniform
  - intensity and range are set in the `globals.fireIntensity` and `globals.fireRange` uniforms
- main simulation defaults to a warm campfire tint (red-orange)
- secret mode uses a purple flame for the boss, and orange/yellow flames for the player

## Included Assets

Current `data/` contents:

- `campfire.glb`
- `cassette_tape.glb`
- `luna_park_grass_field.glb`
- `netherwing_pollux.glb`
- `HSR - Aventurine.glb`

These are discovered dynamically by the mesh loader and shown in the asset browser when appropriate.

## Source Tree

| Path | Purpose |
| :--- | :--- |
| `src/main.cpp` | App bootstrap, render loop, input dispatch |
| `src/Scene.*` | Simulation state and secret-mode gameplay |
| `src/SceneObject.*` | Burnable mesh object logic |
| `src/Particles.*` | Particle simulation and instance generation |
| `src/Renderer.*` | Rendering orchestration |
| `src/BillboardRenderer.*` | Instanced billboard upload/draw |
| `src/Camera.*` | Orbit + FPS camera behavior |
| `src/MeshLoader.*` | GLB scanning, loading, caching |
| `src/UI.*` | Dear ImGui panels and controls |
| `src/shaderSource.h` | Inline shader source |
| `data/` | Runtime mesh assets |
| `includes/` | Third-party headers |

## Notes

- The project uses `OpenGL` + `GLFW` directly rather than a game engine.
- Flame rendering uses inline shader strings instead of separate shader files.
- Secret mode is built into the same executable and scene system rather than being a separate app state manager.

## Difficulties

- Making the fire look more realistic without being too computationally expensive
- Considered methods to make realistic fire that was not used: `mantaflow`, `navier stokes`

## Credits

- OpenGL
- GLFW
- GLAD
- GLM
- Dear ImGui
- stb_image
- https://www.youtube.com/watch?v=p43rJ7bzHM8
- https://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/particles-instancing/
- https://learnopengl.com/In-Practice/2D-Game/Particles
