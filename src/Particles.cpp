#include "Particles.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>


void ParticleSystem::configure(const EmitterSettings& e, const GlobalParams& g) {
    emitter = e;
    params = g;
}

void ParticleSystem::setSmoke(bool smoke) {
    smokeMode = smoke;
}

void ParticleSystem::setTornado(bool enabled, const glm::vec3& origin,
    float strength, float radius,
    float inflow, float updraft)
{
    tornadoEnabled = enabled;
    tornadoOrigin = origin;
    tornadoStrength = strength;
    tornadoRadius = std::max(0.001f, radius);
    tornadoInflow = inflow;
    tornadoUpdraft = updraft;
}

void ParticleSystem::setDisturbers(const std::vector<Disturber>& d) {
    disturbers = d;
}


void ParticleSystem::spawn(int count) {
    particles.reserve(particles.size() + count);
    for (int i = 0; i < count; ++i) {
        float s = (float)(particles.size() + i);

        Particle pr;
        pr.pos = emitter.origin;

        float vx = randf(-0.5f, 0.5f, s * 1.1f) * emitter.radius;
        float vy = randf(emitter.initialSpeedMin, emitter.initialSpeedMax, s * 2.2f);
        float vz = randf(-0.5f, 0.5f, s * 3.3f) * emitter.radius;
        pr.vel = glm::vec3(vx, vy, vz);

        float lifeVar = randf(0.0f, 0.5f, s * 4.4f);
        pr.maxLife = emitter.lifetimeBase + lifeVar;
        pr.lifetime = pr.maxLife;
        pr.color = glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);
        pr.seed = s;
        pr.size = emitter.baseSize * (0.7f + randf(0.0f, 0.6f, s * 5.5f));
        pr.isSpark = false;

        // Sparks
        if (randf(0.0f, 1.0f, s * 6.6f) > 0.9f) {
            pr.maxLife = emitter.lifetimeBase * 0.4f;
            pr.lifetime = pr.maxLife;
            pr.size = emitter.baseSize * 0.3f;
            pr.color = glm::vec4(1.0f, 0.85f, 0.4f, 0.7f);
            pr.isSpark = true;
            float sparkSpeed = randf(0.8f, 1.8f, s * 7.7f);
            float angle = randf(0.0f, 6.28f, s * 8.8f);
            pr.vel = glm::vec3(
                std::cos(angle) * sparkSpeed * emitter.radius * 4.0f,
                randf(0.5f, 1.5f, s * 10.0f) * sparkSpeed,
                std::sin(angle) * sparkSpeed * emitter.radius * 4.0f
            );
        }
        particles.push_back(pr);
    }
}

void ParticleSystem::spawnAt(const glm::vec3& pos, float speed) {
    float s = pos.x + pos.y + pos.z + (float)particles.size();
    Particle pr;
    pr.pos = pos;

    if (smokeMode) {
        pr.vel = glm::vec3(randf(-1.0f, 1.0f, s * 1.3f) * 0.15f,
            randf(speed * 0.2f, speed * 0.6f, s * 3.7f),
            randf(-1.0f, 1.0f, s * 2.1f) * 0.15f);
        pr.maxLife = randf(2.5f, 6.0f, s);
        pr.color = glm::vec4(0.35f, 0.35f, 0.35f, 0.25f);
    }
    else {
        pr.vel = glm::vec3(randf(-0.5f, 0.5f, s * 1.3f) * emitter.radius,
            randf(speed * 0.7f, speed * 1.2f, s * 3.7f),
            randf(-0.5f, 0.5f, s * 2.1f) * emitter.radius);
        pr.maxLife = randf(std::max(0.2f, emitter.lifetimeBase * 0.6f),
            emitter.lifetimeBase + 0.5f, s);
        pr.color = glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);
    }
    pr.lifetime = pr.maxLife;
    pr.size = emitter.baseSize;
    pr.seed = s;
    pr.isSpark = false;
    particles.push_back(pr);
}


float ParticleSystem::randf(float a, float b, float s) const {
    float x = std::sin(s * 12.9898f) * 43758.5453f;
    x = x - std::floor(x);
    return a + (b - a) * x;
}

