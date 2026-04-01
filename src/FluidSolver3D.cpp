#include "FluidSolver3D.h"
#include <algorithm>
#include <cmath>
#include <omp.h>

FluidSolver3D::FluidSolver3D(int size, float diffusion, float viscosity, float dt)
    : size(size), diff(diffusion), visc(viscosity), dt(dt) {
    int N = size * size * size;
    density.resize(N, 0.0f);
    density0.resize(N, 0.0f);
    temp.resize(N, 0.0f);
    temp0.resize(N, 0.0f);
    Vx.resize(N, 0.0f);
    Vy.resize(N, 0.0f);
    Vz.resize(N, 0.0f);
    Vx0.resize(N, 0.0f);
    Vy0.resize(N, 0.0f);
    Vz0.resize(N, 0.0f);
    linSolveTmp.resize(N, 0.0f);
    macCormackFwd.resize(N, 0.0f);
    macCormackBwd.resize(N, 0.0f);
}

void FluidSolver3D::clear() {
    std::fill(density.begin(), density.end(), 0.0f);
    std::fill(density0.begin(), density0.end(), 0.0f);
    std::fill(temp.begin(), temp.end(), 0.0f);
    std::fill(temp0.begin(), temp0.end(), 0.0f);
    std::fill(Vx.begin(), Vx.end(), 0.0f);
    std::fill(Vy.begin(), Vy.end(), 0.0f);
    std::fill(Vz.begin(), Vz.end(), 0.0f);
    std::fill(Vx0.begin(), Vx0.end(), 0.0f);
    std::fill(Vy0.begin(), Vy0.end(), 0.0f);
    std::fill(Vz0.begin(), Vz0.end(), 0.0f);
}

void FluidSolver3D::addDensity(int x, int y, int z, float amount) {
    int N = size;
    int r = 3; // radius for injection
    for (int k = -r; k <= r; k++) {
        for (int j = -r; j <= r; j++) {
            for (int i = -r; i <= r; i++) {
                int nx = x + i, ny = y + j, nz = z + k;
                if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz >= 0 && nz < N) {
                    float dist = sqrt((float)(i*i + j*j + k*k));
                    if (dist <= r) {
                        float factor = 1.0f - dist / r;
                        density[IX(nx, ny, nz)] += amount * factor;
                    }
                }
            }
        }
    }
}

void FluidSolver3D::addTemperature(int x, int y, int z, float amount) {
    int N = size;
    int r = 3; // radius for injection
    for (int k = -r; k <= r; k++) {
        for (int j = -r; j <= r; j++) {
            for (int i = -r; i <= r; i++) {
                int nx = x + i, ny = y + j, nz = z + k;
                if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz >= 0 && nz < N) {
                    float dist = sqrt((float)(i*i + j*j + k*k));
                    if (dist <= r) {
                        float factor = 1.0f - dist / r;
                        temp[IX(nx, ny, nz)] += amount * factor;
                    }
                }
            }
        }
    }
}

void FluidSolver3D::addVelocity(int x, int y, int z, float amountX, float amountY, float amountZ) {
    int N = size;
    int r = 3; // radius for injection
    for (int k = -r; k <= r; k++) {
        for (int j = -r; j <= r; j++) {
            for (int i = -r; i <= r; i++) {
                int nx = x + i, ny = y + j, nz = z + k;
                if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz >= 0 && nz < N) {
                    float dist = sqrt((float)(i*i + j*j + k*k));
                    if (dist <= r) {
                        float factor = 1.0f - dist / r;
                        Vx[IX(nx, ny, nz)] += amountX * factor;
                        Vy[IX(nx, ny, nz)] += amountY * factor;
                        Vz[IX(nx, ny, nz)] += amountZ * factor;
                    }
                }
            }
        }
    }
}

