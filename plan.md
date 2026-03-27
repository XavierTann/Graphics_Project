# Fluid Simulation Implementation Plan

## Overview
The goal is to replace the current particle-based fire simulation with a realistic, Mantaflow-style 3D Eulerian fluid simulation. The simulation will run on a 3D grid and use volumetric raymarching for rendering. The existing particle system will be retained and integrated to handle sparks and smoke.

## Key Technical Decisions
*   **Grid Resolution**: 64x64x64 (Provides a good balance between visual detail and real-time performance on the CPU/GPU).
*   **Advection Method**: MacCormack (Better detail preservation than standard Semi-Lagrangian, crucial for realistic fire).
*   **Rendering**: Volumetric Raymarching (Produces true 3D volumetric fire, rather than stacked 2D slices).
*   **Implementation Strategy**: Given that the project currently targets OpenGL 3.3 (or 4.1 on macOS) and doesn't use Compute Shaders, we have two options:
    1.  **CPU Solver + GPU Rendering**: Run the fluid solver on the CPU (using multi-threading if possible) and upload the resulting 3D textures to the GPU for raymarching. This is easier to implement but might be a bottleneck for a 64^3 grid (262,144 cells).
    2.  **GPU Solver (Compute Shaders)**: Upgrade the OpenGL context to 4.3+ and implement the fluid solver entirely on the GPU using Compute Shaders. This is much faster and highly recommended for real-time 3D fluid simulation.

*Given the performance requirements of a 64x64x64 3D fluid simulation, upgrading to OpenGL 4.3 and using Compute Shaders is strongly recommended. However, to minimize disruption, I will plan for a CPU-based solver first, with the option to migrate to Compute Shaders if performance is inadequate.*

## Implementation Steps

### 1. 3D Fluid Data Structures (CPU)
*   Create a `FluidSolver3D` class.
*   Implement 3D grids (arrays) for:
    *   Velocity (u, v, w)
    *   Density (smoke/soot)
    *   Temperature (drives buoyancy and color)
    *   Fuel/Reaction (optional, for burning)

### 2. Fluid Solver Core (CPU)
*   Implement the standard stable fluids pipeline (Jos Stam):
    *   **Add Sources**: Inject density and temperature from emitters (e.g., the lighter, campfire).
    *   **Buoyancy**: Apply upward force based on temperature.
    *   **Advection (MacCormack)**: Move velocity, density, and temperature fields along the velocity field.
    *   **Projection**: Enforce incompressibility (divergence-free velocity field) using Jacobi iteration or Conjugate Gradient.

### 3. GPU Integration & Rendering (Raymarching)
*   Create 3D Textures (`GL_TEXTURE_3D`) for Density and Temperature.
*   Every frame, upload the CPU grid data to these 3D textures.
*   Create a `VolumeRenderer` class.
*   Implement a Raymarching Fragment Shader:
    *   Calculate ray intersections with the bounding box of the fluid volume.
    *   Step along the ray through the 3D texture.
    *   Sample Temperature to determine fire color (using a color ramp/blackbody emission) and Density for smoke opacity.
    *   Accumulate color and alpha.

### 4. Integration with Existing System
*   **Particles (Sparks/Smoke)**: Modify the `ParticleSystem` to read velocity data from the `FluidSolver3D` to advect sparks and smoke particles realistically.
*   **Scene Objects**: Update `SceneObject` burning logic to act as heat/density sources for the fluid solver.
*   **UI**: Add ImGui controls for fluid parameters (viscosity, cooling rate, buoyancy strength, advection type toggle).

## Risk Assessment
*   **CPU Performance**: A 64^3 grid requires updating roughly 1-2 million floats per step. The CPU solver might struggle to maintain 60 FPS. If this occurs, we must transition to Compute Shaders (OpenGL 4.3+).
*   **Memory Bandwidth**: Uploading several 64^3 textures every frame from CPU to GPU might be slow.
