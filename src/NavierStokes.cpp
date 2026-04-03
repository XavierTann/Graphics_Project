// Implements the NavierStokesGrid methods and the ParticleSystem fluid pipeline.
// Solver: Jos Stam "Stable Fluids" (1999) adapted to 3-D with buoyancy,
//         wind, tornado, and disturber body forces.

#include "Particles.h"
#include <cmath>
#include <algorithm>
#include <numeric>



void NavierStokesGrid::allocate() {
    int n = size();
    u.assign(n, 0.f);  v.assign(n, 0.f);  w.assign(n, 0.f);
    u0.assign(n, 0.f); v0.assign(n, 0.f); w0.assign(n, 0.f);
    pressure.assign(n, 0.f);
    divergence.assign(n, 0.f);
    temperature.assign(n, 0.f);
    density.assign(n, 0.f);
}

void NavierStokesGrid::clear() {
    std::fill(u.begin(), u.end(), 0.f);
    std::fill(v.begin(), v.end(), 0.f);
    std::fill(w.begin(), w.end(), 0.f);
    std::fill(u0.begin(), u0.end(), 0.f);
    std::fill(v0.begin(), v0.end(), 0.f);
    std::fill(w0.begin(), w0.end(), 0.f);
    std::fill(pressure.begin(), pressure.end(), 0.f);
    std::fill(divergence.begin(), divergence.end(), 0.f);
    std::fill(temperature.begin(), temperature.end(), 0.f);
    std::fill(density.begin(), density.end(), 0.f);
}

// Trilinear interpolation helper (cell-centre sampling)
static float trilinear(const std::vector<float>& f,
    int Nx, int Ny, int Nz,
    float gx, float gy, float gz)
{
    gx = std::max(0.5f, std::min((float)Nx - 1.5f, gx));
    gy = std::max(0.5f, std::min((float)Ny - 1.5f, gy));
    gz = std::max(0.5f, std::min((float)Nz - 1.5f, gz));

    int i0 = (int)gx, i1 = i0 + 1;
    int j0 = (int)gy, j1 = j0 + 1;
    int k0 = (int)gz, k1 = k0 + 1;

    float tx = gx - i0, ty = gy - j0, tz = gz - k0;

    auto idx = [&](int i, int j, int k) { return i + Nx * (j + Ny * k); };

    float c000 = f[idx(i0, j0, k0)], c100 = f[idx(i1, j0, k0)];
    float c010 = f[idx(i0, j1, k0)], c110 = f[idx(i1, j1, k0)];
    float c001 = f[idx(i0, j0, k1)], c101 = f[idx(i1, j0, k1)];
    float c011 = f[idx(i0, j1, k1)], c111 = f[idx(i1, j1, k1)];

    return (1 - tz) * ((1 - ty) * ((1 - tx) * c000 + tx * c100) +
        ty * ((1 - tx) * c010 + tx * c110)) +
        tz * ((1 - ty) * ((1 - tx) * c001 + tx * c101) +
            ty * ((1 - tx) * c011 + tx * c111));
}

glm::vec3 NavierStokesGrid::sampleVelocity(const glm::vec3& p) const {
    glm::vec3 g = worldToGrid(p);
    return glm::vec3(
        trilinear(u, Nx, Ny, Nz, g.x, g.y, g.z),
        trilinear(v, Nx, Ny, Nz, g.x, g.y, g.z),
        trilinear(w, Nx, Ny, Nz, g.x, g.y, g.z)
    );
}

float NavierStokesGrid::sampleScalar(const std::vector<float>& field,
    const glm::vec3& p) const
{
    glm::vec3 g = worldToGrid(p);
    return trilinear(field, Nx, Ny, Nz, g.x, g.y, g.z);
}


void ParticleSystem::configureFluid(int Nx, int Ny, int Nz,
    float cellSize, const glm::vec3& origin,
    float viscosity, float diffusion,
    int solverIter)
{
    nsGrid.Nx = Nx;
    nsGrid.Ny = Ny;
    nsGrid.Nz = Nz;
    nsGrid.cellSize = cellSize;
    nsGrid.origin = origin;
    nsGrid.viscosity = viscosity;
    nsGrid.diffusion = diffusion;
    nsGrid.solverIter = solverIter;
    nsGrid.allocate();
    nsEnabled = true;
}