void FluidSolver3D::setBnd(int b, std::vector<float>& x) {
    // Basic boundary conditions: reflecting boundaries
    int N = size;
    for (int j = 1; j < N - 1; j++) {
        for (int i = 1; i < N - 1; i++) {
            x[IX(i, j, 0)]     = b == 3 ? -x[IX(i, j, 1)]     : x[IX(i, j, 1)];
            x[IX(i, j, N-1)]   = b == 3 ? -x[IX(i, j, N-2)]   : x[IX(i, j, N-2)];
            x[IX(i, 0, j)]     = b == 2 ? -x[IX(i, 1, j)]     : x[IX(i, 1, j)];
            x[IX(i, N-1, j)]   = b == 2 ? -x[IX(i, N-2, j)]   : x[IX(i, N-2, j)];
            x[IX(0, i, j)]     = b == 1 ? -x[IX(1, i, j)]     : x[IX(1, i, j)];
            x[IX(N-1, i, j)]   = b == 1 ? -x[IX(N-2, i, j)]   : x[IX(N-2, i, j)];
        }
    }
    
    // Corners
    x[IX(0, 0, 0)]       = 0.33f * (x[IX(1, 0, 0)] + x[IX(0, 1, 0)] + x[IX(0, 0, 1)]);
    x[IX(0, N-1, 0)]     = 0.33f * (x[IX(1, N-1, 0)] + x[IX(0, N-2, 0)] + x[IX(0, N-1, 1)]);
    x[IX(0, 0, N-1)]     = 0.33f * (x[IX(1, 0, N-1)] + x[IX(0, 1, N-1)] + x[IX(0, 0, N-2)]);
    x[IX(0, N-1, N-1)]   = 0.33f * (x[IX(1, N-1, N-1)] + x[IX(0, N-2, N-1)] + x[IX(0, N-1, N-2)]);
    x[IX(N-1, 0, 0)]     = 0.33f * (x[IX(N-2, 0, 0)] + x[IX(N-1, 1, 0)] + x[IX(N-1, 0, 1)]);
    x[IX(N-1, N-1, 0)]   = 0.33f * (x[IX(N-2, N-1, 0)] + x[IX(N-1, N-2, 0)] + x[IX(N-1, N-1, 1)]);
    x[IX(N-1, 0, N-1)]   = 0.33f * (x[IX(N-2, 0, N-1)] + x[IX(N-1, 1, N-1)] + x[IX(N-1, 0, N-2)]);
    x[IX(N-1, N-1, N-1)] = 0.33f * (x[IX(N-2, N-1, N-1)] + x[IX(N-1, N-2, N-1)] + x[IX(N-1, N-1, N-2)]);
}

void FluidSolver3D::linSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c, int iter) {
    float cRecip = 1.0f / c;
    int N = size;
    if ((int)linSolveTmp.size() != (int)x.size()) linSolveTmp.resize(x.size(), 0.0f);

    for (int k = 0; k < iter; k++) {
        #pragma omp parallel for
        for (int z = 1; z < N - 1; z++) {
            for (int j = 1; j < N - 1; j++) {
                for (int i = 1; i < N - 1; i++) {
                    linSolveTmp[IX(i, j, z)] =
                        (x0[IX(i, j, z)]
                            + a * (x[IX(i+1, j, z)]
                                 + x[IX(i-1, j, z)]
                                 + x[IX(i, j+1, z)]
                                 + x[IX(i, j-1, z)]
                                 + x[IX(i, j, z+1)]
                                 + x[IX(i, j, z-1)]
                            )) * cRecip;
                }
            }
        }
        x.swap(linSolveTmp);
        setBnd(b, x);
    }
}

void FluidSolver3D::diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff) {
    float a = dt * diff * (size - 2) * (size - 2) * (size - 2);
    linSolve(b, x, x0, a, 1.0f + 6.0f * a, diffusionIterations);
}

void FluidSolver3D::advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, const std::vector<float>& w) {
    advectInternal(b, d, d0, u, v, w, 1.0f);
}

void FluidSolver3D::advectInternal(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, const std::vector<float>& w, float dtSign) {
    int N = size;
    float dt0 = dt * N; // Note: using N instead of N-2 for simpler world space mapping

    #pragma omp parallel for
    for (int z = 1; z < N - 1; z++) {
        for (int j = 1; j < N - 1; j++) {
            for (int i = 1; i < N - 1; i++) {
                float x = i - (dtSign * dt0) * u[IX(i, j, z)];
                float y = j - (dtSign * dt0) * v[IX(i, j, z)];
                float z_pos = z - (dtSign * dt0) * w[IX(i, j, z)];

                if (x < 0.5f) x = 0.5f;
                if (x > N - 1.5f) x = N - 1.5f;
                int i0 = (int)x;
                int i1 = i0 + 1;
                
                if (y < 0.5f) y = 0.5f;
                if (y > N - 1.5f) y = N - 1.5f;
                int j0 = (int)y;
                int j1 = j0 + 1;

                if (z_pos < 0.5f) z_pos = 0.5f;
                if (z_pos > N - 1.5f) z_pos = N - 1.5f;
                int k0 = (int)z_pos;
                int k1 = k0 + 1;

                float s1 = x - i0;
                float s0 = 1.0f - s1;
                float t1 = y - j0;
                float t0 = 1.0f - t1;
                float u1 = z_pos - k0;
                float u0 = 1.0f - u1;

                d[IX(i, j, z)] =
                    s0 * (t0 * (u0 * d0[IX(i0, j0, k0)] + u1 * d0[IX(i0, j0, k1)]) +
                          t1 * (u0 * d0[IX(i0, j1, k0)] + u1 * d0[IX(i0, j1, k1)])) +
                    s1 * (t0 * (u0 * d0[IX(i1, j0, k0)] + u1 * d0[IX(i1, j0, k1)]) +
                          t1 * (u0 * d0[IX(i1, j1, k0)] + u1 * d0[IX(i1, j1, k1)]));
            }
        }
    }
    setBnd(b, d);
}

