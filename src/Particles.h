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

// ---------------------------------------------------------------------------
// Navier-Stokes velocity grid  (Jos Stam "Stable Fluids", 1999)
//
//  Uses a MAC (marker-and-cell) staggered layout:
//    u[i,j,k]  — x-velocity face between cell (i-1,j,k) and (i,j,k)
//    v[i,j,k]  — y-velocity face
//    w[i,j,k]  — z-velocity face
//
//  Cell scalars (pressure, temperature, density) live at cell centres.
//  All arrays are flat row-major: idx = i + Nx*(j + Ny*k)
// ---------------------------------------------------------------------------

struct NavierStokesGrid {
    // Grid dimensions (cell count per axis)
    int   Nx = 32, Ny = 64, Nz = 32;
    float cellSize = 0.1f;   // world-space size of one cell
    glm::vec3 origin;          // world position of cell (0,0,0)

    // Velocity components — length (Nx+1)*Ny*Nz etc. for staggered faces,
    // but we keep them at cell-centre size for simplicity (semi-staggered).
    std::vector<float> u, v, w;   // current velocity
    std::vector<float> u0, v0, w0;  // scratch / previous step

    // Scalar fields (cell centres)
    std::vector<float> pressure;
    std::vector<float> divergence;
    std::vector<float> temperature;  // drives buoyancy
    std::vector<float> density;      // smoke density (visual)

    // Solver parameters
    float viscosity = 0.00001f; // kinematic viscosity ν
    float diffusion = 0.00005f; // smoke / temperature diffusion
    int   solverIter = 20;       // Gauss-Seidel iterations for projection

    // Helpers
    int  idx(int i, int j, int k) const { return i + Nx * (j + Ny * k); }
    int  size()                   const { return Nx * Ny * Nz; }

    // Clamp index to [0, N-1]
    int ci(int i, int N) const { return (i < 0) ? 0 : (i >= N ? N - 1 : i); }
    int safeIdx(int i, int j, int k) const {
        return ci(i, Nx) + Nx * (ci(j, Ny) + Ny * ci(k, Nz));
    }

    void allocate();
    void clear();

    // Convert world position to grid coords (float)
    glm::vec3 worldToGrid(const glm::vec3& p) const {
        return (p - origin) / cellSize;
    }
    // Trilinear velocity sample at world position p
    glm::vec3 sampleVelocity(const glm::vec3& p) const;

    // Trilinear scalar sample (temperature / density)
    float sampleScalar(const std::vector<float>& field, const glm::vec3& p) const;
};


class ParticleSystem {
public:
    void configure(const EmitterSettings& e, const GlobalParams& g);
    void setSmoke(bool smoke);
    void setTornado(bool enabled, const glm::vec3& origin, float strength,
        float radius, float inflow, float updraft);
    void setDisturbers(const std::vector<Disturber>& d);
    void configureFluid(int Nx, int Ny, int Nz,
        float cellSize, const glm::vec3& origin,
        float viscosity = 0.00001f,
        float diffusion = 0.00005f,
        int   solverIter = 20);

    void spawn(int count);
    void spawnAt(const glm::vec3& pos, float speed);
    void update(float dt, float time);
    void reset();
    void buildFireInstanceData(std::vector<InstanceAttrib>& out, const glm::mat4& viewProj) const;
    void buildSmokeInstanceData(std::vector<InstanceAttrib>& out, const glm::mat4& viewProj) const;
    int count() const { return (int)particles.size(); }

    const NavierStokesGrid& grid() const { return nsGrid; }

private:
    std::vector<Particle> particles;
    EmitterSettings emitter;
    GlobalParams    params;

    bool      smokeMode = false;
    float     smokeDensity = 1.0f;

    bool      tornadoEnabled = false;
    glm::vec3 tornadoOrigin = glm::vec3(0.0f);
    float     tornadoStrength = 0.0f;
    float     tornadoRadius = 1.0f;
    float     tornadoInflow = 0.0f;
    float     tornadoUpdraft = 0.0f;

    std::vector<Disturber> disturbers;

    NavierStokesGrid nsGrid;
    bool             nsEnabled = false;
    void stepFluid(float dt);
    void fluidDiffuse(float dt,std::vector<float>& x, std::vector<float>& x0, float diff, int axis);
    void fluidProject();
    void fluidAdvect(float dt, std::vector<float>& d, std::vector<float>& d0, const std::vector<float>& uRef, const std::vector<float>& vRef, const std::vector<float>& wRef,int axis);
    void setBounds(int axis, std::vector<float>& x);
    void linearSolve(int axis, std::vector<float>& x, const std::vector<float>& x0,float a, float cRecip);


    float     randf(float a, float b, float s) const;
    float     noise(float x, float y, float z) const;
    glm::vec3 computeCurl(const glm::vec3& p) const;
};