# Final Report Draft

This draft follows the same structure as the final presentation and expands each part into report-style writing. Replace bracketed placeholders such as measured FPS values, group member names, and figure references with your final material before submission.

## 1. Introduction

### 1.1 Project Title
**Ember Engine: A Real-Time Interactive Fire and Smoke Simulation in a 3D Environment**

### 1.2 Problem Statement

#### 1.2.1 Graphics Problem
Real-time fire is a challenging graphics problem because it must appear dynamic, believable, and responsive while still running efficiently enough for interactive applications. Many existing real-time fire effects rely on pre-animated textures or loosely parameterized particle systems. Although these methods can produce visually appealing flames, they often provide limited control over physically meaningful factors such as wind, buoyancy, turbulence, smoke generation, and fuel. As a result, the visual effect may look acceptable in isolation, but it does not respond convincingly to environmental changes or user interaction.

#### 1.2.2 Project Goal
The goal of this project is to design and implement a real-time fire simulation system that allows users to control flame behavior in an interactive 3D scene. The system aims to support parameter-driven fire and smoke motion, environmental interaction with scene objects, and immediate visual feedback through a graphical user interface. In addition to rendering the main fire emitter, the system also supports object ignition, burning progression, wind control, smoke generation, and lighting effects.

#### 1.2.3 Why This Problem Matters
This problem is worth addressing because controllable fire effects are useful in graphics education, real-time simulation, game prototyping, and visual experimentation. A system that allows users to change fire parameters and immediately observe the effect helps bridge the gap between simple visual effects and more physically inspired simulation. It also demonstrates how particle systems, procedural motion, rendering techniques, and scene interaction can be combined into one coherent graphics application.

## 2. Approach

### 2.1 Overall Approach
To address the problem, the project uses a **physically inspired particle-based approach** rather than pre-animated textures. Fire and smoke are represented as particles that evolve over time according to external forces and age-based visual changes. This design allows the simulation to remain lightweight enough for real-time rendering while still producing expressive and responsive motion.

The approach combines three main ideas:

1. **Particle-based flame and smoke simulation** for real-time motion and appearance.
2. **Environmental interaction** through wind, turbulence, tornado-like motion, and burnable scene objects.
3. **Interactive control** through a real-time user interface built with Dear ImGui.

The overall objective is not to reproduce physically exact combustion, but to create a controllable and visually convincing fire system that responds naturally to user input and scene conditions.

### 2.2 System Pipeline

#### 2.2.1 Scene Initialization
At startup, the program initializes the window, OpenGL context, shaders, renderer, camera, particle system, and user interface. The main emitter is configured with initial values such as particle radius, particle size, launch speed, and lifetime. Global environmental parameters such as wind, buoyancy, cooling, and turbulence are also initialized.

In addition, the mesh loader scans the `data/` directory and makes available the 3D assets that can be inserted into the scene as interactive objects.

#### 2.2.2 Parameter Control
During runtime, users can change simulation parameters through the control panel. Parameters include:

- emitter size and particle launch speed,
- wind direction and wind strength,
- turbulence amplitude and frequency,
- tornado parameters,
- smoke visibility,
- fuel settings, and
- fire lighting settings.

These values are directly mapped to the simulation state and are applied in the next update step, enabling immediate feedback.

#### 2.2.3 Particle Emission
Each frame, the scene computes the current fire intensity and generates particles from the active fire source. Particles are emitted from the main fire emitter and from burning objects in the scene. When objects burn, particle spawn positions are sampled from the object surface so that the flame appears attached to the geometry instead of coming from a single point.

#### 2.2.4 Particle Update
After emission, each particle is updated over time using a simple time-stepping process:

\[
\mathbf{v}_{t+\Delta t} = \mathbf{v}_t + \Delta t \cdot \mathbf{F}
\]

\[
\mathbf{x}_{t+\Delta t} = \mathbf{x}_t + \Delta t \cdot \mathbf{v}_{t+\Delta t}
\]

where \(\mathbf{F}\) is the combined force from wind, buoyancy, turbulence, tornado motion, and local disturbance effects. Particle color, opacity, and size are also updated according to lifetime so that bright flames gradually transition into darker smoke.

#### 2.2.5 Object Burning
Burnable scene objects are updated each frame. Objects can ignite when they are close to the main emitter or another burning object. Once burning, they:

- lose fuel over time,
- emit additional particles,
- gradually increase in ash value, and
- visually fade as they are consumed.

This creates a stronger relationship between the fire effect and the 3D environment.

#### 2.2.6 Rendering
The simulation data is converted into renderable instance data. Flames and smoke are drawn as camera-facing billboards for efficiency. Scene objects and decorative meshes are rendered as standard 3D meshes. Additional visual elements such as the floor grid, wind arrow, axis labels, and dynamic fire lighting help make the simulation easier to interpret.

### 2.3 Techniques and Algorithms

#### 2.3.1 Particle System
The main simulation uses a particle system because it offers a good balance between performance and expressiveness. Each particle stores position, velocity, lifetime, color, size, and a seed value used for procedural variation.

