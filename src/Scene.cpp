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

static glm::vec3 rotateAroundAxis(const glm::vec3& v, const glm::vec3& axis, float angleRad)
{
    glm::vec3 a = glm::normalize(axis);
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    return v * c + glm::cross(a, v) * s + a * glm::dot(a, v) * (1.0f - c);
}

static float autoScaleForMesh(MeshLoader* loader, const std::string& meshFile, float desiredMaxExtent, float fallback)
{
    if (!loader) return fallback;
    MeshLoader::MeshSettings tuning = loader->settingsFor(meshFile);
    const GpuMesh* m = loader->get(meshFile);
    if (!m || !m->boundsValid) return fallback;
    if (tuning.fixedScale > 0.0f) return tuning.fixedScale;
    glm::vec3 ext = m->aabbMax - m->aabbMin;
    float maxExt = std::max(ext.x, std::max(ext.y, ext.z));
    if (maxExt <= 1e-6f) return fallback;
    float s = std::clamp(tuning.desiredMaxExtent / maxExt, 0.02f, 10.0f);
    s *= tuning.scaleMultiplier;
    s *= std::clamp(desiredMaxExtent / tuning.desiredMaxExtent, 0.05f, 20.0f);
    return s;
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
}

void Scene::enterSecretMode()
{
    if (secret_.active) return;
    secret_.active = true;
    secret_.intensity = 1.0f;

    secret_.saved.emitter = emitter;
    secret_.saved.globals = globals;
    secret_.saved.enableWind = enableWind;
    secret_.saved.showWind = showWind;
    secret_.saved.windStrength = windStrength;
    secret_.saved.tornadoMode = tornadoMode;
    secret_.saved.tornadoStrength = tornadoStrength;
    secret_.saved.tornadoRadius = tornadoRadius;
    secret_.saved.tornadoInflow = tornadoInflow;
    secret_.saved.tornadoUpdraft = tornadoUpdraft;
    secret_.saved.fuelEnabled = fuelEnabled;
    secret_.saved.fuelBlowAway = fuelBlowAway;
    secret_.saved.fuelInfinite = fuelInfinite;
    secret_.saved.fuelMax = fuelMax;
    secret_.saved.fuel = fuel;
    secret_.saved.fuelBurnRate = fuelBurnRate;
    secret_.saved.addFuelAmount = addFuelAmount;
    secret_.saved.smokeEnabled = smokeEnabled;
    secret_.saved.fireLightColor = fireLightColor;
    secret_.saved.fireLightIntensity = fireLightIntensity;
    secret_.saved.fireLightRange = fireLightRange;
    secret_.saved.objects = objects;
    secret_.saved.selectedObjectIndex = selectedObjectIndex;

    selectedObjectIndex = -1;
    objects.clear();
    flames.reset();

    enableWind = false;
    showWind = false;
    windStrength = 0.0f;
    tornadoMode = false;
    smokeEnabled = true;
    globals.wind = glm::vec3(0.0f);
    globals.buoyancy = 1.1f;
    globals.cooling = 0.22f;
    globals.humidity = 0.0f;
    globals.turbAmp = 0.55f;
    globals.turbFreq = 1.35f;

    emitter.origin = glm::vec3(0.0f);
    emitter.radius = 0.08f;
    emitter.initialSpeedMin = 0.35f;
    emitter.initialSpeedMax = 1.1f;
    emitter.baseSize = 0.06f;
    emitter.lifetimeBase = 1.35f;

    fuelEnabled = false;
    fuelInfinite = true;
    fuel = fuelMax;

    fireLightColor = glm::vec3(0.75f, 0.35f, 1.0f);
    fireLightIntensity = 2.6f;
    fireLightRange = 4.0f;

    secret_.playerHp = 3;
    secret_.blocking = false;
    secret_.bossIndex = -1;
    secret_.shieldIndex = -1;
    secret_.lastPlayerShotTime = -1000.0f;
    secret_.lastBossShotTime = -1000.0f;
    secret_.projectiles.clear();

    SceneObject boss;
    boss.meshFile = "netherwing_pollux.glb";
    boss.pos = glm::vec3(0.0f, 2.3f, 0.0f);
    boss.markerSize = autoScaleForMesh(meshLoader, boss.meshFile, 2.4f, 0.6f);
    boss.burnability = 1.0f;
    boss.fuelMax = 20.0f;
    boss.fuel = boss.fuelMax;
    boss.burnRate = 0.65f;
    boss.disturbRadius = 2.5f;
    boss.disturbStrength = 3.5f;
    objects.push_back(boss);
    secret_.bossIndex = 0;
}

