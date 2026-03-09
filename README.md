# Graphics_Project

A fire simulation UI built with OpenGL and ImGui.

## Building

1. Ensure you have CMake and GLFW installed.
   - On macOS: `brew install cmake glfw`

2. Clone or download ImGui (already included in include/).

3. Build:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

## Running

```bash
./FireSimulationUI
```

This will open a window with sliders to adjust fire simulation parameters:
- Fire Strength
- Wind Strength
- Wind Direction
- Smoke Density
- Enable Particles

The UI displays the current values of these parameters.