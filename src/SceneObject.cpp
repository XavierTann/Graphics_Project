#include "SceneObject.h"
#include <algorithm>
#include <cmath>

float SceneObject::burnFront(float intensity) const
{
    if (!ignitionSet) return burning ? 0.0f : 1.0f;
    float base = burning ? fadeProgress : 1.0f;
    float tweak = 0.35f + 0.65f * std::clamp(intensity, 0.0f, 1.0f);
    return std::clamp(base * tweak, 0.0f, 1.0f);
}

int SceneObject::update(float dt,
    float intensity,
    const glm::vec3& mainOrigin,
    float mainRadius,
    const std::vector<SceneObject>& others,
    int selfIndex)
{
    if (alpha < 0.001f) return 0;

    // Clamp fuel
    if (fuelMax < 0.1f) fuelMax = 0.1f;
    if (fuel > fuelMax) fuel = fuelMax;

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

    if (burning) {
        if (!ignitionSet) {
            ignitionSet = true;
            ignitionLocal = glm::vec3(0.0f);
            ignitionLocal.z = 0.0f;
            float len = glm::length(ignitionLocal);
            if (len > 0.85f) ignitionLocal *= (0.85f / len);
            burnTime = 0.0f;
            fadeProgress = 0.0f;
        }
        burnTime += dt;
        float spreadSpeed = 0.65f + 1.45f * std::clamp(intensity, 0.0f, 1.0f);
        float target = std::clamp(burnTime * spreadSpeed, 0.0f, 1.0f);
        if (target > fadeProgress) fadeProgress = target;
    }

    if (burning && fuel > 0.0f) {
        float burnMul = (0.4f + intensity) * (0.2f + burnability);
        fuel = std::max(0.0f, fuel - dt * burnRate * burnMul);
        ash = std::clamp(1.0f - (fuel / fuelMax), 0.0f, 1.0f);
    }
    if (burning && fuel <= 0.0f) {
        burning = false;
        ash = 1.0f;
        alpha = 0.0f;
        return 0;
    }
    if (!burning) {
        alpha = 1.0f;
    }
    else {
        if (ash > fadeProgress) fadeProgress = ash;
        float spread = std::clamp(fadeProgress, 0.0f, 1.0f);
        float fade = std::clamp((ash - 0.65f) / 0.35f, 0.0f, 1.0f);
        fade = fade * fade * (3.0f - 2.0f * fade);
        alpha = std::clamp(1.0f - fade * spread, 0.0f, 1.0f);
    }

    // --- Accumulate particle spawn ---
    if (!burning) return 0;
    float sizeFactor = markerSize / 0.5f;
    if (sizeFactor < 0.25f) sizeFactor = 0.25f;
    float areaFactor = sizeFactor * sizeFactor;
    areaFactor = std::clamp(areaFactor, 0.5f, 10.0f);

    float emitRate = (15.0f + 35.0f * burnability) * (12.0f + intensity) * areaFactor;
    fireEmitAcc += dt * emitRate;
    int spawnNow = (int)fireEmitAcc;
    if (spawnNow > 0) {
        fireEmitAcc -= (float)spawnNow;
        int maxPerFrame = (int)(12.0f * areaFactor);
        maxPerFrame = std::clamp(maxPerFrame, 12, 120);
        spawnNow = std::min(spawnNow, maxPerFrame);
    }
    return spawnNow;
}
