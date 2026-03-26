# Fire Simulator

Ember Engine is a real-time, interactive fire simulation tool designed for graphics research and game development. It allows users to simulate, visualize, and control fire dynamics using physically-inspired particle systems.

![Fire Simulation](https://via.placeholder.com/800x450?text=Ember+Engine+Preview)

## Features

### Core Simulation
- **Physically-based Particles**: Simulates fire behavior using buoyancy, wind, drag, and turbulence.
- **Realistic Rendering**: Uses additive blending, color ramps (Yellow -> Orange -> Red), and quadratic falloff for visual fidelity.
- **Turbulence**: Implements curl noise to create natural waving and curling flames.
- **Optimization**: Screen-space culling prevents rendering off-screen particles to save resources.
- **Fuel System**: Adds an overall fire lifetime via fuel depletion, with an optional infinite fuel mode.

### Interactive Controls (ImGui)
- **Wind Control**: Adjust wind direction/strength, toggle wind visualization, or disable wind entirely.
- **Tornado Mode**: Enables a spiral wind field with strength/radius/inflow/updraft controls.
- **Emitter Settings**: Real-time adjustment of fire radius, base size, particle lifetime, and speed.
- **Global Parameters**: Control buoyancy, cooling rate, and turbulence amplitude/frequency.
- **Presets**: Quick-start configurations for Lighter, Campfire, and Wildfire.
- **Save/Load**: Persist your simulation settings to `config.txt`.
- **Objects Panel**: Lists mesh files from `./data`. Objects can be placed, dragged in the scene, and set to burn/spread fire.

### Camera System
- **Orbit Camera**: Rotate around the fire using mouse drag.
- **Zoom**: Scroll or use `Z`/`X` keys to zoom in/out.
- **Pan**: Arrow keys to move the camera vertically.

## Building and Running

### Prerequisites
- **C++ Compiler**: MSVC (Windows), Clang (macOS), or GCC (Linux) with C++17 support.
- **CMake**: Version 3.10 or higher (optional for Windows if using VS Solution).
- **Dependencies**:
  - **GLFW**: Windowing and input.
  - **OpenGL**: 3.3 Core Profile.
  - **ImGui**: Included in `includes/`.
  - **GLM**: Included in `includes/`.
  - **Glad**: Included in `src/`.

### Windows
**Option 1: Visual Studio (Recommended)**
1. Open `FireSimulator.sln` in Visual Studio 2019/2022.
2. Select `x64` `Debug` or `Release` configuration.
3. Build and Run (F5).

**Option 2: CMake**
This option requires GLFW to be installed for CMake to find it.

1. Install prerequisites:
   - CMake
   - A C++17 compiler (Visual Studio Build Tools)
   - GLFW (recommended via vcpkg)
2. Install GLFW with vcpkg:
   ```powershell
   git clone https://github.com/microsoft/vcpkg
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg install glfw3:x64-windows
   ```
3. Configure and build:
   ```powershell
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake
   cmake --build . --config Release
   ```
4. Run:
   - `.\Release\FireSimulator.exe`

### macOS
1. Install dependencies via Homebrew:
   ```bash
   brew install cmake glfw
   ```
2. Build with CMake:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```
3. Run:
   ```bash
   ./FireSimulator
   ```

### Linux / Docker
The Docker image is intended for clean, reproducible builds. Running an OpenGL window inside Docker requires additional host setup (X11/Wayland forwarding and GPU access).

To build in a clean Linux environment using Docker:
```bash
docker build -t fire-simulator .
```

To run with X11 forwarding (example):
```bash
xhost +local:docker
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix fire-simulator
```

## Controls

| Input | Action |
| :--- | :--- |
| **Mouse Left Drag (empty space)** | Rotate Camera |
| **Mouse Left Drag (on object)** | Drag Object on its height plane |
| **Mouse Middle Drag** | Pan Camera |
| **Mouse Scroll** | Zoom In/Out |
| **Arrow Up/Down** | Move Camera Vertically |
| **Z / X** | Zoom In/Out |
| **F5** | Save Config |
| **F9** | Load Config |
| **ESC** | Exit |

## Project Structure
- `src/`: Source code (`main.cpp`, `Particles.cpp`, `BillboardRenderer.cpp`).
- `includes/`: Header libraries (ImGui, GLM, GLFW).
- `libs/`: Pre-compiled libraries for Windows.
- `shaders/`: Inline shaders defined in `shaderSource.h`.
- `data/`: Placeable object meshes (e.g., `.glb`). If empty, the Objects panel reports “No objects found.”

## Credits
- **OpenGL**: Graphics API.
- **Dear ImGui**: Immediate Mode GUI.
- **GLFW & GLAD**: Windowing and Loader.
