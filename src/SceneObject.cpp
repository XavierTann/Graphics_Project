#include "SceneObject.h"
#include <algorithm>
#include <cmath>

int SceneObject::update(float dt,
    float intensity,
    const glm::vec3& mainOrigin,
    float mainRadius,
    const std::vector<SceneObject>& others,
    int selfIndex)
{
    // Clamp fuel
    if (fuelMax < 0.1f) fuelMax = 0.1f;
    if (fuel > fuelMax) fuel = fuelMax;

    // Transition to ash when fuel is gone
    if (fuel <= 0.0f) {
        burning = false;
        ash = 1.0f;
        return 0;
    }

    // --- Ignition check ---
    if (!burning && fuel > 0.0f && burnability > 0.0f) {
        // Proximity to the main emitter
        float d0 = glm::length(pos - mainOrigin);
        float igniteRange = (0.8f + intensity * 1.8f) + mainRadius * 2.0f;
        if (intensity > 0.2f && d0 < igniteRange) {
            burning = true;
        }

        // Spread from other burning objects
        if (!burning) {
            for (int j = 0; j < (int)others.size(); ++j) {
                if (j == selfIndex) continue;
                if (!others[j].burning) continue;
                float spreadDist = 0.6f + others[j].burnability * 0.8f;
                if (glm::length(pos - others[j].pos) < spreadDist) {
                    burning = true;
                    break;
                }
            }
        }
    }

    if (!burning) return 0;

    // --- Burn fuel ---
    float burnMul = (0.4f + intensity) * (0.2f + burnability);
    fuel = std::max(0.0f, fuel - dt * burnRate * burnMul);
    ash = std::clamp(1.0f - (fuel / fuelMax), 0.0f, 1.0f);

    // --- Accumulate particle spawn ---
    float emitRate = (15.0f + 35.0f * burnability) * (0.25f + intensity);
    fireEmitAcc += dt * emitRate;
    int spawnNow = (int)fireEmitAcc;
    if (spawnNow > 0) {
        fireEmitAcc -= (float)spawnNow;
        spawnNow = std::min(spawnNow, 12);
    }
    return spawnNow;
}