void ParticleSystem::setBounds(int axis, std::vector<float>& x) {
    const int& Nx = nsGrid.Nx;
    const int& Ny = nsGrid.Ny;
    const int& Nz = nsGrid.Nz;
    auto idx = [&](int i, int j, int k) { return nsGrid.idx(i, j, k); };

    for (int j = 0; j < Ny; ++j) {
        for (int k = 0; k < Nz; ++k) {
            x[idx(0, j, k)] = (axis == 1) ? -x[idx(1, j, k)] : x[idx(1, j, k)];
            x[idx(Nx - 1, j, k)] = (axis == 1) ? -x[idx(Nx - 2, j, k)] : x[idx(Nx - 2, j, k)];
        }
    }
    for (int i = 0; i < Nx; ++i) {
        for (int k = 0; k < Nz; ++k) {
            x[idx(i, 0, k)] = (axis == 2) ? -x[idx(i, 1, k)] : x[idx(i, 1, k)];
            x[idx(i, Ny - 1, k)] = (axis == 2) ? -x[idx(i, Ny - 2, k)] : x[idx(i, Ny - 2, k)];
        }
    }
    for (int i = 0; i < Nx; ++i) {
        for (int j = 0; j < Ny; ++j) {
            x[idx(i, j, 0)] = (axis == 3) ? -x[idx(i, j, 1)] : x[idx(i, j, 1)];
            x[idx(i, j, Nz - 1)] = (axis == 3) ? -x[idx(i, j, Nz - 2)] : x[idx(i, j, Nz - 2)];
        }
    }

    for (int j = 1; j < Ny - 1; ++j) {
        x[idx(0, j, 0)] = 0.5f * (x[idx(1, j, 0)] + x[idx(0, j, 1)]);
        x[idx(Nx - 1, j, 0)] = 0.5f * (x[idx(Nx - 2, j, 0)] + x[idx(Nx - 1, j, 1)]);
        x[idx(0, j, Nz - 1)] = 0.5f * (x[idx(1, j, Nz - 1)] + x[idx(0, j, Nz - 2)]);
        x[idx(Nx - 1, j, Nz - 1)] = 0.5f * (x[idx(Nx - 2, j, Nz - 1)] + x[idx(Nx - 1, j, Nz - 2)]);
    }
}

// ===========================================================================
//  Linear solver (Gauss-Seidel)  —  solves (I - a·∇²) x = x0
// ===========================================================================

void ParticleSystem::linearSolve(int axis,
    std::vector<float>& x,
    const std::vector<float>& x0,
    float a, float cRecip)
{
    const int& Nx = nsGrid.Nx;
    const int& Ny = nsGrid.Ny;
    const int& Nz = nsGrid.Nz;
    auto idx = [&](int i, int j, int k) { return nsGrid.idx(i, j, k); };

    for (int iter = 0; iter < nsGrid.solverIter; ++iter) {
        for (int k = 1; k < Nz - 1; ++k) {
            for (int j = 1; j < Ny - 1; ++j) {
                for (int i = 1; i < Nx - 1; ++i) {
                    x[idx(i, j, k)] = (x0[idx(i, j, k)]
                        + a * (x[idx(i - 1, j, k)] + x[idx(i + 1, j, k)]
                            + x[idx(i, j - 1, k)] + x[idx(i, j + 1, k)]
                            + x[idx(i, j, k - 1)] + x[idx(i, j, k + 1)])) * cRecip;
                }
            }
        }
        setBounds(axis, x);
    }
}

// ===========================================================================
//  Diffuse  —  implicit backward-Euler viscosity / diffusion
//  a = dt * diff * (N^3)  (Stam normalises by grid cell count)
// ===========================================================================

void ParticleSystem::fluidDiffuse(float dt,
    std::vector<float>& x,
    std::vector<float>& x0,
    float diff, int axis)
{
    int N3 = nsGrid.Nx * nsGrid.Ny * nsGrid.Nz;
    float a = dt * diff * (float)N3;
    linearSolve(axis, x, x0, a, 1.0f / (1.0f + 6.0f * a));
}

