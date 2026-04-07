#include "Scene.h"
#include "Config.h"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void Scene::init()
{
    emitter.origin = glm::vec3(0.0f);
    emitter.radius = 0.15f;
    emitter.initialSpeedMin = 0.1f;
    emitter.initialSpeedMax = 0.5f;
    emitter.baseSize = 0.08f;
    emitter.lifetimeBase = 1.0f;

    globals.wind = glm::vec3(0.0f);
    globals.buoyancy = 1.6f;
    globals.cooling = 0.25f;
    globals.humidity = 0.0f;
    globals.turbAmp = 0.4f;
    globals.turbFreq = 1.2f;

    flames.configure(emitter, globals);
    flames.spawn(500);

    smokeEnabled = true;
    smokeWasEnabled_ = true;
}

// ---------------------------------------------------------------------------
// Intensity
// ---------------------------------------------------------------------------

float Scene::intensity() const
{
    if (!fuelEnabled) return 1.0f;
    if (fuelMax <= 0.0f) return 0.0f;
    return std::clamp(fuel / fuelMax, 0.0f, 1.0f);
}

glm::vec3 Scene::fireLightPosition() const
{
    return emitter.origin + glm::vec3(0.0f, 0.35f + emitter.radius * 1.2f, 0.0f);
}

float Scene::fireLightStrength() const
{
    return fireLightIntensity * (0.25f + 0.75f * intensity());
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

void Scene::applyPreset(const std::string& name)
{
    if (name == "Lighter") {
        emitter.radius = 0.54f;
        emitter.baseSize = 0.2f;
        emitter.lifetimeBase = 1.6f;
        emitter.initialSpeedMin = 0.08f;
        emitter.initialSpeedMax = 0.22f;
        globals.buoyancy = 1.0f;
        globals.turbAmp = 0.25f;
        globals.turbFreq = 1.4f;
    }
    else if (name == "Campfire") {
        emitter.radius = 0.18f;
        emitter.baseSize = 0.09f;
        emitter.lifetimeBase = 3.0f;
        emitter.initialSpeedMin = 0.08f;
        emitter.initialSpeedMax = 0.25f;
        globals.buoyancy = 0.9f;
        globals.turbAmp = 0.6f;
        globals.turbFreq = 1.0f;
    }
    else if (name == "Wildfire") {
        emitter.radius = 0.45f;
        emitter.baseSize = 0.11f;
        emitter.lifetimeBase = 3.5f;
        emitter.initialSpeedMin = 0.15f;
        emitter.initialSpeedMax = 0.45f;
        globals.buoyancy = 1.4f;
        globals.turbAmp = 1.0f;
        globals.turbFreq = 1.3f;
    }
}

// ---------------------------------------------------------------------------
// Helpers: fueled emitter/globals
// ---------------------------------------------------------------------------

EmitterSettings Scene::makeFueledEmitter(float intens) const
{
    EmitterSettings fe = emitter;
    fe.radius *= (0.6f + 0.6f * intens);
    fe.baseSize *= (0.6f + 0.7f * intens);
    fe.initialSpeedMin *= (0.45f + 0.75f * intens);
    fe.initialSpeedMax *= (0.45f + 0.75f * intens);
    fe.lifetimeBase *= (0.7f + 0.6f * intens);
    return fe;
}

GlobalParams Scene::makeFueledGlobals(float intens) const
{
    // Apply wind vector
    glm::vec3 windDir = globals.wind;
    float windLen = glm::length(windDir);
    if (windLen > 1e-4f) windDir /= windLen;
    glm::vec3 windVec = enableWind ? windDir * windStrength : glm::vec3(0.0f);

    GlobalParams fg = globals;
    fg.wind = windVec;
    fg.buoyancy *= (0.25f + 0.75f * intens);
    if (fuelEnabled && intens <= 0.0f && fuelBlowAway) {
        fg.buoyancy *= 0.1f;
        fg.wind *= 1.4f;
    }
    return fg;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void Scene::update(float dt, float time, const glm::mat4& viewProj)
{
    // --- Handle smoke toggle ---
    if (!smokeEnabled) {
        if (smokeWasEnabled_) smokeSys.reset();
        smokeInstData.clear();
        smokeWasEnabled_ = false;
    }
    else {
        smokeWasEnabled_ = true;
    }

    // --- Consume global fuel ---
    if (fuelEnabled) {
        if (fuelInfinite) {
            fuel = fuelMax;
        }
        else if (fuel > 0.0f) {
            fuel = std::max(0.0f, fuel - dt * fuelBurnRate);
        }
    }

    float intens = intensity();
    EmitterSettings fe = makeFueledEmitter(intens);
    GlobalParams    fg = makeFueledGlobals(intens);

    // --- Configure flame particle system ---
    flames.configure(fe, fg);
    flames.setTornado(enableWind && tornadoMode,
        emitter.origin, tornadoStrength,
        tornadoRadius, tornadoInflow, tornadoUpdraft);

    // --- Update scene objects, collect disturbers ---
    std::vector<Disturber> disturbers;
    disturbers.reserve(objects.size());
    for (int i = 0; i < (int)objects.size(); ++i) {
        SceneObject& obj = objects[i];
        int spawnCount = obj.update(dt, intens,
            emitter.origin, emitter.radius,
            objects, i);

        // Spawn fire particles from burning objects (smoke is now integrated)
        for (int k = 0; k < spawnCount; ++k) {
            float angle = (float)k / spawnCount * 6.28f + time;
            float r = obj.markerSize * 0.3f * (0.5f + (float)k * 0.1f);
            glm::vec3 spawnPos = obj.pos + glm::vec3(
                std::cos(angle) * r,
                0.0f,
                std::sin(angle) * r
            );
            flames.spawnAt(spawnPos, std::max(0.05f, fe.initialSpeedMax * (1.0f - obj.ash)));
        }

        if (obj.burning && obj.disturbRadius > 0.01f && obj.ash < 1.0f) {
            Disturber d;
            d.pos = obj.pos;
            d.radius = obj.disturbRadius * (1.0f - obj.ash * 0.5f);
            d.strength = obj.disturbStrength * (1.0f - obj.ash) * (obj.fuel / obj.fuelMax);
            disturbers.push_back(d);
        }
    }

    flames.setDisturbers(disturbers);

    // --- Spawn main flame particles ---
    static float flameSpawnAcc = 0.0f;
    const float  spawnRateBase = 250.0f;
    const int    maxParticlesBase = 1000;

    float spawnRate = spawnRateBase * (0.15f + 0.85f * intens);
    int   maxParticles = (int)(maxParticlesBase * (0.2f + 0.8f * intens));
    if (fuelEnabled && intens <= 0.0f) { spawnRate = 0.0f; maxParticles = 0; }

    flameSpawnAcc += dt * spawnRate;
    int newFlames = (int)flameSpawnAcc;
    if (newFlames > 0) {
        flameSpawnAcc -= (float)newFlames;
        int canSpawn = std::max(0, maxParticles - flames.count());
        newFlames = std::min(newFlames, canSpawn);
        if (newFlames > 0) flames.spawn(newFlames);
    }

    // --- Update flames & build split instance data ---
    flames.update(dt, time);
    flames.buildFireInstanceData(flameInstData, viewProj);

    // Smoke integrated into fire system, only build if enabled
    if (smokeEnabled) {
        flames.buildSmokeInstanceData(smokeInstData, viewProj);
    }
    else {
        smokeInstData.clear();
    }
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void Scene::reset()
{
    flames.reset();
    // smokeSys.reset();        <-- remove, smokeSys unused
    // flames.setSmoke(false);  <-- remove
    // smokeSys.setSmoke(...);  <-- remove
    fuel = fuelMax;
    flames.spawn(500);
}
