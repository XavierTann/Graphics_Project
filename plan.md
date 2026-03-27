# Plan: Implement 2D GPU Fluid Fire Simulation and Disable Smoke

## 1. Disable Smoke by Default
*   **Scene State:** Add a `bool enableSmoke = false;` to the `Scene` class.
*   **UI Integration:** Add a checkbox labeled "Enable Smoke" in `UI.cpp` (e.g., under Global Parameters or Emitter Settings).
*   **Simulation Logic:** In `Scene::update()`, conditionally wrap the smoke emission, update, and instance data building steps so that smoke particles only spawn and simulate when `enableSmoke` is true. If false, the smoke system should be cleared.

## 2. Upgrade OpenGL Context to 4.3
*   **main.cpp:** Modify the GLFW window hints to request OpenGL 4.3 Core Profile instead of 3.3. This provides native support for Compute Shaders, which will heavily optimize our grid-based simulation.

## 3. Implement 2D Grid-Based Fluid Simulation (Compute Shaders)
*   **FluidSystem Class:** Create `FluidSystem.h` and `FluidSystem.cpp`. This class will manage the grid dimensions, OpenGL textures, and Compute Shader dispatching.
*   **Ping-Pong Textures:** Allocate floating-point textures (e.g., `GL_RGBA16F`) for holding physical properties: Velocity, Density (Fire intensity), Temperature, and Pressure. We will need ping-pong pairs (read/write) for advection and Jacobi iterations.
*   **Compute Shaders:** Write the GLSL compute shaders to implement Stam's Stable Fluids algorithm (as described in GPU Gems 38):
    *   **Advection:** Semi-Lagrangian advection to move velocity, density, and temperature along the velocity field.
    *   **External Forces (Buoyancy & Injection):** Apply upward force based on temperature, and inject density/temperature/velocity at the emitter's location (representing burning objects and the main emitter).
    *   **Divergence:** Compute the divergence of the intermediate velocity field.
    *   **Pressure Solve (Jacobi):** Iterative solver to compute the pressure field from the divergence.
    *   **Projection:** Subtract the pressure gradient from the velocity to enforce mass conservation (divergence-free field).

## 4. Integrate and Render the Fluid Fire
*   **Scene Integration:** Replace the `flames` particle system in `Scene` with the new `FluidSystem`. Pass the emitter parameters and burnable object states to the fluid system to act as injection sources.
*   **Renderer Update:** Add `drawFluid()` to `Renderer`. 
*   **Billboard Rendering:** Render the fluid grid onto a 2D quad positioned at the main emitter. We will map the fluid density and temperature to a fire color gradient (using a fragment shader) to achieve a realistic fire look.

## 5. Clean up Particle Fire
*   Remove the old fire particle emission logic from `Scene.cpp`, but leave the smoke particle logic intact (since we're addressing smoke later, as requested).