// ===========================================================================
//  Project  —  enforce ∇·u = 0 (Helmholtz decomposition)
//  Solves ∇²p = ∇·u  with Gauss-Seidel, then subtracts ∇p from velocity.
// ===========================================================================

void ParticleSystem::fluidProject() {
    const int& Nx = nsGrid.Nx;
    const int& Ny = nsGrid.Ny;
    const int& Nz = nsGrid.Nz;
    const float& h = nsGrid.cellSize;
    auto& u = nsGrid.u;
    auto& v = nsGrid.v;
    auto& w = nsGrid.w;
    auto& p = nsGrid.pressure;
    auto& div = nsGrid.divergence;
    auto idx = [&](int i, int j, int k) { return nsGrid.idx(i, j, k); };

    
    for (int k = 1; k < Nz - 1; ++k)
        for (int j = 1; j < Ny - 1; ++j)
            for (int i = 1; i < Nx - 1; ++i) {
                div[idx(i, j, k)] = -0.5f * h * (
                    u[idx(i + 1, j, k)] - u[idx(i - 1, j, k)] +
                    v[idx(i, j + 1, k)] - v[idx(i, j - 1, k)] +
                    w[idx(i, j, k + 1)] - w[idx(i, j, k - 1)]);
                p[idx(i, j, k)] = 0.0f;
            }
    setBounds(0, div);
    setBounds(0, p);

    // Solve ∇²p = div  (Gauss-Seidel, a=1, cRecip=1/6)
    linearSolve(0, p, div, 1.0f, 1.0f / 6.0f);

    // Subtract pressure gradient
    for (int k = 1; k < Nz - 1; ++k)
        for (int j = 1; j < Ny - 1; ++j)
            for (int i = 1; i < Nx - 1; ++i) {
                u[idx(i, j, k)] -= 0.5f * (p[idx(i + 1, j, k)] - p[idx(i - 1, j, k)]) / h;
                v[idx(i, j, k)] -= 0.5f * (p[idx(i, j + 1, k)] - p[idx(i, j - 1, k)]) / h;
                w[idx(i, j, k)] -= 0.5f * (p[idx(i, j, k + 1)] - p[idx(i, j, k - 1)]) / h;
            }
    setBounds(1, u);
    setBounds(2, v);
    setBounds(3, w);
}

// ===========================================================================
//  Advect  —  semi-Lagrangian back-trace
//  Traces each cell centre backwards along the current velocity field and
//  interpolates — unconditionally stable for any dt.
// ===========================================================================

void ParticleSystem::fluidAdvect(float dt,
    std::vector<float>& d,
    std::vector<float>& d0,
    const std::vector<float>& uRef,
    const std::vector<float>& vRef,
    const std::vector<float>& wRef,
    int axis)
{
    const int& Nx = nsGrid.Nx;
    const int& Ny = nsGrid.Ny;
    const int& Nz = nsGrid.Nz;
    const float& h = nsGrid.cellSize;
    auto idx = [&](int i, int j, int k) { return nsGrid.idx(i, j, k); };

    float dtx = dt / h;

    for (int k = 1; k < Nz - 1; ++k) {
        for (int j = 1; j < Ny - 1; ++j) {
            for (int i = 1; i < Nx - 1; ++i) {
                float x = (float)i - dtx * uRef[idx(i, j, k)];
                float y = (float)j - dtx * vRef[idx(i, j, k)];
                float z = (float)k - dtx * wRef[idx(i, j, k)];

                x = std::max(0.5f, std::min((float)Nx - 1.5f, x));
                y = std::max(0.5f, std::min((float)Ny - 1.5f, y));
                z = std::max(0.5f, std::min((float)Nz - 1.5f, z));

                int i0 = (int)x, i1 = i0 + 1;
                int j0 = (int)y, j1 = j0 + 1;
                int k0 = (int)z, k1 = k0 + 1;

                float tx = x - i0, ty = y - j0, tz = z - k0;

                d[idx(i, j, k)] =
                    (1 - tz) * ((1 - ty) * ((1 - tx) * d0[idx(i0, j0, k0)] + tx * d0[idx(i1, j0, k0)]) +
                        ty * ((1 - tx) * d0[idx(i0, j1, k0)] + tx * d0[idx(i1, j1, k0)])) +
                    tz * ((1 - ty) * ((1 - tx) * d0[idx(i0, j0, k1)] + tx * d0[idx(i1, j0, k1)]) +
                        ty * ((1 - tx) * d0[idx(i0, j1, k1)] + tx * d0[idx(i1, j1, k1)]));
            }
        }
    }
    setBounds(axis, d);
}