#### 2.3.2 Wind and Buoyancy
Wind adds directional force to the flame and smoke, while buoyancy drives upward movement. Together, these two forces produce the main large-scale motion of the fire.

#### 2.3.3 Curl-Noise Turbulence
The project uses curl-noise-inspired turbulence to create swirling, fluid-like motion. This prevents the fire from looking too rigid or too predictable and adds natural variation to both flames and smoke.

#### 2.3.4 Tornado and Disturbance Fields
An optional tornado mode adds tangential swirl, inward pull, and upward lift around the emitter. Burnable objects also act as local disturbance sources, pushing and swirling nearby particles. These features help make the fire respond more dynamically to the scene.

#### 2.3.5 Mesh Surface Sampling
When objects burn, the system samples points from mesh geometry so that fire particles appear to spread across the object surface. This improves realism compared to emitting all particles from the object center.

#### 2.3.6 Billboard Rendering and Culling
Flames and smoke are rendered as billboards rather than full geometry. Additive blending is used for flames and alpha blending is used for smoke. Frustum culling is applied before building instance data so that particles outside the camera view are skipped.

## 3. Implementation

### 3.1 Development Environment
The project is implemented in **C++17** and uses **OpenGL** as the graphics API. The application is built with **CMake**, and the current codebase is configured to compile on macOS, Windows, and Linux with the appropriate compiler and OpenGL/GLFW setup.

The main libraries used in the implementation are:

- **GLFW** for window creation and input handling,
- **GLAD** for OpenGL function loading,
- **GLM** for vector and matrix mathematics, and
- **Dear ImGui** for the real-time control interface.

### 3.2 Software Architecture
The project is organized into several core modules:

- `main.cpp`: application entry point, render loop, input handling, and subsystem initialization.
- `Scene.cpp` / `Scene.h`: overall simulation state, particle spawning, object burning logic, and per-frame scene update.
- `Particles.cpp` / `Particles.h`: particle update rules, forces, lifetime transitions, and render instance generation.
- `Renderer.cpp` / `Renderer.h`: OpenGL draw calls for grids, markers, meshes, flames, smoke, and decorations.
- `MeshLoader.cpp` / `MeshLoader.h`: custom loading and caching of `.glb` assets from the `data/` directory.
- `UI.cpp` / `UI.h`: Dear ImGui interface for parameter control and object management.
- `Camera.cpp` / `Camera.h`: orbit-style camera movement and projection setup.

This separation helps keep simulation logic, rendering logic, asset handling, and user interaction relatively modular.

### 3.3 Simulation Implementation
The simulation is centered around a configurable emitter and a particle system. The emitter defines where particles spawn and how they are launched, while the particle system updates each particle every frame.

Key implementation features include:

- configurable emitter radius, size, lifetime, and speed,
- wind and buoyancy force application,
- turbulence through procedural curl noise,
- smoke generation based on particle age,
- spark particles for extra detail,
- per-object ignition and burning progression, and
- intensity scaling through fuel depletion.

Each scene object stores its own burnability, fuel amount, burn rate, ash progression, alpha state, and disturbance parameters. This allows objects to behave independently while still interacting with the same fire system.

### 3.4 Rendering Implementation
The rendering system uses multiple passes for different types of content:

1. **Grid and reference markers** are rendered first to provide spatial context.
2. **Scene objects and decorative meshes** are rendered as textured or colored geometry.
3. **Flame billboards** are rendered with additive blending to produce a bright glowing effect.
4. **Smoke billboards** are rendered with alpha blending and sorted for better transparency results.

The project also includes a dynamic fire-lighting model for billboards, allowing the flames and smoke to visually respond to a local fire light source. This improves depth and overall scene coherence.

### 3.5 User Interface Implementation
The user interface is implemented with Dear ImGui and provides direct control over the simulation. The current interface includes:

- an **asset library panel** for selecting and adding meshes into the scene,
- a **placed objects panel** for managing inserted objects,
- a **control panel** for adjusting fire, particles, wind, and lighting parameters, and
- preset buttons for quickly switching between predefined fire behaviors.

The interface was designed not only as a control panel but also as a debugging and experimentation tool. It allows users to understand the effect of each parameter by changing it during runtime and observing the result immediately.

### 3.6 Input Data and Asset Preparation
The scene uses 3D assets stored in the `data/` directory in `.glb` format. The mesh loader parses mesh data, stores GPU buffers, extracts CPU vertex data, and computes bounding information used for:

- automatic object scaling,
- orientation handling,
- mesh surface sampling, and
- object placement on the scene floor.

This means the input data is not just rendered visually; it is also used by the simulation logic for ignition and fire spread.

### 3.7 Additional Interactive Features
Besides the core fire simulation, the current codebase also includes an additional hidden interaction mode that behaves like a small gameplay extension. Although this feature is not the main focus of the report, it demonstrates that the scene system and fire simulation are flexible enough to support more advanced interactions beyond a static demo environment.

## 4. Results

### 4.1 Intermediate Results
The project produced several intermediate components before reaching the final integrated result:

- a working particle-based flame emitter,
- smoke generation driven by particle age,
- wind and turbulence controls,
- interactive object insertion and burning,
- a polished real-time UI for simulation control, and
- a custom `.glb` asset pipeline for scene objects.

