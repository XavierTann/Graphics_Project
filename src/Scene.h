#pragma once
#include "Particles.h"
#include "SceneObject.h"
#include <vector>
#include <glm/glm.hpp>

// Scene owns all simulation state: particle systems, scene objects,
// emitter/global settings, fuel, wind, and tornado parameters.
class Scene {
public:
    // ---- Emitter & global physics ----
    EmitterSettings emitter;
    GlobalParams    globals;

    // ---- Wind ----
    bool        enableWind    = true;
    bool        showWind      = false;
    float       windStrength  = 1.0f;

    // ---- Tornado ----
    bool  tornadoMode     = false;
    float tornadoStrength = 4.0f;
    float tornadoRadius   = 2.5f;
    float tornadoInflow   = 1.5f;
    float tornadoUpdraft  = 2.0f;

    // ---- Global fuel (main emitter) ----
    bool  fuelEnabled    = true;
    bool  fuelBlowAway   = true;
    bool  fuelInfinite   = false;
    float fuelMax        = 20.0f;
    float fuel           = 20.0f;
    float fuelBurnRate   = 1.0f;
    float addFuelAmount  = 5.0f;

    // ---- Smoke ----
    bool smokeEnabled = true;

    // ---- Lighting ----
    glm::vec3 fireLightColor = glm::vec3(1.0f, 0.48f, 0.18f);
    float fireLightIntensity = 2.8f;
    float fireLightRange = 3.2f;

    // ---- Scene objects ----
    std::vector<SceneObject> objects;
    int selectedObjectIndex = -1;

    // Non-owning pointer to the mesh name list inside Renderer's MeshLoader.
    // Set by App::initSubsystems() after Renderer::init() runs.
    // UI reads this to populate the file browser without depending on Renderer.
    const std::vector<std::string>* availableMeshNames = nullptr;

    // ---- Particle systems (public so Renderer can read instance data) ----
    ParticleSystem flames;
    ParticleSystem smokeSys;

    // ---- Derived instance data (filled by update, read by Renderer) ----
    std::vector<InstanceAttrib> flameInstData;
    std::vector<InstanceAttrib> smokeInstData;
    std::vector<InstanceAttrib> objectInstData;

    // Call once after construction to apply default settings.
    void init();

    // Advance the simulation by dt seconds.
    // viewProj is needed for billboard culling.
    void update(float dt, float time, const glm::mat4& viewProj);

    // Reset particle systems and refill fuel.
    void reset();

    // ---- Fuel helpers ----
    void addFuel() { fuel = std::min(fuelMax, fuel + addFuelAmount); }

    // Current intensity [0,1] derived from fuel
    float intensity() const;

    // Apply a named preset to emitter/globals
    void applyPreset(const std::string& name);

    glm::vec3 fireLightPosition() const;
    float fireLightStrength() const;

private:
    // Build the fueled copies of emitter/globals for this frame
    EmitterSettings makeFueledEmitter(float intens) const;
    GlobalParams    makeFueledGlobals(float intens) const;
    bool smokeWasEnabled_ = true;
};