// ===========================================================================
//  Source injection
//  Adds velocity, temperature, and smoke density at the emitter location.
//  Also injects wind, tornado body force, and disturbers into the grid.
// ===========================================================================

void ParticleSystem::fluidAddSources(float dt) {
    const int& Nx = nsGrid.Nx;
    const int& Ny = nsGrid.Ny;
    const int& Nz = nsGrid.Nz;
    const float& h = nsGrid.cellSize;
    auto& u = nsGrid.u;
    auto& v = nsGrid.v;
    auto& w = nsGrid.w;
    auto& T = nsGrid.temperature;
    auto& den = nsGrid.density;
    auto idx = [&](int i, int j, int k) { return nsGrid.idx(i, j, k); };

    // --- Emitter: inject upward velocity + heat + smoke density ---
    {
        glm::vec3 g = nsGrid.worldToGrid(emitter.origin);
        int ci = std::max(1, std::min(Nx - 2, (int)g.x));
        int cj = std::max(1, std::min(Ny - 2, (int)g.y));
        int ck = std::max(1, std::min(Nz - 2, (int)g.z));

        // Inject in a small sphere of radius emitter.radius / cellSize
        int rg = std::max(1, (int)(emitter.radius / h));
        for (int dk = -rg; dk <= rg; ++dk)
            for (int dj = -rg; dj <= rg; ++dj)
                for (int di = -rg; di <= rg; ++di) {
                    float dist2 = (float)(di * di + dj * dj + dk * dk);
                    if (dist2 > (float)(rg * rg)) continue;
                    int ii = ci + di, jj = cj + dj, kk = ck + dk;
                    if (ii < 1 || ii >= Nx - 1 || jj < 1 || jj >= Ny - 1 || kk < 1 || kk >= Nz - 1) continue;
                    float w_ = 1.0f - std::sqrt(dist2) / (float)rg;
                    float spd = (emitter.initialSpeedMin + emitter.initialSpeedMax) * 0.5f;
                    v[idx(ii, jj, kk)] += spd * w_ * dt * 30.0f;  // upward
                    T[idx(ii, jj, kk)] += 5.0f * w_ * dt;          // heat
                    den[idx(ii, jj, kk)] += 1.5f * w_ * dt;        // smoke
                }
    }

    // --- Global wind body force ---
    {
        glm::vec3 windForce = params.wind * dt;
        for (int n = 0; n < (int)u.size(); ++n) {
            u[n] += windForce.x;
            v[n] += windForce.y;
            w[n] += windForce.z;
        }
    }

    // --- Buoyancy: hot cells rise.  Uses Boussinesq approximation ---
    //     F_y = buoyancy * T
    {
        float buoy = params.buoyancy * dt;
        for (int k = 1; k < Nz - 1; ++k)
            for (int j = 1; j < Ny - 1; ++j)
                for (int i = 1; i < Nx - 1; ++i)
                    v[idx(i, j, k)] += buoy * T[idx(i, j, k)];
    }

    // --- Tornado body force ---
    if (tornadoEnabled) {
        for (int k = 1; k < Nz - 1; ++k) {
            for (int j = 1; j < Ny - 1; ++j) {
                for (int i = 1; i < Nx - 1; ++i) {
                    // World position of cell centre
                    glm::vec3 wp = nsGrid.origin +
                        glm::vec3(i + 0.5f, j + 0.5f, k + 0.5f) * h;
                    glm::vec3 rel = wp - tornadoOrigin;
                    float r = std::sqrt(rel.x * rel.x + rel.z * rel.z);
                    float falloff = std::exp(-r / tornadoRadius);

                    glm::vec3 tang(0.f), inward(0.f);
                    if (r > 1e-4f) {
                        tang = glm::normalize(glm::vec3(-rel.z, 0.f, rel.x));
                        inward = -glm::normalize(glm::vec3(rel.x, 0.f, rel.z));
                    }
                    int n = idx(i, j, k);
                    u[n] += (tang.x * tornadoStrength + inward.x * tornadoInflow) * falloff * dt;
                    v[n] += tornadoUpdraft * falloff * dt;
                    w[n] += (tang.z * tornadoStrength + inward.z * tornadoInflow) * falloff * dt;
                }
            }
        }
    }

    // --- Disturbers ---
    for (const auto& dist : disturbers) {
        for (int k = 1; k < Nz - 1; ++k) {
            for (int j = 1; j < Ny - 1; ++j) {
                for (int i = 1; i < Nx - 1; ++i) {
                    glm::vec3 wp = nsGrid.origin +
                        glm::vec3(i + 0.5f, j + 0.5f, k + 0.5f) * h;
                    glm::vec3 rel = wp - dist.pos;
                    float r = glm::length(rel);
                    if (r >= dist.radius || dist.radius < 1e-4f) continue;
                    float falloff = 1.0f - r / dist.radius;
                    glm::vec3 horiz(rel.x, 0.f, rel.z);
                    float hr = glm::length(horiz);
                    if (hr < 1e-4f) continue;
                    glm::vec3 push = glm::normalize(horiz);
                    glm::vec3 swirl = glm::normalize(glm::vec3(-horiz.z, 0.f, horiz.x));
                    glm::vec3 f = (swirl + push * 0.35f) * dist.strength * falloff * dt;
                    int n = idx(i, j, k);
                    u[n] += f.x;
                    w[n] += f.z;
                }
            }
        }
    }

    // --- Cool down temperature field ---
    {
        float cool = 1.0f - params.cooling * dt;
        cool = std::max(0.0f, cool);
        for (auto& t : T) t *= cool;
    }
}