void FluidSolver3D::advectMacCormack(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, const std::vector<float>& w) {
    if ((int)macCormackFwd.size() != (int)d0.size()) macCormackFwd.resize(d0.size(), 0.0f);
    if ((int)macCormackBwd.size() != (int)d0.size()) macCormackBwd.resize(d0.size(), 0.0f);

    advectInternal(b, macCormackFwd, d0, u, v, w, 1.0f);
    advectInternal(b, macCormackBwd, macCormackFwd, u, v, w, -1.0f);

    // 3. Error correction and Min-Max Limiting
    int N = size;
    float dt0 = dt * N;

    #pragma omp parallel for
    for (int z = 1; z < N - 1; z++) {
        for (int j = 1; j < N - 1; j++) {
            for (int i = 1; i < N - 1; i++) {
                // Corrected value
                float corrected = macCormackFwd[IX(i, j, z)] + 0.5f * (d0[IX(i, j, z)] - macCormackBwd[IX(i, j, z)]);
                
                // Limiting
                float x = i - dt0 * u[IX(i, j, z)];
                float y = j - dt0 * v[IX(i, j, z)];
                float z_pos = z - dt0 * w[IX(i, j, z)];

                if (x < 0.5f) x = 0.5f; if (x > N - 1.5f) x = N - 1.5f;
                if (y < 0.5f) y = 0.5f; if (y > N - 1.5f) y = N - 1.5f;
                if (z_pos < 0.5f) z_pos = 0.5f; if (z_pos > N - 1.5f) z_pos = N - 1.5f;
                
                int i0 = (int)x; int i1 = i0 + 1;
                int j0 = (int)y; int j1 = j0 + 1;
                int k0 = (int)z_pos; int k1 = k0 + 1;

                float minVal = std::min({
                    d0[IX(i0, j0, k0)], d0[IX(i0, j0, k1)], d0[IX(i0, j1, k0)], d0[IX(i0, j1, k1)],
                    d0[IX(i1, j0, k0)], d0[IX(i1, j0, k1)], d0[IX(i1, j1, k0)], d0[IX(i1, j1, k1)]
                });

                float maxVal = std::max({
                    d0[IX(i0, j0, k0)], d0[IX(i0, j0, k1)], d0[IX(i0, j1, k0)], d0[IX(i0, j1, k1)],
                    d0[IX(i1, j0, k0)], d0[IX(i1, j0, k1)], d0[IX(i1, j1, k0)], d0[IX(i1, j1, k1)]
                });

                d[IX(i, j, z)] = glm::clamp(corrected, minVal, maxVal);
            }
        }
    }
    setBnd(b, d);
}

void FluidSolver3D::project(std::vector<float>& u, std::vector<float>& v, std::vector<float>& w, std::vector<float>& p, std::vector<float>& div, int iter) {
    int N = size;
    float h = 1.0f / N;

    #pragma omp parallel for
    for (int z = 1; z < N - 1; z++) {
        for (int j = 1; j < N - 1; j++) {
            for (int i = 1; i < N - 1; i++) {
                div[IX(i, j, z)] = -0.5f * h * (
                    u[IX(i+1, j, z)] - u[IX(i-1, j, z)] +
                    v[IX(i, j+1, z)] - v[IX(i, j-1, z)] +
                    w[IX(i, j, z+1)] - w[IX(i, j, z-1)]
                );
                p[IX(i, j, z)] = 0;
            }
        }
    }
    setBnd(0, div);
    setBnd(0, p);

    linSolve(0, p, div, 1, 6, iter);

    #pragma omp parallel for
    for (int z = 1; z < N - 1; z++) {
        for (int j = 1; j < N - 1; j++) {
            for (int i = 1; i < N - 1; i++) {
                u[IX(i, j, z)] -= 0.5f * (p[IX(i+1, j, z)] - p[IX(i-1, j, z)]) / h;
                v[IX(i, j, z)] -= 0.5f * (p[IX(i, j+1, z)] - p[IX(i, j-1, z)]) / h;
                w[IX(i, j, z)] -= 0.5f * (p[IX(i, j, z+1)] - p[IX(i, j, z-1)]) / h;
            }
        }
    }
    setBnd(1, u);
    setBnd(2, v);
    setBnd(3, w);
}

