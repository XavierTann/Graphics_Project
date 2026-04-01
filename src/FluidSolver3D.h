#pragma once
#include <vector>
#include <glm/glm.hpp>

class FluidSolver3D {
public:
    FluidSolver3D(int size, float diffusion, float viscosity, float dt);
    ~FluidSolver3D() = default;

    void step();
    void clear();
    void addDensity(int x, int y, int z, float amount);
    void addTemperature(int x, int y, int z, float amount);
    void addVelocity(int x, int y, int z, float amountX, float amountY, float amountZ);

    int getSize() const { return size; }
    const float* getDensity() const { return density.data(); }
    const float* getTemperature() const { return temp.data(); }
    const float* getVx() const { return Vx.data(); }
    const float* getVy() const { return Vy.data(); }
    const float* getVz() const { return Vz.data(); }

    void setBuoyancy(float alpha, float beta) { buoyancyAlpha = alpha; buoyancyBeta = beta; }
    void setCooling(float c) { cooling = c; }
    void setDissipation(float d) { densityDissipation = d; }
    void setMacCormack(bool use) { useMacCormack = use; }
    void setAmbientTemperature(float temp) { ambientTemp = temp; }
    void setWind(const glm::vec3& w) { wind = w; }
    void setDt(float newDt) { dt = newDt; }
    void setPressureIterations(int iters) {
        pressureIterations = iters;
        if (pressureIterations < 1) pressureIterations = 1;
        if (pressureIterations > 200) pressureIterations = 200;
    }
    void setDiffusionIterations(int iters) {
        diffusionIterations = iters;
        if (diffusionIterations < 1) diffusionIterations = 1;
        if (diffusionIterations > 200) diffusionIterations = 200;
    }

    inline int IX(int x, int y, int z) const {
        x = glm::clamp(x, 0, size - 1);
        y = glm::clamp(y, 0, size - 1);
        z = glm::clamp(z, 0, size - 1);
        return x + y * size + z * size * size;
    }

private:
    int size;
    float dt;
    float diff;
    float visc;

    int pressureIterations = 10;
    int diffusionIterations = 10;

    float buoyancyAlpha = 0.5f; // Density weight
    float buoyancyBeta = 1.5f;  // Temperature weight
    float cooling = 0.99f;       
    float densityDissipation = 0.99f;
    float ambientTemp = 0.0f;
    bool useMacCormack = false; // Disable MacCormack by default as it can cause instability if not tuned well
    glm::vec3 wind = glm::vec3(0.0f);

    std::vector<float> density;
    std::vector<float> density0;
    
    std::vector<float> temp;
    std::vector<float> temp0;

    std::vector<float> Vx;
    std::vector<float> Vy;
    std::vector<float> Vz;

    std::vector<float> Vx0;
    std::vector<float> Vy0;
    std::vector<float> Vz0;

    std::vector<float> linSolveTmp;
    std::vector<float> macCormackFwd;
    std::vector<float> macCormackBwd;

    void setBnd(int b, std::vector<float>& x);
    void linSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c, int iter);
    void diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff);
    void advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, const std::vector<float>& w);
    void advectInternal(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, const std::vector<float>& w, float dtSign);
    void advectMacCormack(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, const std::vector<float>& w);
    void project(std::vector<float>& u, std::vector<float>& v, std::vector<float>& w, std::vector<float>& p, std::vector<float>& div, int iter);
    void applyBuoyancy();
    void applyWind();
    void coolAndDissipate();
};
