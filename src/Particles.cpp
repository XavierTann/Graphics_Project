#include "Particles.h"
#include <cmath>
#include <algorithm>
#include <cstdlib> 

void ParticleSystem::configure(const EmitterSettings& e, const GlobalParams& g) {
    emitter = e;
    params = g;
}

/*

void ParticleSystem::setSmoke(bool smoke) {
    smokeMode = smoke;
}

void ParticleSystem::setSmokeDensity(float density) {
    smokeDensity = std::max(0.0f, density);
}
*/


void ParticleSystem::setTornado(bool enabled, const glm::vec3& origin, float strength, float radius, float inflow, float updraft) {
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

        glm::vec3 p = emitter.origin;

        float vx = randf(-0.5f, 0.5f, s * 1.1f) * emitter.radius;
        float vy = randf(emitter.initialSpeedMin, emitter.initialSpeedMax, s * 2.2f);
        float vz = randf(-0.5f, 0.5f, s * 3.3f) * emitter.radius;



        Particle pr;
        pr.pos = p;
        pr.vel = glm::vec3(vx, vy, vz);


        float lifeVar = randf(0.0f, 0.5f, s * 4.4f);
        pr.maxLife = emitter.lifetimeBase + lifeVar;
        pr.lifetime = pr.maxLife;
        pr.color = glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);
        pr.seed = s;
        pr.size = emitter.baseSize * (0.7f + randf(0.0f, 0.6f, s * 5.5f));

        if (randf(0.0f, 1.0f, s * 6.6f) > 0.9f) {
            pr.maxLife = emitter.lifetimeBase * 0.4f;
            pr.lifetime = pr.maxLife;
            pr.size = emitter.baseSize * 0.3f;
            pr.color = glm::vec4(1.0f, 0.85f, 0.4f, 0.7f);

            // shoot sparks outward beyond normal fire spread
            float sparkSpeed = randf(0.8f, 1.8f, s * 7.7f);
            float angle = randf(0.0f, 6.28f, s * 8.8f); // random horizontal direction
            pr.vel = glm::vec3(
                std::cos(angle) * sparkSpeed * emitter.radius * 4.0f,  // wide horizontal spread
                randf(0.5f, 1.5f, s * 10.0f) * sparkSpeed,             // upward kick
                std::sin(angle) * sparkSpeed * emitter.radius * 4.0f   // wide horizontal spread
            );
        }




        particles.push_back(pr);
    }
}


float ParticleSystem::noise(float x, float y, float z) const {
    float ptr = std::sin(x * 12.9898f + y * 78.233f + z * 151.7182f) * 43758.5453f;
    return ptr - std::floor(ptr);
}

glm::vec3 ParticleSystem::computeCurl(const glm::vec3& p) const {
    const float eps = 0.1f;

    auto potential = [&](float x, float y, float z) -> glm::vec3 {
        return glm::vec3(
            noise(x, y, z),
            noise(x + 100.0f, y, z),
            noise(x, y, z + 100.0f)
        );
        };


    glm::vec3 p_dy_plus = potential(p.x, p.y + eps, p.z);
    glm::vec3 p_dy_minus = potential(p.x, p.y - eps, p.z);
    glm::vec3 dP_dy = (p_dy_plus - p_dy_minus) / (2.0f * eps);

    glm::vec3 p_dz_plus = potential(p.x, p.y, p.z + eps);
    glm::vec3 p_dz_minus = potential(p.x, p.y, p.z - eps);
    glm::vec3 dP_dz = (p_dz_plus - p_dz_minus) / (2.0f * eps);

    glm::vec3 p_dx_plus = potential(p.x + eps, p.y, p.z);
    glm::vec3 p_dx_minus = potential(p.x - eps, p.y, p.z);
    glm::vec3 dP_dx = (p_dx_plus - p_dx_minus) / (2.0f * eps);

    return glm::vec3(
        dP_dy.z - dP_dz.y,
        dP_dz.x - dP_dx.z,
        dP_dx.y - dP_dy.x
    );
}

