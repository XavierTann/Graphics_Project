#pragma once
#include <vector>
#include <glm/glm.hpp>

struct Particle {
    glm::vec3 pos;
    glm::vec3 vel;
    float lifetime;
    float maxLife;
    glm::vec4 color;
    float size;
    float seed;
};

struct InstanceAttrib {
    glm::vec3 pos;
    float size;
    glm::vec4 color;
};

struct EmitterSettings {
    glm::vec3 origin;
    float radius;
    float initialSpeedMin;
    float initialSpeedMax;
    float baseSize;
    float lifetimeBase;
};

struct GlobalParams {
    glm::vec3 wind;
    float buoyancy;
    float cooling;
    float humidity;
    float turbAmp;
    float turbFreq;
};

struct Disturber {
    glm::vec3 pos;
    float radius;
    float strength;
};

class ParticleSystem {
public:
    void configure(const EmitterSettings& e, const GlobalParams& g);
    void setSmoke(bool smoke);
    void setSmokeDensity(float density);
    void setTornado(bool enabled, const glm::vec3& origin, float strength, float radius, float inflow, float updraft);
    void setDisturbers(const std::vector<Disturber>& d);
    void spawn(int count);
    void update(float dt, float time);
    void buildInstanceData(std::vector<InstanceAttrib>& out, const glm::mat4& viewProj) const;
    void spawnAt(const glm::vec3& pos, float speed);
    void buildSmokeEmitPositions(std::vector<glm::vec3>& out) const;
    void reset();
    int count() const { return (int)particles.size(); }
private:
    std::vector<Particle> particles;
    EmitterSettings emitter;
    GlobalParams params;
    bool smokeMode = false;
    float smokeDensity = 1.0f;
    bool tornadoEnabled = false;
    glm::vec3 tornadoOrigin = glm::vec3(0.0f);
    float tornadoStrength = 0.0f;
    float tornadoRadius = 1.0f;
    float tornadoInflow = 0.0f;
    float tornadoUpdraft = 0.0f;
    std::vector<Disturber> disturbers;
    float randf(float a, float b, float s) const;
    float noise(float x, float y, float z) const;
    glm::vec3 computeCurl(const glm::vec3& p) const;
};
