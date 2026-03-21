#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

// SceneObject is a self-contained burnable object in the world.
// It owns its own fuel state and fire-emission accumulator,
// and exposes an update() that drives burning logic each frame.
class SceneObject {
public:
    // --- Identity / Mesh ---
    std::string meshFile;

    // --- Transform ---
    glm::vec3 pos = glm::vec3(0.0f);
    float     markerSize = 0.5f;

    // --- Disturbance field (pushes nearby particles) ---
    float disturbRadius = 0.8f;
    float disturbStrength = 2.0f;

    // --- Burning state ---
    float burnability = 0.7f;
    float fuelMax = 8.0f;
    float fuel = 8.0f;
    float burnRate = 0.6f;
    bool  burning = false;
    float ash = 0.0f;          // 0 = fresh, 1 = fully ashed

    // --- Internal ---
    float fireEmitAcc = 0.0f;          // fractional particle spawn accumulator

    // Update burning simulation for one frame.
    // Returns the number of fire particles that should be spawned this frame
    // (caller is responsible for actually calling flames.spawnAt / smoke.spawnAt).
    // 'intensity'    – global fire intensity [0,1]
    // 'mainOrigin'   – position of the main emitter (for proximity ignition)
    // 'mainRadius'   – radius of the main emitter
    // 'others'       – other scene objects (for spread ignition)
    // 'dt'           – delta time
    int update(float dt,
        float intensity,
        const glm::vec3& mainOrigin,
        float mainRadius,
        const std::vector<SceneObject>& others,
        int selfIndex);

    // Convenience: has this object been completely consumed?
    bool isAsh()  const { return fuel <= 0.0f; }
    bool isAlive() const { return fuel > 0.0f || burning; }
};