void FluidSolver3D::applyBuoyancy() {
    int N = size;
    #pragma omp parallel for
    for (int z = 1; z < N - 1; z++) {
        for (int j = 1; j < N - 1; j++) {
            for (int i = 1; i < N - 1; i++) {
                int index = IX(i, j, z);
                float t = temp[index];
                float d = density[index];
                
                if (t > ambientTemp) {
                    Vy[index] += dt * ((t - ambientTemp) * buoyancyBeta - d * buoyancyAlpha);
                } else if (d > 0.0f) {
                    // Even without temp, smoke goes up
                    Vy[index] += dt * (d * buoyancyAlpha); 
                }
            }
        }
    }
}

void FluidSolver3D::applyWind() {
    int N = size;
    #pragma omp parallel for
    for (int z = 1; z < N - 1; z++) {
        for (int j = 1; j < N - 1; j++) {
            for (int i = 1; i < N - 1; i++) {
                int index = IX(i, j, z);
                Vx[index] += dt * wind.x;
                Vy[index] += dt * wind.y;
                Vz[index] += dt * wind.z;
            }
        }
    }
}

void FluidSolver3D::coolAndDissipate() {
    int Ncells = (int)temp.size();
    int N = size;
    #pragma omp parallel for
    for (int i = 0; i < Ncells; i++) {
        int y = (i / N) % N;
        float h = (float)y / (float)std::max(1, N - 1);
        float top = std::clamp((h - 0.72f) / 0.28f, 0.0f, 1.0f);
        float topDamp = 1.0f - 0.22f * (top * top);

        temp[(size_t)i] *= (cooling * topDamp);
        density[(size_t)i] *= (densityDissipation * topDamp);
        
        // Prevent values from growing to infinity or dropping below 0
        if (temp[(size_t)i] < 0.0001f) temp[(size_t)i] = 0.0f;
        if (temp[(size_t)i] > 10.0f) temp[(size_t)i] = 10.0f;
        
        if (density[(size_t)i] < 0.0001f) density[(size_t)i] = 0.0f;
        if (density[(size_t)i] > 10.0f) density[(size_t)i] = 10.0f;
    }
}

void FluidSolver3D::step() {
    // 1. Add Forces (Buoyancy, Wind)
    applyBuoyancy();
    applyWind();

    // 2. Diffuse Velocity (usually omitted for real-time fire as numerical dissipation is high enough, but included for completeness)
    // Using diff=0 and visc=0 usually to save performance, but we can do it if visc > 0
    std::copy(Vx.begin(), Vx.end(), Vx0.begin());
    std::copy(Vy.begin(), Vy.end(), Vy0.begin());
    std::copy(Vz.begin(), Vz.end(), Vz0.begin());

    if (visc > 0.0f) {
        diffuse(1, Vx, Vx0, visc);
        diffuse(2, Vy, Vy0, visc);
        diffuse(3, Vz, Vz0, visc);
    } else {
        std::copy(Vx0.begin(), Vx0.end(), Vx.begin());
        std::copy(Vy0.begin(), Vy0.end(), Vy.begin());
        std::copy(Vz0.begin(), Vz0.end(), Vz.begin());
    }

    project(Vx, Vy, Vz, Vx0, Vy0, pressureIterations);
    // 3. Advect Velocity
    std::copy(Vx.begin(), Vx.end(), Vx0.begin());
    std::copy(Vy.begin(), Vy.end(), Vy0.begin());
    std::copy(Vz.begin(), Vz.end(), Vz0.begin());

    if (useMacCormack) {
        advectMacCormack(1, Vx, Vx0, Vx0, Vy0, Vz0);
        advectMacCormack(2, Vy, Vy0, Vx0, Vy0, Vz0);
        advectMacCormack(3, Vz, Vz0, Vx0, Vy0, Vz0);
    } else {
        advect(1, Vx, Vx0, Vx0, Vy0, Vz0);
        advect(2, Vy, Vy0, Vx0, Vy0, Vz0);
        advect(3, Vz, Vz0, Vx0, Vy0, Vz0);
    }

    project(Vx, Vy, Vz, Vx0, Vy0, pressureIterations);

    // 4. Diffuse and Advect Scalars (Temperature, Density)
    std::copy(temp.begin(), temp.end(), temp0.begin());
    std::copy(density.begin(), density.end(), density0.begin());

    if (diff > 0.0f) {
        diffuse(0, temp, temp0, diff);
        diffuse(0, density, density0, diff);
        std::copy(temp.begin(), temp.end(), temp0.begin());
        std::copy(density.begin(), density.end(), density0.begin());
    } else {
        std::copy(temp0.begin(), temp0.end(), temp.begin());
        std::copy(density0.begin(), density0.end(), density.begin());
    }

    if (useMacCormack) {
        advectMacCormack(0, temp, temp0, Vx, Vy, Vz);
        advectMacCormack(0, density, density0, Vx, Vy, Vz);
    } else {
        advect(0, temp, temp0, Vx, Vy, Vz);
        advect(0, density, density0, Vx, Vy, Vz);
    }

    // 5. Cool and Dissipate
    coolAndDissipate();
}