float ParticleSystem::noise(float x, float y, float z) const {
    float ptr = std::sin(x * 12.9898f + y * 78.233f + z * 151.7182f) * 43758.5453f;
    return ptr - std::floor(ptr);
}

glm::vec3 ParticleSystem::computeCurl(const glm::vec3& p) const {
    const float eps = 0.1f;
    auto potential = [&](float x, float y, float z) -> glm::vec3 {
        return glm::vec3(noise(x, y, z), noise(x + 100.f, y, z), noise(x, y, z + 100.f));
        };
    glm::vec3 dPdy = (potential(p.x, p.y + eps, p.z) - potential(p.x, p.y - eps, p.z)) / (2.f * eps);
    glm::vec3 dPdz = (potential(p.x, p.y, p.z + eps) - potential(p.x, p.y, p.z - eps)) / (2.f * eps);
    glm::vec3 dPdx = (potential(p.x + eps, p.y, p.z) - potential(p.x - eps, p.y, p.z)) / (2.f * eps);
    return glm::vec3(dPdy.z - dPdz.y, dPdz.x - dPdx.z, dPdx.y - dPdy.x);
}



void ParticleSystem::update(float dt, float time) {
    if (nsEnabled)
        stepFluid(dt);

    for (auto& p : particles) {
        p.lifetime -= dt;
        if (p.lifetime <= 0.0f) continue;

        float t = 1.0f - (p.lifetime / p.maxLife);

        if (nsEnabled) {
            glm::vec3 gridVel = nsGrid.sampleVelocity(p.pos);
            float blend = smokeMode ? 0.92f : 0.70f;
            p.vel = glm::mix(p.vel, gridVel, blend);
            float localT = nsGrid.sampleScalar(nsGrid.temperature, p.pos);
            p.vel.y += params.buoyancy * localT * dt * 0.5f;
        }
        else {
            glm::vec3 externalForces = smokeMode
                ? params.wind * 0.6f
                : params.wind;

            float buoyScale = smokeMode ? 0.15f : 0.5f;
            externalForces.y += params.buoyancy * buoyScale;

            glm::vec3 curlPos = p.pos * params.turbFreq;
            curlPos.y += smokeMode ? time * 0.2f : -time * params.turbFreq;
            externalForces += computeCurl(curlPos) *
                (smokeMode ? params.turbAmp * 0.6f : params.turbAmp);

            if (tornadoEnabled) {
                glm::vec3 rel = p.pos - tornadoOrigin;
                float r = std::sqrt(rel.x * rel.x + rel.z * rel.z);
                float falloff = std::exp(-r / tornadoRadius);
                glm::vec3 tang(0.f), inward(0.f);
                if (r > 1e-4f) {
                    tang = glm::normalize(glm::vec3(-rel.z, 0.f, rel.x));
                    inward = -glm::normalize(glm::vec3(rel.x, 0.f, rel.z));
                }
                externalForces += tang * tornadoStrength * falloff;
                externalForces += inward * tornadoInflow * falloff;
                externalForces += glm::vec3(0.f, tornadoUpdraft * falloff, 0.f);
            }

            for (const auto& dist : disturbers) {
                glm::vec3 rel = p.pos - dist.pos;
                float r = glm::length(rel);
                if (r >= dist.radius || dist.radius < 1e-4f) continue;
                float falloff = 1.0f - r / dist.radius;
                glm::vec3 horiz(rel.x, 0.f, rel.z);
                float hr = glm::length(horiz);
                if (hr < 1e-4f) continue;
                glm::vec3 push = glm::normalize(horiz);
                glm::vec3 swirl = glm::normalize(glm::vec3(-horiz.z, 0.f, horiz.x));
                externalForces += (swirl + push * 0.35f) * dist.strength * falloff;
            }

            p.vel += externalForces * dt;
            if (smokeMode) {
                p.vel *= 0.985f;
            }
            else {
                float smokePhase = std::clamp((t - 0.45f) / 0.55f, 0.0f, 1.0f);
                if (smokePhase > 0.0f) {
                    glm::vec3 smokeCurlPos = p.pos * (params.turbFreq * 0.55f + 0.15f);
                    smokeCurlPos.y += time * 0.18f + p.seed * 0.0015f;
                    glm::vec3 smokeLift = params.wind * (0.25f + smokePhase * 0.45f);
                    smokeLift.y += params.buoyancy * (0.06f + smokePhase * 0.08f);
                    smokeLift += computeCurl(smokeCurlPos) * (params.turbAmp * (0.18f + smokePhase * 0.24f));
                    p.vel += smokeLift * dt;
                    p.vel *= (1.0f - smokePhase * 0.025f);
                }
                p.vel.x *= 0.99f;
                p.vel.z *= 0.99f;
            }
        }

        p.pos += p.vel * dt;
        if (smokeMode) {
            p.size = emitter.baseSize * (0.7f + t * 2.5f);
            float a = (1.0f - t);
            p.color = glm::vec4(0.35f, 0.35f, 0.35f, a * 0.35f * smokeDensity);
        }
        else {
            float flameCurve = std::max(0.0f, 1.0f - std::pow((std::min(t, 0.62f) / 0.62f) * 2.0f - 1.0f, 2.0f));
            float smokePhase = std::clamp((t - 0.40f) / 0.60f, 0.0f, 1.0f);
            float puff = 0.80f + 0.35f * noise(p.pos.x * 2.5f + p.seed * 0.01f,
                p.pos.y * 2.0f + time * 0.2f,
                p.pos.z * 2.5f - p.seed * 0.02f);
            p.size = emitter.baseSize * (0.28f + flameCurve * 1.35f + smokePhase * (2.1f + puff * 1.6f));

            if (t < 0.20f) {
                p.color = glm::mix(glm::vec4(1.f, 0.9f, 0.2f, 1.f),
                    glm::vec4(1.f, 0.4f, 0.0f, 1.f),
                    t / 0.20f);
            }
            else if (t < 0.48f) {
                p.color = glm::mix(glm::vec4(1.f, 0.4f, 0.0f, 1.f),
                    glm::vec4(0.32f, 0.06f, 0.02f, 0.72f),
                    (t - 0.20f) / 0.28f);
            }
            else if (t < 0.72f) {
                float tt = (t - 0.48f) / 0.24f;
                glm::vec4 warmSmoke(0.26f, 0.15f, 0.11f, 0.62f);
                glm::vec4 coolSmoke(0.18f, 0.18f, 0.20f, 0.50f);
                p.color = glm::mix(warmSmoke, coolSmoke, tt);
            }
            else {
                float tt = (t - 0.72f) / 0.28f;
                glm::vec4 agedSmoke(0.16f, 0.16f, 0.17f, 0.42f);
                glm::vec4 fadeSmoke(0.10f, 0.10f, 0.11f, 0.02f);
                p.color = glm::mix(agedSmoke, fadeSmoke, tt);
            }
        }
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& pr) { return pr.lifetime <= 0.0f; }),
        particles.end());
}