These intermediate results were important because they validated each subsystem individually before full integration.

**Figure Placeholder 1:** Insert a screenshot of the main control interface and emitter setup here.

**Figure Placeholder 2:** Insert a screenshot showing object placement and the asset library here.

### 4.2 Final Visual Result
The final system is able to render a controllable fire and smoke simulation inside a 3D scene. Users can place objects into the environment, ignite them through proximity to the main emitter, and observe how the fire changes under different wind, turbulence, and fuel settings.

The final visual result includes:

- layered flame and smoke behavior,
- visible response to environmental forces,
- object burning and fading,
- dynamic lighting contribution from the fire, and
- a real-time interface for experimentation.

Compared with a static fire sprite or a basic emitter, the final result is more interactive and more strongly connected to the environment.

**Figure Placeholder 3:** Insert a screenshot of the standard fire simulation scene here.

**Figure Placeholder 4:** Insert a screenshot demonstrating smoke behavior or wind response here.

**Figure Placeholder 5:** Insert a screenshot showing object ignition and progressive burning here.

**Figure Placeholder 6:** Insert a screenshot showing different presets or parameter variations here.

### 4.3 Performance Observations
The project is designed to run in real time on a desktop system using OpenGL and instanced billboard rendering. Performance is supported by several design choices:

- billboard rendering instead of full volumetric simulation,
- frustum culling before particle instance generation,
- particle count caps,
- mesh caching, and
- simple force-based particle updates.

The current control panel also exposes the frame rate during runtime, which provides immediate performance feedback during testing.

You may insert measured values here after testing on your machine:

| Scene Condition | Approximate FPS |
| --- | --- |
| Default fire only | [Fill in] |
| Fire with smoke enabled | [Fill in] |
| Fire with multiple burning objects | [Fill in] |
| Highest stress case tested | [Fill in] |

## 5. Discussion

### 5.1 Advantages of the Approach
The main advantage of this approach is that it provides a good balance between **visual quality**, **runtime performance**, and **user control**. The particle system is lightweight enough for real-time use, while still being flexible enough to support wind, turbulence, smoke, burning objects, and dynamic interaction.

Another strength is the integration of simulation and user interaction. Instead of being a passive fire effect, the project allows the user to actively experiment with parameters and immediately observe the results. This makes the system useful not only for presentation but also for learning and prototyping.

### 5.2 Limitations
Although the system is visually effective, it is still an approximation rather than a physically exact combustion model. Some limitations include:

- no full fluid simulation for flames and smoke,
- simplified ignition and burning logic,
- transparency and ordering challenges common to particle rendering,
- dependence on hand-tuned parameters for the best results, and
- limited quantitative evaluation in the current version.

The project prioritizes real-time interactivity and controllability over strict physical accuracy.

### 5.3 Lecture Knowledge Applied
This project draws on several important graphics concepts covered in class:

- the graphics pipeline and real-time rendering workflow,
- transformations, camera projection, and billboarding,
- shader-based rendering,
- blending for semi-transparent visual effects,
- particle systems for dynamic phenomena,
- mesh loading and scene representation, and
- user interaction in real-time graphics applications.

These topics were particularly important in making the project both functional and visually coherent.

### 5.4 New Knowledge Learned During the Project
In addition to course material, the project required learning several practical skills beyond standard lecture content, such as:

- tuning a particle system for stylized but believable fire motion,
- using procedural noise to create turbulence,
- combining mesh geometry with simulation logic,
- designing a usable interactive graphics interface, and
- debugging the interaction between simulation state, rendering, and input handling.

This made the project valuable not only as a graphics implementation exercise, but also as a software integration task.

## 6. Conclusion

### 6.1 Project Summary
This project presented Ember Engine, a real-time interactive fire and smoke simulation system implemented in C++ and OpenGL. The system combines particle-based flame rendering, smoke generation, wind and turbulence control, burnable scene objects, and an interactive user interface within a unified 3D environment.

Rather than focusing on fully physically accurate combustion, the project aimed to create a controllable and believable real-time fire effect that responds to environmental parameters and user interaction. The final result demonstrates that a physically inspired particle system, when combined with scene interaction and a well-designed interface, can produce an engaging and flexible graphics application.

### 6.2 What Was Gained from the Project
From this project, we gained practical experience in building a full interactive graphics application rather than an isolated rendering demo. The work required combining multiple areas of computer graphics, including particle simulation, shading, blending, mesh handling, user interaction, and scene management.

More importantly, the project showed how technical graphics concepts can be turned into a tool that users can directly explore. The result is not only a fire simulation, but also a platform for testing, understanding, and presenting real-time environmental effects.

## 7. References

This section is optional. You may include references such as:

- course lecture notes,
- OpenGL documentation,
- Dear ImGui documentation,
- GLFW documentation,
- GLM documentation, and
- any references used for particle systems, turbulence, or real-time fire rendering.

Example placeholder format:

1. [Course note / lecture title]
2. [OpenGL reference]
3. [Paper or article on particle-based fire simulation]
