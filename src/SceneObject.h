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
    float     minLocalZ = 0.0f;
    bool      boundsReady = false;

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
    float alpha = 1.0f;

    // --- Internal ---
    float fireEmitAcc = 0.0f;
    float burnTime = 0.0f;
    float fadeProgress = 0.0f;
    bool  ignitionSet = false;
    glm::vec3 ignitionLocal = glm::vec3(0.0f);

    bool isDead() const { return alpha <= 0.001f; }
    float minAllowedZ() const
    {
        float z = boundsReady ? (-minLocalZ * markerSize) : 0.0f;
        return z > 0.0f ? z : 0.0f;
    }
    float burnFront(float intensity) const;



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