static bool frustumCull(const glm::vec3& pos, const glm::mat4& vp) {
    glm::vec4 clip = vp * glm::vec4(pos, 1.0f);
    if (clip.w <= 0.0f) return true;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return std::abs(ndc.x) > 1.2f || std::abs(ndc.y) > 1.2f;
}

void ParticleSystem::buildFireInstanceData(std::vector<InstanceAttrib>& out,
    const glm::mat4& viewProj) const
{
    out.clear();
    for (const auto& p : particles) {
        float t = 1.0f - (p.lifetime / p.maxLife);
        if (!p.isSpark && t >= 0.62f) continue;
        if (frustumCull(p.pos, viewProj)) continue;
        out.push_back({ p.pos, p.size, p.color });
    }
}

void ParticleSystem::buildSmokeInstanceData(std::vector<InstanceAttrib>& out,
    const glm::mat4& viewProj) const
{
    out.clear();
    for (const auto& p : particles) {
        if (p.isSpark) continue;
        float t = 1.0f - (p.lifetime / p.maxLife);
        if (t < 0.48f) continue;
        if (frustumCull(p.pos, viewProj)) continue;
        out.push_back({ p.pos, p.size, p.color });
    }
}

void ParticleSystem::reset() {
    particles.clear();
    particles.reserve(2000);
    if (nsEnabled) nsGrid.clear();
}