// ===========================================================================
//  stepFluid  —  one full NS step
//  Pipeline:  sources → diffuse vel → project → advect vel → project
//             (scalar density/temperature follow with advect + diffuse)
// ===========================================================================

void ParticleSystem::stepFluid(float dt) {
    auto& ns = nsGrid;

    // Swap buffers (previous ← current, then overwrite current)
    std::swap(ns.u, ns.u0);
    std::swap(ns.v, ns.v0);
    std::swap(ns.w, ns.w0);

    // 1. Inject sources into velocity / temperature / density
    fluidAddSources(dt);

    // 2. Diffuse velocity  (implicit)
    fluidDiffuse(dt, ns.u, ns.u0, ns.viscosity, 1);
    fluidDiffuse(dt, ns.v, ns.v0, ns.viscosity, 2);
    fluidDiffuse(dt, ns.w, ns.w0, ns.viscosity, 3);

    // 3. Project after diffuse
    fluidProject();

    // 4. Advect velocity along itself
    std::swap(ns.u, ns.u0);
    std::swap(ns.v, ns.v0);
    std::swap(ns.w, ns.w0);
    fluidAdvect(dt, ns.u, ns.u0, ns.u0, ns.v0, ns.w0, 1);
    fluidAdvect(dt, ns.v, ns.v0, ns.u0, ns.v0, ns.w0, 2);
    fluidAdvect(dt, ns.w, ns.w0, ns.u0, ns.v0, ns.w0, 3);

    // 5. Project after advect (ensures final field is divergence-free)
    fluidProject();

    // 6. Advect scalar density and temperature
    {
        auto  den_prev = ns.density;
        auto  T_prev = ns.temperature;
        fluidAdvect(dt, ns.density, den_prev, ns.u, ns.v, ns.w, 0);
        fluidAdvect(dt, ns.temperature, T_prev, ns.u, ns.v, ns.w, 0);

        // Diffuse scalars
        auto den2 = ns.density;
        auto T2 = ns.temperature;
        fluidDiffuse(dt, ns.density, den2, ns.diffusion, 0);
        fluidDiffuse(dt, ns.temperature, T2, ns.diffusion, 0);
    }

    // 7. Clamp density to avoid blow-up
    for (auto& d : ns.density)     d = std::max(0.0f, std::min(5.0f, d));
    for (auto& t : ns.temperature) t = std::max(0.0f, std::min(20.0f, t));
}