#include "Scene.h"
#include "MeshLoader.h"
#include <algorithm>
#include <cmath>

static float rand01(float s)
{
    float x = std::sin(s * 12.9898f) * 43758.5453f;
    return x - std::floor(x);
}

static bool sampleSurfacePoint(const GpuMesh* mesh, float seed, glm::vec3& out)
{
    if (!mesh) return false;
    if (mesh->cpuPositions.size() < 3) return false;

    unsigned int ia = 0, ib = 1, ic = 2;

    if (!mesh->cpuIndices.empty() && mesh->cpuIndices.size() >= 3 && mesh->triAreaSum > 1e-8f
        && mesh->triCdf.size() == (mesh->cpuIndices.size() / 3))
    {
        float r = rand01(seed) * mesh->triAreaSum;
        auto it = std::lower_bound(mesh->triCdf.begin(), mesh->triCdf.end(), r);
        size_t tri = (it == mesh->triCdf.end()) ? (mesh->triCdf.size() - 1) : (size_t)(it - mesh->triCdf.begin());
        ia = mesh->cpuIndices[tri * 3 + 0];
        ib = mesh->cpuIndices[tri * 3 + 1];
        ic = mesh->cpuIndices[tri * 3 + 2];
        if (ia >= mesh->cpuPositions.size() || ib >= mesh->cpuPositions.size() || ic >= mesh->cpuPositions.size())
            return false;
    }
    else if ((mesh->cpuPositions.size() % 3) == 0) {
        size_t triCount = mesh->cpuPositions.size() / 3;
        size_t tri = (size_t)(rand01(seed) * (float)triCount);
        if (tri >= triCount) tri = triCount - 1;
        ia = (unsigned int)(tri * 3 + 0);
        ib = (unsigned int)(tri * 3 + 1);
        ic = (unsigned int)(tri * 3 + 2);
    }
    else {
        size_t n = mesh->cpuPositions.size();
        size_t a = (size_t)(rand01(seed + 1.1f) * (float)n);
        size_t b = (size_t)(rand01(seed + 2.2f) * (float)n);
        size_t c = (size_t)(rand01(seed + 3.3f) * (float)n);
        if (a >= n) a = n - 1;
        if (b >= n) b = n - 1;
        if (c >= n) c = n - 1;
        ia = (unsigned int)a;
        ib = (unsigned int)b;
        ic = (unsigned int)c;
    }

    const glm::vec3& a = mesh->cpuPositions[ia];
    const glm::vec3& b = mesh->cpuPositions[ib];
    const glm::vec3& c = mesh->cpuPositions[ic];

    float r1 = rand01(seed + 4.4f);
    float r2 = rand01(seed + 5.5f);
    float sr1 = std::sqrt(r1);
    float u = 1.0f - sr1;
    float v = sr1 * (1.0f - r2);
    float w = sr1 * r2;
    out = a * u + b * v + c * w;
    return true;
}

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
            glm::vec3 spawnPos = obj.pos;
            const GpuMesh* m = meshLoader ? meshLoader->get(obj.meshFile) : nullptr;
            glm::vec3 local;
            float seed = time * 13.1f + (float)i * 97.3f + (float)k * 41.7f;
            if (sampleSurfacePoint(m, seed, local)) {
                spawnPos = obj.pos + local * obj.markerSize;
                glm::vec3 jitter(
                    rand01(seed + 9.1f) - 0.5f,
                    rand01(seed + 10.2f) - 0.5f,
                    rand01(seed + 11.3f) - 0.5f);
                float jitterScale = 0.06f * (obj.markerSize > 0.05f ? obj.markerSize : 0.05f);
                spawnPos += jitter * jitterScale;
                if (spawnPos.y < 0.0f) spawnPos.y = 0.0f;
            }
            else {
                int denom = (spawnCount > 0) ? spawnCount : 1;
                float angle = (float)k / (float)denom * 6.28f + time;
                float r = obj.markerSize * 0.3f * (0.5f + (float)k * 0.1f);
                spawnPos = obj.pos + glm::vec3(
                    std::cos(angle) * r,
                    0.0f,
                    std::sin(angle) * r
                );
            }
            float spd = fe.initialSpeedMax * (1.0f - obj.ash);
            if (spd < 0.05f) spd = 0.05f;
            flames.spawnAt(spawnPos, spd);
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
