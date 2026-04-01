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
    void render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, const glm::vec3& volumePos, float volumeScale, float timeSeconds);

    void setDensityScale(float scale) { densityScale = scale; }
    void setTemperatureScale(float scale) { temperatureScale = scale; }
    void setStepSize(float size) { stepSize = size; }
    void setMaxSteps(int steps) { maxSteps = steps < 8 ? 8 : (steps > 512 ? 512 : steps); }
    void setEmptySpaceSkip(float mult) { emptySpaceSkip = mult; }
    void setEmptyThreshold(float t) { emptyThreshold = t; }
    void setExposure(float e) { exposure = e; }
    void setFireIntensity(float v) { fireIntensity = v; }
    void setNoiseScale(float v) { noiseScale = v; }
    void setNoiseStrength(float v) { noiseStrength = v; }

private:
    FluidSolver3D* solver;
    shader volumeShader;
    
    GLuint densityTexture;
    GLuint tempTexture;

    GLuint vao, vbo;

    float densityScale = 5.0f;
    float temperatureScale = 2.0f;
    float stepSize = 0.01f;
    int maxSteps = 96;
    float emptySpaceSkip = 4.0f;
    float emptyThreshold = 0.01f;
    float exposure = 1.25f;
    float fireIntensity = 8.0f;
    float noiseScale = 7.0f;
    float noiseStrength = 0.35f;

    GLint locModel = -1;
    GLint locView = -1;
    GLint locProj = -1;
    GLint locCameraPos = -1;
    GLint locVolumeMin = -1;
    GLint locVolumeMax = -1;
    GLint locStepSize = -1;
    GLint locDensityScale = -1;
    GLint locTempScale = -1;
    GLint locMaxSteps = -1;
    GLint locEmptySpaceSkip = -1;
    GLint locEmptyThreshold = -1;
    GLint locTime = -1;
    GLint locExposure = -1;
    GLint locFireIntensity = -1;
    GLint locNoiseScale = -1;
    GLint locNoiseStrength = -1;
    GLint locDensityTex = -1;
    GLint locTempTex = -1;

    void setupCube();
};