void Scene::exitSecretMode()
{
    if (!secret_.active) return;

    emitter = secret_.saved.emitter;
    globals = secret_.saved.globals;
    enableWind = secret_.saved.enableWind;
    showWind = secret_.saved.showWind;
    windStrength = secret_.saved.windStrength;
    tornadoMode = secret_.saved.tornadoMode;
    tornadoStrength = secret_.saved.tornadoStrength;
    tornadoRadius = secret_.saved.tornadoRadius;
    tornadoInflow = secret_.saved.tornadoInflow;
    tornadoUpdraft = secret_.saved.tornadoUpdraft;
    fuelEnabled = secret_.saved.fuelEnabled;
    fuelBlowAway = secret_.saved.fuelBlowAway;
    fuelInfinite = secret_.saved.fuelInfinite;
    fuelMax = secret_.saved.fuelMax;
    fuel = secret_.saved.fuel;
    fuelBurnRate = secret_.saved.fuelBurnRate;
    addFuelAmount = secret_.saved.addFuelAmount;
    smokeEnabled = secret_.saved.smokeEnabled;
    fireLightColor = secret_.saved.fireLightColor;
    fireLightIntensity = secret_.saved.fireLightIntensity;
    fireLightRange = secret_.saved.fireLightRange;
    objects = secret_.saved.objects;
    selectedObjectIndex = secret_.saved.selectedObjectIndex;

    secret_.active = false;
    secret_.projectiles.clear();
    secret_.bossIndex = -1;
    secret_.shieldIndex = -1;
    secret_.blocking = false;

    flames.reset();
    flames.configure(emitter, globals);
    flames.spawn(500);
}

void Scene::secretSetBlocking(bool blocking)
{
    if (!secret_.active) return;
    secret_.blocking = blocking;
}

