#pragma once
#include "Particles.h"
#include "SceneObject.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

class MeshLoader;

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
    bool  fuelInfinite   = true;
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

    MeshLoader* meshLoader = nullptr;

    // ---- Particle systems (public so Renderer can read instance data) ----
    ParticleSystem flames;

    // ---- Derived instance data (filled by update, read by Renderer) ----
    std::vector<InstanceAttrib> flameInstData;
    std::vector<InstanceAttrib> smokeInstData;
    std::vector<InstanceAttrib> secretBossFlameInstData;
    std::vector<InstanceAttrib> secretPlayerFlameInstData;

    // Call once after construction to apply default settings.
    void init();

    // Advance the simulation by dt seconds.
    // viewProj is needed for billboard culling.
    void update(float dt, float time, const glm::mat4& viewProj, const glm::vec3& cameraPos, const glm::vec3& cameraForward);

    // Reset particle systems and refill fuel.
    void reset();

    bool isSecretMode() const { return secret_.active; }
    void enterSecretMode();
    void exitSecretMode();
    void secretSetBlocking(bool blocking);
    void secretTryShoot(float time, const glm::vec3& cameraPos, const glm::vec3& cameraForward);

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

    struct SecretProjectile {
        glm::vec3 pos = glm::vec3(0.0f);
        glm::vec3 vel = glm::vec3(0.0f);
        float life = 0.0f;
        float splitTimer = 0.0f;
        bool canSplit = false;
        bool fromBoss = false;
        float radius = 0.25f;
        float size = 0.18f;
        glm::vec4 color = glm::vec4(1.0f);
    };

    struct SavedState {
        EmitterSettings emitter;
        GlobalParams globals;
        bool enableWind = true;
        bool showWind = false;
        float windStrength = 1.0f;
        bool tornadoMode = false;
        float tornadoStrength = 4.0f;
        float tornadoRadius = 2.5f;
        float tornadoInflow = 1.5f;
        float tornadoUpdraft = 2.0f;
        bool fuelEnabled = true;
        bool fuelBlowAway = true;
        bool fuelInfinite = true;
        float fuelMax = 20.0f;
        float fuel = 20.0f;
        float fuelBurnRate = 1.0f;
        float addFuelAmount = 5.0f;
        bool smokeEnabled = true;
        glm::vec3 fireLightColor = glm::vec3(1.0f, 0.48f, 0.18f);
        float fireLightIntensity = 2.8f;
        float fireLightRange = 3.2f;
        std::vector<SceneObject> objects;
        int selectedObjectIndex = -1;
    };

    struct SecretState {
        bool active = false;
        SavedState saved;
        int playerHp = 3;
        float lastPlayerShotTime = -1000.0f;
        float lastBossShotTime = -1000.0f;
        bool blocking = false;
        int bossIndex = -1;
        int shieldIndex = -1;
        float intensity = 0.8f;
        std::vector<SecretProjectile> projectiles;
    };

    SecretState secret_;
};
