#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "FluidSolver3D.h"
#include "shader.h"

class VolumeRenderer {
public:
    VolumeRenderer(FluidSolver3D* solver);
    ~VolumeRenderer();

    void init();
    void updateTextures();
    void render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, const glm::vec3& volumePos, float volumeScale);

    void setDensityScale(float scale) { densityScale = scale; }
    void setTemperatureScale(float scale) { temperatureScale = scale; }
    void setStepSize(float size) { stepSize = size; }

private:
    FluidSolver3D* solver;
    shader volumeShader;
    
    GLuint densityTexture;
    GLuint tempTexture;

    GLuint vao, vbo;

    float densityScale = 5.0f;
    float temperatureScale = 2.0f;
    float stepSize = 0.01f;

    void setupCube();
};