void Scene::secretTryShoot(float time, const glm::vec3& cameraPos, const glm::vec3& cameraForward)
{
    if (!secret_.active) return;
    const float cooldown = 0.22f;
    if (time - secret_.lastPlayerShotTime < cooldown) return;
    secret_.lastPlayerShotTime = time;

    SecretProjectile p;
    p.fromBoss = false;
    p.pos = cameraPos + cameraForward * 0.55f;
    p.vel = glm::normalize(cameraForward) * 6.2f;
    p.life = 0.55f;
    p.canSplit = false;
    p.splitTimer = 0.0f;
    p.radius = 0.28f;
    p.size = 0.22f;
    p.color = glm::vec4(1.0f, 0.05f, 0.02f, 1.0f);
    secret_.projectiles.push_back(p);
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
    return emitter.origin + glm::vec3(0.0f, 0.0f, 0.35f + emitter.radius * 1.2f);
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
void Scene::update(float dt, float time, const glm::mat4& viewProj, const glm::vec3& cameraPos, const glm::vec3& cameraForward)
{
    if (!secret_.active) {
        if (fuelEnabled) {
            if (fuelInfinite) {
                fuel = fuelMax;
            }
            else if (fuel > 0.0f) {
                fuel = std::max(0.0f, fuel - dt * fuelBurnRate);
            }
        }
    }

    float intens = secret_.active ? secret_.intensity : intensity();
    EmitterSettings fe = makeFueledEmitter(intens);
    GlobalParams    fg = makeFueledGlobals(intens);

    // --- Configure flame particle system ---
    flames.configure(fe, fg);
    flames.setTornado(enableWind && tornadoMode,
        emitter.origin, tornadoStrength,
        tornadoRadius, tornadoInflow, tornadoUpdraft);

    bool endSecret = false;

    if (secret_.active) {
        if (secret_.bossIndex >= 0 && secret_.bossIndex < (int)objects.size()) {
            emitter.origin = objects[secret_.bossIndex].pos + glm::vec3(0.0f, 0.0f, 1.2f);
        }

        if (secret_.blocking) {
            if (secret_.shieldIndex < 0) {
                SceneObject shield;
                shield.meshFile = "HSR - Aventurine.glb";
                shield.markerSize = autoScaleForMesh(meshLoader, shield.meshFile, 0.9f, 0.25f);
                shield.burnability = 0.0f;
                shield.fuelMax = 1.0f;
                shield.fuel = 1.0f;
                shield.burnRate = 0.0f;
                shield.disturbRadius = 0.0f;
                shield.disturbStrength = 0.0f;
                objects.push_back(shield);
                secret_.shieldIndex = (int)objects.size() - 1;
            }
            if (secret_.shieldIndex >= 0 && secret_.shieldIndex < (int)objects.size()) {
                glm::vec3 fwd = glm::normalize(cameraForward);
                objects[secret_.shieldIndex].pos = cameraPos + fwd * 0.9f + glm::vec3(0.0f, 0.0f, -0.1f);
            }
        }
        else if (secret_.shieldIndex >= 0) {
            if (secret_.shieldIndex >= 0 && secret_.shieldIndex < (int)objects.size()) {
                objects.erase(objects.begin() + secret_.shieldIndex);
            }
            secret_.shieldIndex = -1;
        }

        if (secret_.bossIndex < 0 || secret_.bossIndex >= (int)objects.size()) {
            endSecret = true;
        }

        if (!endSecret) {
            glm::vec3 bossPos = objects[secret_.bossIndex].pos + glm::vec3(0.0f, 0.0f, 1.15f);
            glm::vec3 toPlayer = cameraPos - bossPos;
            float dist = glm::length(toPlayer);
            glm::vec3 aim = dist > 1e-4f ? (toPlayer / dist) : glm::vec3(0.0f, -1.0f, 0.0f);

            const float bossCooldown = 1.05f;
            if (time - secret_.lastBossShotTime > bossCooldown) {
                secret_.lastBossShotTime = time;
                SecretProjectile p;
                p.fromBoss = true;
                p.pos = bossPos;
                p.vel = aim * 3.0f;
                p.life = 2.6f;
                p.canSplit = true;
                p.splitTimer = 0.35f;
                p.radius = 0.32f;
                p.size = 0.26f;
                p.color = glm::vec4(0.62f, 0.22f, 1.0f, 1.0f);
                secret_.projectiles.push_back(p);
            }

            glm::vec3 playerPos = cameraPos;
            float playerRadius = 0.35f;

            glm::vec3 shieldPos = glm::vec3(0.0f);
            float shieldRadius = 0.85f;
            bool shieldActive = secret_.blocking && secret_.shieldIndex >= 0 && secret_.shieldIndex < (int)objects.size();
            if (shieldActive) shieldPos = objects[secret_.shieldIndex].pos;

            float bossHitRadius = std::max(0.6f, objects[secret_.bossIndex].markerSize * 0.65f);

            std::vector<SecretProjectile> spawned;
            for (auto& pr : secret_.projectiles) {
                pr.life -= dt;
                if (pr.life <= 0.0f) continue;
                pr.pos += pr.vel * dt;

                if (pr.fromBoss && pr.canSplit) {
                    pr.splitTimer -= dt;
                    if (pr.splitTimer <= 0.0f) {
                        pr.canSplit = false;
                        glm::vec3 dir = glm::normalize(pr.vel);
                        glm::vec3 up(0.0f, 0.0f, 1.0f);
                        float angle = 0.32f;

                        SecretProjectile a = pr;
                        SecretProjectile b = pr;
                        a.vel = rotateAroundAxis(dir, up, angle) * glm::length(pr.vel);
                        b.vel = rotateAroundAxis(dir, up, -angle) * glm::length(pr.vel);
                        a.life = std::min(a.life, 1.6f);
                        b.life = std::min(b.life, 1.6f);
                        a.radius *= 0.9f;
                        b.radius *= 0.9f;
                        a.size *= 0.9f;
                        b.size *= 0.9f;
                        spawned.push_back(a);
                        spawned.push_back(b);
                    }
                }

                if (pr.fromBoss) {
                    if (shieldActive && glm::length(pr.pos - shieldPos) < (pr.radius + shieldRadius)) {
                        SceneObject& shield = objects[secret_.shieldIndex];
                        shield.burnability = 1.0f;
                        shield.burning = true;
                        shield.fuelMax = std::max(shield.fuelMax, 3.0f);
                        shield.fuel = std::min(shield.fuelMax, std::max(shield.fuel, 1.25f));
                        shield.burnRate = std::max(shield.burnRate, 1.8f);
                        shield.fuel = std::max(0.0f, shield.fuel - 0.55f);
                        pr.life = 0.0f;
                        continue;
                    }
                    if (glm::length(pr.pos - playerPos) < (pr.radius + playerRadius)) {
                        pr.life = 0.0f;
                        secret_.playerHp -= 1;
                        if (secret_.playerHp <= 0) {
                            endSecret = true;
                            break;
                        }
                    }
                }
                else {
                    if (glm::length(pr.pos - objects[secret_.bossIndex].pos) < (pr.radius + bossHitRadius)) {
                        pr.life = 0.0f;
                        SceneObject& boss = objects[secret_.bossIndex];
                        boss.burning = true;
                        boss.burnability = 1.0f;
                        boss.burnRate = std::min(1.4f, boss.burnRate + 0.02f);
                        boss.fuel = std::max(0.0f, boss.fuel - 1.1f);
                    }
                }
            }

            secret_.projectiles.erase(
                std::remove_if(secret_.projectiles.begin(), secret_.projectiles.end(),
                    [](const SecretProjectile& p) { return p.life <= 0.0f; }),
                secret_.projectiles.end());
            if (!spawned.empty()) {
                secret_.projectiles.insert(secret_.projectiles.end(), spawned.begin(), spawned.end());
            }
        }
    }

    // --- Update scene objects, collect disturbers ---
    std::vector<Disturber> disturbers;
    disturbers.reserve(objects.size());
    for (int i = 0; i < (int)objects.size(); ) {
        SceneObject& obj = objects[i];
        if (!obj.boundsReady && meshLoader) {
            MeshLoader::MeshSettings tuning = meshLoader->settingsFor(obj.meshFile);
            const GpuMesh* m = meshLoader->get(obj.meshFile);
            if (m && m->boundsValid) {
                bool authoredZUp = m->authoredZUp;
                if (tuning.upMode == 1) authoredZUp = true;
                if (tuning.upMode == 0) authoredZUp = false;
                float minUp = authoredZUp ? m->aabbMin.z : m->aabbMin.y;
                obj.minLocalZ = minUp;
            }
            obj.boundsReady = true;
        }

        float minAllowedZ = obj.minAllowedZ();
        if (obj.pos.z < minAllowedZ) obj.pos.z = minAllowedZ;
        glm::vec3 ignOrigin = secret_.active ? glm::vec3(10000.0f) : emitter.origin;
        float ignRadius = secret_.active ? 0.0f : emitter.radius;
        int spawnCount = obj.update(dt, intens,
            ignOrigin, ignRadius,
            objects, i);

        // Spawn fire particles from burning objects (smoke is now integrated)
        for (int k = 0; k < spawnCount; ++k) {
            glm::vec3 spawnPos = obj.pos;
            const GpuMesh* m = meshLoader ? meshLoader->get(obj.meshFile) : nullptr;
            glm::vec3 local;
            float seed = time * 13.1f + (float)i * 97.3f + (float)k * 41.7f;
            if (sampleSurfacePoint(m, seed, local)) {
                bool authoredZUp = m ? m->authoredZUp : false;
                if (meshLoader) {
                    MeshLoader::MeshSettings tuning = meshLoader->settingsFor(obj.meshFile);
                    if (tuning.upMode == 1) authoredZUp = true;
                    if (tuning.upMode == 0) authoredZUp = false;
                }
                glm::vec3 localR = authoredZUp ? local : glm::vec3(local.x, -local.z, local.y);
                float front = obj.burnFront(intens);
                glm::vec3 fromIgnition = localR - obj.ignitionLocal;
                float dist = glm::length(fromIgnition);
                float allow = std::clamp(front * 1.35f, 0.0f, 1.0f);
                if (dist > allow) continue;

                spawnPos = obj.pos + localR * obj.markerSize;
                glm::vec3 jitter(
                    rand01(seed + 9.1f) - 0.5f,
                    rand01(seed + 10.2f) - 0.5f,
                    rand01(seed + 11.3f) - 0.5f);
                float jitterScale = 0.06f * (obj.markerSize > 0.05f ? obj.markerSize : 0.05f);
                spawnPos += jitter * jitterScale;
                if (spawnPos.z < 0.0f) spawnPos.z = 0.0f;
            }
            else {
                int denom = (spawnCount > 0) ? spawnCount : 1;
                float angle = (float)k / (float)denom * 6.28f + time;
                float r = obj.markerSize * 0.3f * (0.5f + (float)k * 0.1f);
                spawnPos = obj.pos + glm::vec3(
                    std::cos(angle) * r,
                    std::sin(angle) * r,
                    0.0f
                );
                if (spawnPos.z < 0.0f) spawnPos.z = 0.0f;
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

        if (obj.isDead()) {
            if (secret_.active && i == secret_.bossIndex) endSecret = true;
            if (secret_.active) {
                if (i == secret_.shieldIndex) {
                    secret_.shieldIndex = -1;
                    secret_.blocking = false;
                }
                else if (secret_.shieldIndex > i) {
                    secret_.shieldIndex--;
                }

                if (secret_.bossIndex > i) {
                    secret_.bossIndex--;
                }
            }
            objects.erase(objects.begin() + i);
            if (selectedObjectIndex == i) selectedObjectIndex = -1;
            else if (selectedObjectIndex > i) selectedObjectIndex--;
            continue;
        }
        ++i;
    }

    flames.setDisturbers(disturbers);

    if (!secret_.active) {
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
    }

    // --- Update flames & build split instance data ---
    flames.update(dt, time);
    flames.buildFireInstanceData(flameInstData, viewProj);

    if (secret_.active) {
        secretBossFlameInstData.clear();
        secretPlayerFlameInstData.clear();
        for (const auto& pr : secret_.projectiles) {
            auto& out = pr.fromBoss ? secretBossFlameInstData : secretPlayerFlameInstData;
            out.push_back({ pr.pos, pr.size, pr.color });

            int burst = pr.fromBoss ? 26 : 30;
            float seedBase = pr.pos.x * 17.1f + pr.pos.y * 31.7f + pr.pos.z * 59.3f + pr.life * 23.9f + time * 7.7f;
            float rad = pr.fromBoss ? 0.22f : 0.18f;
            for (int i = 0; i < burst; ++i) {
                float s = seedBase + (float)i * 9.13f;
                glm::vec3 j(
                    rand01(s + 0.1f) - 0.5f,
                    rand01(s + 1.2f) - 0.5f,
                    rand01(s + 2.3f) - 0.5f);
                glm::vec3 ppos = pr.pos + j * rad;
                float sz = pr.size * (0.35f + 0.85f * rand01(s + 3.4f));
                glm::vec4 col = pr.color;
                col.a = 0.85f;
                out.push_back({ ppos, sz, col });
            }

            glm::vec3 dir = glm::length(pr.vel) > 1e-4f ? glm::normalize(pr.vel) : glm::vec3(0.0f, 1.0f, 0.0f);
            int trail = pr.fromBoss ? 6 : 7;
            for (int k = 1; k <= trail; ++k) {
                float s = seedBase + 200.0f + (float)k * 13.7f;
                float t = (float)k / (float)trail;
                glm::vec3 jitter(
                    rand01(s + 0.7f) - 0.5f,
                    rand01(s + 1.8f) - 0.5f,
                    rand01(s + 2.9f) - 0.5f);
                glm::vec3 ppos = pr.pos - dir * (0.12f + 0.10f * t) * (float)k + jitter * rad * 0.35f;
                float sz = pr.size * (0.60f - 0.45f * t);
                glm::vec4 col = pr.color;
                col.a = 0.75f - 0.55f * t;
                out.push_back({ ppos, sz, col });
            }
        }
    }

    // Smoke integrated into fire system, only build if enabled
    if (smokeEnabled) {
        flames.buildSmokeInstanceData(smokeInstData, viewProj);
    }
    else {
        smokeInstData.clear();
    }

    if (secret_.active && endSecret) {
        exitSecretMode();
    }
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void Scene::reset()
{
    flames.reset();
    fuel = fuelMax;
    flames.spawn(500);
}
