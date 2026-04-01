#include "Scene.h"
#include "Config.h"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

Scene::Scene() {
    fluidSolver = std::make_unique<FluidSolver3D>(48, 0.0f, 0.0f, 1.0f / 30.0f);
}

Scene::~Scene() = default;

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
    flames.setSmoke(false);
    flames.spawn(500);

    EmitterSettings se = emitter; se.baseSize = 0.12f;
    GlobalParams    sg = globals; sg.buoyancy = 0.6f; sg.cooling = 0.1f;
    smokeSys.configure(se, sg);
    smokeSys.setSmoke(true);
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

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

void Scene::applyPreset(const std::string& name)
{
    if (name == "Lighter") {
        emitter.radius = 0.05f;
        emitter.baseSize = 0.06f;
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
    else if (name == "Iris Fire") {
        emitter.radius = 0.16f;
        emitter.initialSpeedMin = 0.3f;
        emitter.initialSpeedMax = 0.5f;
        emitter.baseSize = 0.08f;
        emitter.lifetimeBase = 1.0f;
        globals.wind = glm::vec3(0.0f);
        globals.turbAmp = 0.0f;
        globals.cooling = 0.5f;
        enableWind = false;
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
    fluidUpdatedThisFrame = false;
    if (!smokeEnabled) {
        if (smokeWasEnabled_) smokeSys.reset();
        smokeInstData.clear();
        smokeWasEnabled_ = false;
    } else {
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
    flames.setSmoke(false);
    flames.setTornado(enableWind && tornadoMode,
        emitter.origin, tornadoStrength,
        tornadoRadius, tornadoInflow, tornadoUpdraft);

    // --- Update scene objects, collect disturbers ---
    std::vector<Disturber> disturbers;
    disturbers.reserve(objects.size());
    int burningCount = 0;

    for (int i = 0; i < (int)objects.size(); ++i) {
        SceneObject& obj = objects[i];
        int spawnCount = obj.update(dt, intens,
            emitter.origin, emitter.radius,
            objects, i);
        if (obj.burning) ++burningCount;

        // Spawn fire/smoke particles from burning objects
        for (int k = 0; k < spawnCount; ++k) {
            flames.spawnAt(obj.pos,
                std::max(0.05f, fe.initialSpeedMax));
            if (smokeEnabled) smokeSys.spawnAt(obj.pos, 0.25f);
        }

        // Register as disturber if not fully ashed
        if (obj.disturbRadius > 0.01f && obj.disturbStrength > 0.01f && obj.ash < 1.0f) {
            Disturber d;
            d.pos = obj.pos;
            d.radius = obj.disturbRadius;
            d.strength = obj.disturbStrength * (obj.burning ? 1.4f : 1.0f);
            disturbers.push_back(d);
        }
    }

    flames.setDisturbers(disturbers);
    if (smokeEnabled) {
        smokeSys.setDisturbers(disturbers);
        smokeSys.setSmokeDensity(0.25f + 1.25f * intens + 0.12f * (float)burningCount);
    }

    // --- Update Fluid Simulation ---
    if (enableFluidSimulation && fluidSolver) {
        fluidSolver->setWind(fg.wind);
        fluidSolver->setBuoyancy(0.05f, fg.buoyancy * 5.0f);
        fluidSolver->setPressureIterations(fluidPressureIterations);
        fluidSolver->setDiffusionIterations(fluidDiffusionIterations);

        float simHz = std::clamp(fluidSimRateHz, 1.0f, 120.0f);
        float simDt = 1.0f / simHz;
        fluidAccum_ += dt;

        glm::vec3 relPos = (emitter.origin - fluidVolumePos) / fluidVolumeScale + 0.5f;
        if (relPos.y < 0.05f) relPos.y = 0.05f;
        bool emitterInVolume =
            relPos.x >= 0.0f && relPos.x <= 1.0f &&
            relPos.y >= 0.0f && relPos.y <= 1.0f &&
            relPos.z >= 0.0f && relPos.z <= 1.0f;

        int emitterGx = 0, emitterGy = 0, emitterGz = 0;
        if (emitterInVolume) {
            emitterGx = (int)(relPos.x * fluidSolver->getSize());
            emitterGy = (int)(relPos.y * fluidSolver->getSize());
            emitterGz = (int)(relPos.z * fluidSolver->getSize());
        }

        struct SourceCell { int x, y, z; float densityMul; float tempMul; };
        std::vector<SourceCell> sources;
        sources.reserve(objects.size() + 1);

        if (emitterInVolume && (!fuelEnabled || fuel > 0.0f)) {
            SourceCell s;
            int N = fluidSolver->getSize();
            int lift = std::max(1, (int)(0.06f * (float)N));
            s.x = emitterGx;
            s.y = std::clamp(emitterGy + lift, 1, N - 2);
            s.z = emitterGz;
            s.densityMul = smokeEnabled ? 50.0f * intens : 0.0f;
            s.tempMul = 100.0f * intens;
            sources.push_back(s);
        }

        for (const auto& obj : objects) {
            if (!obj.burning || obj.ash >= 1.0f) continue;
            glm::vec3 objRelPos = (obj.pos - fluidVolumePos) / fluidVolumeScale + 0.5f;
            if (objRelPos.x < 0.0f || objRelPos.x > 1.0f ||
                objRelPos.y < 0.0f || objRelPos.y > 1.0f ||
                objRelPos.z < 0.0f || objRelPos.z > 1.0f) {
                continue;
            }

            SourceCell s;
            int N = fluidSolver->getSize();
            s.x = (int)(objRelPos.x * N);
            s.y = std::clamp((int)(objRelPos.y * N) + 1, 1, N - 2);
            s.z = (int)(objRelPos.z * N);
            s.densityMul = smokeEnabled ? 50.0f : 0.0f;
            s.tempMul = 250.0f;
            sources.push_back(s);
        }

        int substeps = 0;
        while (fluidAccum_ >= simDt && substeps < 4) {
            fluidSolver->setDt(simDt);
            for (const auto& s : sources) {
                float amount = simDt;
                if (s.densityMul > 0.0f) fluidSolver->addDensity(s.x, s.y, s.z, s.densityMul * amount);
                if (s.tempMul > 0.0f) fluidSolver->addTemperature(s.x, s.y, s.z, s.tempMul * amount);
            }
            fluidSolver->step();
            fluidAccum_ -= simDt;
            fluidUpdatedThisFrame = true;
            substeps++;
        }

        // Create a velocity field sampler for particles (trilinear sample, world-space-ish scaling)
        auto velField = [this](const glm::vec3& wpos) -> glm::vec3 {
            glm::vec3 rel = (wpos - fluidVolumePos) / fluidVolumeScale + 0.5f;
            if (rel.x < 0.0f || rel.x > 1.0f || rel.y < 0.0f || rel.y > 1.0f || rel.z < 0.0f || rel.z > 1.0f) {
                return glm::vec3(0.0f);
            }

            int N = fluidSolver->getSize();
            float gx = rel.x * (float)(N - 1);
            float gy = rel.y * (float)(N - 1);
            float gz = rel.z * (float)(N - 1);

            int x0 = std::clamp((int)std::floor(gx), 0, N - 1);
            int y0 = std::clamp((int)std::floor(gy), 0, N - 1);
            int z0 = std::clamp((int)std::floor(gz), 0, N - 1);
            int x1 = std::min(x0 + 1, N - 1);
            int y1 = std::min(y0 + 1, N - 1);
            int z1 = std::min(z0 + 1, N - 1);

            float tx = gx - (float)x0;
            float ty = gy - (float)y0;
            float tz = gz - (float)z0;

            auto sample = [&](const float* field) -> float {
                auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
                float c000 = field[fluidSolver->IX(x0, y0, z0)];
                float c100 = field[fluidSolver->IX(x1, y0, z0)];
                float c010 = field[fluidSolver->IX(x0, y1, z0)];
                float c110 = field[fluidSolver->IX(x1, y1, z0)];
                float c001 = field[fluidSolver->IX(x0, y0, z1)];
                float c101 = field[fluidSolver->IX(x1, y0, z1)];
                float c011 = field[fluidSolver->IX(x0, y1, z1)];
                float c111 = field[fluidSolver->IX(x1, y1, z1)];

                float c00 = lerp(c000, c100, tx);
                float c10 = lerp(c010, c110, tx);
                float c01 = lerp(c001, c101, tx);
                float c11 = lerp(c011, c111, tx);
                float c0 = lerp(c00, c10, ty);
                float c1 = lerp(c01, c11, ty);
                return lerp(c0, c1, tz);
            };

            glm::vec3 v(sample(fluidSolver->getVx()), sample(fluidSolver->getVy()), sample(fluidSolver->getVz()));
            float maxV = 6.0f;
            float len = glm::length(v);
            if (len > maxV && len > 1e-4f) v *= (maxV / len);
            return v * fluidVolumeScale;
        };
        flames.setVelocityField(velField);
        smokeSys.setVelocityField(velField);

        auto heatField = [this](const glm::vec3& wpos) -> float {
            glm::vec3 rel = (wpos - fluidVolumePos) / fluidVolumeScale + 0.5f;
            if (rel.x < 0.0f || rel.x > 1.0f || rel.y < 0.0f || rel.y > 1.0f || rel.z < 0.0f || rel.z > 1.0f) {
                return 0.0f;
            }

            int N = fluidSolver->getSize();
            float gx = rel.x * (float)(N - 1);
            float gy = rel.y * (float)(N - 1);
            float gz = rel.z * (float)(N - 1);

            int x0 = std::clamp((int)std::floor(gx), 0, N - 1);
            int y0 = std::clamp((int)std::floor(gy), 0, N - 1);
            int z0 = std::clamp((int)std::floor(gz), 0, N - 1);
            int x1 = std::min(x0 + 1, N - 1);
            int y1 = std::min(y0 + 1, N - 1);
            int z1 = std::min(z0 + 1, N - 1);

            float tx = gx - (float)x0;
            float ty = gy - (float)y0;
            float tz = gz - (float)z0;

            auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
            const float* T = fluidSolver->getTemperature();
            float c000 = T[fluidSolver->IX(x0, y0, z0)];
            float c100 = T[fluidSolver->IX(x1, y0, z0)];
            float c010 = T[fluidSolver->IX(x0, y1, z0)];
            float c110 = T[fluidSolver->IX(x1, y1, z0)];
            float c001 = T[fluidSolver->IX(x0, y0, z1)];
            float c101 = T[fluidSolver->IX(x1, y0, z1)];
            float c011 = T[fluidSolver->IX(x0, y1, z1)];
            float c111 = T[fluidSolver->IX(x1, y1, z1)];

            float c00 = lerp(c000, c100, tx);
            float c10 = lerp(c010, c110, tx);
            float c01 = lerp(c001, c101, tx);
            float c11 = lerp(c011, c111, tx);
            float c0 = lerp(c00, c10, ty);
            float c1 = lerp(c01, c11, ty);
            float temp = lerp(c0, c1, tz);

            return std::clamp(temp / 3.0f, 0.0f, 1.0f);
        };
        smokeSys.setHeatField(heatField);
    } else {
        flames.setVelocityField(nullptr);
        smokeSys.setVelocityField(nullptr);
        smokeSys.setHeatField(nullptr);
    }

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

    // --- Update flames & build instance data ---
    flames.update(dt, time);
    flames.buildInstanceData(flameInstData, viewProj);

    // --- Smoke from flames ---
    if (smokeEnabled) {
        std::vector<glm::vec3> emitPositions;
        flames.buildSmokeEmitPositions(emitPositions, time, intens);
        float speed = 0.22f + 0.28f * intens;
        float minY = emitter.origin.y + std::max(0.12f, emitter.radius * 1.25f);
        float liftY = std::max(0.08f, emitter.radius * 0.75f) * (0.6f + 0.8f * intens);
        for (auto ep : emitPositions) {
            ep.y = std::max(ep.y + liftY, minY);
            smokeSys.spawnAt(ep, speed);
        }
    }

    // --- Configure smoke system ---
    if (smokeEnabled) {
        EmitterSettings se = emitter;
        se.baseSize = emitter.baseSize * 2.2f;

        GlobalParams sg = fg;
        sg.buoyancy = globals.buoyancy * 0.35f;
        sg.turbAmp = globals.turbAmp * 0.8f;
        sg.turbFreq = globals.turbFreq * 0.6f;
        smokeSys.configure(se, sg);
        smokeSys.setSmoke(true);
        smokeSys.setTornado(enableWind && tornadoMode,
            emitter.origin,
            tornadoStrength * 0.8f,
            tornadoRadius * 1.2f,
            tornadoInflow * 0.6f,
            tornadoUpdraft * 0.8f);
        smokeSys.update(dt, time);
        smokeSys.buildInstanceData(smokeInstData, viewProj);
    } else {
        smokeInstData.clear();
    }

    // --- Build object billboard instance data ---
    objectInstData.clear();
    objectInstData.reserve(objects.size());
    for (const auto& obj : objects) {
        InstanceAttrib inst;
        inst.pos = obj.pos;
        inst.size = obj.markerSize;
        if (obj.ash >= 1.0f)
            inst.color = glm::vec4(0.25f, 0.25f, 0.25f, 0.6f);
        else if (obj.burning)
            inst.color = glm::vec4(1.0f, 0.5f, 0.05f, 0.9f);
        else
            inst.color = glm::vec4(0.0f, 0.9f, 0.1f, 0.85f);
        objectInstData.push_back(inst);
    }
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void Scene::reset()
{
    flames.reset();
    smokeSys.reset();
    flames.setSmoke(false);
    smokeSys.setSmoke(smokeEnabled);
    fuel = fuelMax;
    flames.spawn(500);
    if (fluidSolver) fluidSolver->clear();
    fluidAccum_ = 0.0f;
    fluidUpdatedThisFrame = true;
}