void ParticleSystem::update(float dt, float time) {
    for (auto& p : particles) {

        p.lifetime -= dt;
        if (p.lifetime <= 0.0f) continue;

        float t = 1.0f - (p.lifetime / p.maxLife);

        if (smokeMode) {
            glm::vec3 externalForces = params.wind * 0.6f;
            externalForces.y += params.buoyancy * 0.15f;
            glm::vec3 curlPos = p.pos * params.turbFreq;
            curlPos.y += time * 0.2f;
            glm::vec3 curl = computeCurl(curlPos);
            externalForces += curl * (params.turbAmp * 0.6f);

            if (tornadoEnabled) {
                glm::vec3 rel = p.pos - tornadoOrigin;
                glm::vec2 r2(rel.x, rel.z);
                float r = std::sqrt(r2.x * r2.x + r2.y * r2.y);
                float falloff = std::exp(-r / tornadoRadius);
                glm::vec3 tangential(0.0f);
                if (r > 1e-4f) {
                    tangential = glm::normalize(glm::vec3(-rel.z, 0.0f, rel.x));
                }
                glm::vec3 inward(0.0f);
                if (r > 1e-4f) {
                    inward = -glm::normalize(glm::vec3(rel.x, 0.0f, rel.z));
                }
                externalForces += tangential * tornadoStrength * falloff;
                externalForces += inward * tornadoInflow * falloff;
                externalForces += glm::vec3(0.0f, tornadoUpdraft * falloff, 0.0f);
            }

            for (const auto& dist : disturbers) {
                glm::vec3 rel = p.pos - dist.pos;
                float r = glm::length(rel);
                if (r <= dist.radius && dist.radius > 1e-4f) {
                    float falloff = 1.0f - (r / dist.radius);
                    glm::vec3 horiz(rel.x, 0.0f, rel.z);
                    float hr = glm::length(horiz);
                    if (hr > 1e-4f) {
                        glm::vec3 push = glm::normalize(horiz);
                        glm::vec3 swirl = glm::normalize(glm::vec3(-horiz.z, 0.0f, horiz.x));
                        externalForces += (swirl + push * 0.35f) * dist.strength * falloff;
                    }
                }
            }

            p.vel += externalForces * dt;
            p.vel *= 0.985f;
            p.pos += p.vel * dt;

            p.size = emitter.baseSize * (0.7f + t * 2.5f);
            float a = (1.0f - t);
            p.color = glm::vec4(0.35f, 0.35f, 0.35f, a * 0.35f * smokeDensity);
            continue;
        }

        glm::vec3 externalForces = params.wind;
        externalForces.y += params.buoyancy * 0.5f;

        glm::vec3 curlPos = p.pos * params.turbFreq;
        curlPos.y -= time * params.turbFreq;

        glm::vec3 curl = computeCurl(curlPos);
        externalForces += curl * params.turbAmp;

        if (tornadoEnabled) {
            glm::vec3 rel = p.pos - tornadoOrigin;
            float r = std::sqrt(rel.x * rel.x + rel.z * rel.z);
            float falloff = std::exp(-r / tornadoRadius);
            glm::vec3 tangential(0.0f);
            if (r > 1e-4f) {
                tangential = glm::normalize(glm::vec3(-rel.z, 0.0f, rel.x));
            }
            glm::vec3 inward(0.0f);
            if (r > 1e-4f) {
                inward = -glm::normalize(glm::vec3(rel.x, 0.0f, rel.z));
            }
            externalForces += tangential * tornadoStrength * falloff;
            externalForces += inward * tornadoInflow * falloff;
            externalForces += glm::vec3(0.0f, tornadoUpdraft * falloff, 0.0f);
        }

        for (const auto& dist : disturbers) {
            glm::vec3 rel = p.pos - dist.pos;
            float r = glm::length(rel);
            if (r <= dist.radius && dist.radius > 1e-4f) {
                float falloff = 1.0f - (r / dist.radius);
                glm::vec3 horiz(rel.x, 0.0f, rel.z);
                float hr = glm::length(horiz);
                if (hr > 1e-4f) {
                    glm::vec3 push = glm::normalize(horiz);
                    glm::vec3 swirl = glm::normalize(glm::vec3(-horiz.z, 0.0f, horiz.x));
                    externalForces += (swirl + push * 0.35f) * dist.strength * falloff;
                }
            }
        }
        p.vel += externalForces * dt;
        p.vel.x *= 0.99f;
        p.vel.z *= 0.99f;

        p.pos += p.vel * dt;
        float curve = 1.0f - std::pow(2.0f * t - 1.0f, 2.0f);
        float smokeSwell = 1.0f + std::max(0.0f, (t - 0.6f) / 0.4f) * 1.2f;
        p.size = emitter.baseSize * curve * smokeSwell;

        if (t < 0.25f) {
            glm::vec4 yellow(1.0f, 0.9f, 0.2f, 1.0f);
            glm::vec4 orange(1.0f, 0.4f, 0.0f, 1.0f);
            p.color = glm::mix(yellow, orange, t / 0.33f);
        }
        else if (t < 0.70f) {
            glm::vec4 orange(1.0f, 0.4f, 0.0f, 1.0f);
            glm::vec4 deepRed(0.35f, 0.02f, 0.0f, 0.25f);
            p.color = glm::mix(orange, deepRed, (t - 0.33f) / 0.22f);
        }
        else {
            glm::vec4 deepRed(0.25f, 0.08f, 0.02f, 0.25f);
            glm::vec4 darkSmoke(0.12f, 0.12f, 0.12f, 0.0f);
            p.color = glm::mix(deepRed, darkSmoke, (t - 0.75f) / 0.25f);
        }
    }

    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle& pr) { return pr.lifetime <= 0.0f; }), particles.end());
}

