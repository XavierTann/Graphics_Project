# Fire Simulator

## Quick summary

**FireSimulator** is a C++17 OpenGL application that runs an interactive, real-time fire effect using a GPU-oriented particle system (buoyancy, wind, turbulence, additive flame rendering) and Dear ImGui controls. You can orbit the scene, place and move mesh objects from the `data/` folder, and tune simulation parameters (wind, tornado mode, emitter shape, presets, fuel, and more).

---

## Compile and run

### Prerequisites

- **C++17** compiler (Clang on macOS, GCC on Linux, MSVC on Windows).
- **CMake** 3.10 or newer.
- **OpenGL** 3.3 Core context support.
- **GLFW 3** (windowing). ImGui, GLM, and Glad ship with this repository under `includes/` and `src/`.

The CMake step copies `data/` next to the built executable so meshes load correctly.

### macOS

```bash
brew install cmake glfw
mkdir -p build && cd build
cmake ..
cmake --build .
./FireSimulator
```

### Linux (native)

Install GLFW and OpenGL development packages (names vary by distro). Example on Debian/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libglfw3-dev libgl1-mesa-dev
mkdir -p build && cd build
cmake ..
cmake --build .
./FireSimulator
```

### Windows

**Visual Studio (recommended):** open `FireSimulator.sln`, choose **x64** and **Debug** or **Release**, then build and run (F5).

**CMake:** install a C++ toolchain and GLFW (for example via [vcpkg](https://github.com/microsoft/vcpkg)), then:

```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=PATH_TO_VCPKG\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
.\Release\FireSimulator.exe
```

### Docker (Linux build; display needs host setup)

Build a reproducible binary (running the GUI still requires X11/Wayland forwarding and GPU access from the host):

```bash
docker build -t fire-simulator .
```

Example with X11 (adjust for your security preferences):

```bash
xhost +local:docker
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix fire-simulator
```

---

## Features (overview)

- Particle fire with wind, optional tornado field, curl-style turbulence, and color ramping.
- ImGui panels for wind, emitter, globals, presets (e.g. lighter / campfire / wildfire), and object list from `./data`.
- Orbit / zoom / pan camera; place, drag, and keyboard-nudge scene objects; optional fire spread on objects.

## Controls

| Input | Action |
| :--- | :--- |
| **Left drag (empty space)** | Orbit camera |
| **Left drag (on object)** | Drag object on the floor (X/Y) |
| **Middle drag** | Pan camera |
| **Scroll** | Zoom |
| **W A S D** | Move selected object on floor |
| **Q E** | Move selected object up/down |
| **ESC** | Exit |

## Project layout

| Path | Role |
| :--- | :--- |
| `src/` | Application code (`main.cpp`, particles, rendering, UI, scene) |
| `includes/` | ImGui, GLM, headers; GLFW from the system or toolchain |
| `shaders/` / `shaderSource.h` | GLSL sources used by the app |
| `data/` | GLTF/GLB meshes for the objects panel |

## Credits

OpenGL; Dear ImGui; GLFW; GLAD; GLM; LearnOpenGL-style helpers under `includes/learnopengl/`.