/**

void ParticleSystem::buildInstanceData(std::vector<InstanceAttrib>& out, const glm::mat4& viewProj) const {
    out.clear();
    out.reserve(particles.size());
    for (const auto& p : particles) {
        glm::vec4 clipSpace = viewProj * glm::vec4(p.pos, 1.0f);
        if (clipSpace.w > 0.0f) {
            glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
            if (std::abs(ndc.x) > 1.2f  std::abs(ndc.y) > 1.2f) {
                continue;
            }
        }

        InstanceAttrib inst;
        inst.pos = p.pos;
        inst.size = p.size;
        inst.color = p.color;
        out.push_back(inst);
    }
}
*/

void ParticleSystem::buildFireInstanceData(std::vector<InstanceAttrib>& out, const glm::mat4& viewProj) const {
    out.clear();
    for (const auto& p : particles) {
        float t = 1.0f - (p.lifetime / p.maxLife);
        if (t >= 0.75f) continue; // skip smoke phase
        // same frustum cull as buildInstanceData
        glm::vec4 clip = viewProj * glm::vec4(p.pos, 1.0f);
        if (clip.w > 0.0f) {
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (std::abs(ndc.x) > 1.2f || std::abs(ndc.y) > 1.2f) continue;
        }
        InstanceAttrib inst;
        inst.pos = p.pos;
        inst.size = p.size;
        inst.color = p.color;
        out.push_back(inst);
    }
}

void ParticleSystem::buildSmokeInstanceData(std::vector<InstanceAttrib>& out, const glm::mat4& viewProj) const {
    out.clear();
    for (const auto& p : particles) {
        float t = 1.0f - (p.lifetime / p.maxLife);
        if (t < 0.75f) continue; // skip fire phase
        glm::vec4 clip = viewProj * glm::vec4(p.pos, 1.0f);
        if (clip.w > 0.0f) {
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (std::abs(ndc.x) > 1.2f || std::abs(ndc.y) > 1.2f) continue;
        }
        InstanceAttrib inst;
        inst.pos = p.pos;
        inst.size = p.size;
        inst.color = p.color;
        out.push_back(inst);
    }
}






float ParticleSystem::randf(float a, float b, float s) const {
    float x = std::sin(s * 12.9898f) * 43758.5453f;
    x = x - std::floor(x);
    return a + (b - a) * x;
}
void ParticleSystem::spawnAt(const glm::vec3& pos, float speed) {
    // Adapted for new struct
    float s = pos.x + pos.y + pos.z + (float)particles.size();
    Particle pr;
    pr.pos = pos;
    if (smokeMode) {
        float vx = randf(-1.0f, 1.0f, s * 1.3f) * 0.15f;
        float vz = randf(-1.0f, 1.0f, s * 2.1f) * 0.15f;
        float vy = randf(speed * 0.2f, speed * 0.6f, s * 3.7f);
        pr.vel = glm::vec3(vx, vy, vz);
        pr.maxLife = randf(2.5f, 6.0f, s);
        pr.color = glm::vec4(0.35f, 0.35f, 0.35f, 0.25f);
    }
    else {
        float vx = randf(-0.5f, 0.5f, s * 1.3f) * emitter.radius;
        float vz = randf(-0.5f, 0.5f, s * 2.1f) * emitter.radius;
        float vy = randf(speed * 0.7f, speed * 1.2f, s * 3.7f);
        pr.vel = glm::vec3(vx, vy, vz);
        pr.maxLife = randf(std::max(0.2f, emitter.lifetimeBase * 0.6f), emitter.lifetimeBase + 0.5f, s);
        pr.color = glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);
    }
    pr.lifetime = pr.maxLife;
    pr.size = emitter.baseSize;
    pr.seed = s;
    particles.push_back(pr);
}



/*
void ParticleSystem::buildSmokeEmitPositions(std::vector<glm::vec3>& out) const {
    out.clear();
    for (const auto& p : particles) {
        float t = 1.0f - (p.lifetime / p.maxLife);
        if (t > 0.8f && t < 0.9f) {
             if (randf(0.0f, 1.0f, p.seed * t) > 0.8f) {
                 out.push_back(p.pos);
             }
        }
    }
}
*/

void ParticleSystem::reset() {
    particles.clear();
    particles.reserve(2000);
}