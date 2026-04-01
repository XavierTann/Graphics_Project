#include "VolumeRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef __APPLE__
#define VOLUME_GLSL_VERSION "#version 410 core\n"
#else
#define VOLUME_GLSL_VERSION "#version 430 core\n"
#endif

const char* volumeVertexShaderSource = VOLUME_GLSL_VERSION R"(
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 WorldPos;

void main() {
    WorldPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)";

const char* volumeFragmentShaderSource = VOLUME_GLSL_VERSION R"(
out vec4 FragColor;

in vec3 WorldPos;

uniform vec3 cameraPos;
uniform vec3 volumeMin;
uniform vec3 volumeMax;
uniform float stepSize;
uniform float densityScale;
uniform float tempScale;
uniform int maxSteps;
uniform float emptySpaceSkip;
uniform float emptyThreshold;
uniform float uTime;
uniform float exposure;
uniform float fireIntensity;
uniform float noiseScale;
uniform float noiseStrength;

uniform sampler3D densityTex;
uniform sampler3D tempTex;

vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - rayOrigin) / rayDir;
    vec3 tMax = (boxMax - rayOrigin) / rayDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return vec2(tNear, tFar);
}

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i + vec3(0, 0, 0));
    float n100 = hash13(i + vec3(1, 0, 0));
    float n010 = hash13(i + vec3(0, 1, 0));
    float n110 = hash13(i + vec3(1, 1, 0));
    float n001 = hash13(i + vec3(0, 0, 1));
    float n101 = hash13(i + vec3(1, 0, 1));
    float n011 = hash13(i + vec3(0, 1, 1));
    float n111 = hash13(i + vec3(1, 1, 1));
    float n00 = mix(n000, n100, f.x);
    float n10 = mix(n010, n110, f.x);
    float n01 = mix(n001, n101, f.x);
    float n11 = mix(n011, n111, f.x);
    float n0 = mix(n00, n10, f.y);
    float n1 = mix(n01, n11, f.y);
    return mix(n0, n1, f.z);
}

float fbm(vec3 p) {
    float sum = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 4; i++) {
        sum += amp * noise3(p * freq);
        freq *= 2.0;
        amp *= 0.5;
    }
    return sum;
}

vec3 blackbody(float kelvin) {
    float t = kelvin / 100.0;
    float r;
    float g;
    float b;
    if (t <= 66.0) {
        r = 1.0;
        g = clamp(0.3900815787690196 * log(t) - 0.6318414437886275, 0.0, 1.0);
    } else {
        float tt = t - 60.0;
        r = clamp(1.292936186062745 * pow(tt, -0.1332047592), 0.0, 1.0);
        g = clamp(1.129890860895294 * pow(tt, -0.0755148492), 0.0, 1.0);
    }
    if (t >= 66.0) {
        b = 1.0;
    } else if (t <= 19.0) {
        b = 0.0;
    } else {
        b = clamp(0.5432067891101961 * log(t - 10.0) - 1.19625408914, 0.0, 1.0);
    }
    return vec3(r, g, b);
}

void main() {
    vec3 rayDir = normalize(WorldPos - cameraPos);
    vec2 tBounds = intersectAABB(cameraPos, rayDir, volumeMin, volumeMax);
    
    if (tBounds.x >= tBounds.y || tBounds.y < 0.0) {
        discard;
    }

    float t = max(tBounds.x, 0.0);
    float tMax = tBounds.y;

    float Tr = 1.0;
    vec3 accumRGB = vec3(0.0);

    vec3 boxSize = volumeMax - volumeMin;

    float currentStepSize = stepSize;
    if (currentStepSize <= 0.0) {
        currentStepSize = length(boxSize) / float(maxSteps);
    }

    for (int i = 0; i < maxSteps; i++) {
        if (t >= tMax || Tr <= 0.01) break;

        vec3 p = cameraPos + t * rayDir;
        // Map world position to 3D texture coordinates [0, 1]
        vec3 texCoords = (p - volumeMin) / boxSize;

        float density = texture(densityTex, texCoords).r;
        float temp = texture(tempTex, texCoords).r;

        float n = fbm(texCoords * noiseScale + vec3(0.0, uTime * 0.25, 0.0));
        float nMul = mix(1.0 - noiseStrength, 1.0 + noiseStrength, n);
        density *= nMul;
        temp *= nMul;

        if (density < emptyThreshold && temp < emptyThreshold) {
            t += currentStepSize * max(1.0, emptySpaceSkip);
            continue;
        }

        float tempN = clamp(temp * tempScale, 0.0, 1.0);
        float sigmaS = max(0.0, density * densityScale);
        float sigmaE = tempN * fireIntensity;
        vec3 emitColor = blackbody(mix(800.0, 2400.0, tempN));
        vec3 emission = emitColor * sigmaE;

        float extinction = sigmaS + 0.15 * sigmaE;
        float segTr = exp(-extinction * currentStepSize);
        vec3 segCol = emission * (1.0 - segTr) / max(extinction, 1e-4);
        accumRGB += Tr * segCol;
        Tr *= segTr;

        t += currentStepSize;
    }

    vec3 outRGB = vec3(1.0) - exp(-accumRGB * max(0.0, exposure));
    float outA = 1.0 - Tr;

    if (outA < 0.001) {
        discard;
    }

    float a = clamp(outA, 0.0, 1.0);
    FragColor = vec4(outRGB * a, a);
}
)";

VolumeRenderer::VolumeRenderer(FluidSolver3D* solver) : solver(solver) {
    vao = 0;
    vbo = 0;
    densityTexture = 0;
    tempTexture = 0;
}

VolumeRenderer::~VolumeRenderer() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (densityTexture) glDeleteTextures(1, &densityTexture);
    if (tempTexture) glDeleteTextures(1, &tempTexture);
}

void VolumeRenderer::init() {
    volumeShader.setUpShader(volumeVertexShaderSource, volumeFragmentShaderSource);

    locModel = glGetUniformLocation(volumeShader.ID, "model");
    locView = glGetUniformLocation(volumeShader.ID, "view");
    locProj = glGetUniformLocation(volumeShader.ID, "projection");
    locCameraPos = glGetUniformLocation(volumeShader.ID, "cameraPos");
    locVolumeMin = glGetUniformLocation(volumeShader.ID, "volumeMin");
    locVolumeMax = glGetUniformLocation(volumeShader.ID, "volumeMax");
    locStepSize = glGetUniformLocation(volumeShader.ID, "stepSize");
    locDensityScale = glGetUniformLocation(volumeShader.ID, "densityScale");
    locTempScale = glGetUniformLocation(volumeShader.ID, "tempScale");
    locMaxSteps = glGetUniformLocation(volumeShader.ID, "maxSteps");
    locEmptySpaceSkip = glGetUniformLocation(volumeShader.ID, "emptySpaceSkip");
    locEmptyThreshold = glGetUniformLocation(volumeShader.ID, "emptyThreshold");
    locTime = glGetUniformLocation(volumeShader.ID, "uTime");
    locExposure = glGetUniformLocation(volumeShader.ID, "exposure");
    locFireIntensity = glGetUniformLocation(volumeShader.ID, "fireIntensity");
    locNoiseScale = glGetUniformLocation(volumeShader.ID, "noiseScale");
    locNoiseStrength = glGetUniformLocation(volumeShader.ID, "noiseStrength");
    locDensityTex = glGetUniformLocation(volumeShader.ID, "densityTex");
    locTempTex = glGetUniformLocation(volumeShader.ID, "tempTex");

    volumeShader.use();
    if (locDensityTex >= 0) glUniform1i(locDensityTex, 0);
    if (locTempTex >= 0) glUniform1i(locTempTex, 1);

    int N = solver->getSize();

    glGenTextures(1, &densityTexture);
    glBindTexture(GL_TEXTURE_3D, densityTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R16F, N, N, N, 0, GL_RED, GL_FLOAT, nullptr);

    glGenTextures(1, &tempTexture);
    glBindTexture(GL_TEXTURE_3D, tempTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R16F, N, N, N, 0, GL_RED, GL_FLOAT, nullptr);

    setupCube();
}

void VolumeRenderer::updateTextures() {
    int N = solver->getSize();
    
    glBindTexture(GL_TEXTURE_3D, densityTexture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, N, N, N, GL_RED, GL_FLOAT, solver->getDensity());

    glBindTexture(GL_TEXTURE_3D, tempTexture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, N, N, N, GL_RED, GL_FLOAT, solver->getTemperature());
}

void VolumeRenderer::setupCube() {
    float vertices[] = {
        // Front face
        0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Back face
        0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        // Left face
        0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        // Right face
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        // Top face
        0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        // Bottom face
        0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 0.0f
    };

    // Center the cube around origin for easier scaling
    for (int i = 0; i < sizeof(vertices) / sizeof(float); i++) {
        vertices[i] -= 0.5f;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void VolumeRenderer::render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, const glm::vec3& volumePos, float volumeScale, float timeSeconds) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Premultiplied alpha blending
    glDisable(GL_CULL_FACE);

    volumeShader.use();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, volumePos);
    model = glm::scale(model, glm::vec3(volumeScale));

    if (locModel >= 0) glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
    if (locView >= 0) glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));
    if (locProj >= 0) glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));
    if (locCameraPos >= 0) glUniform3fv(locCameraPos, 1, glm::value_ptr(cameraPos));
    
    glm::vec3 minBounds = volumePos - glm::vec3(volumeScale * 0.5f);
    glm::vec3 maxBounds = volumePos + glm::vec3(volumeScale * 0.5f);
    
    if (locVolumeMin >= 0) glUniform3fv(locVolumeMin, 1, glm::value_ptr(minBounds));
    if (locVolumeMax >= 0) glUniform3fv(locVolumeMax, 1, glm::value_ptr(maxBounds));
    
    if (locStepSize >= 0) glUniform1f(locStepSize, stepSize);
    if (locDensityScale >= 0) glUniform1f(locDensityScale, densityScale);
    if (locTempScale >= 0) glUniform1f(locTempScale, temperatureScale);
    if (locMaxSteps >= 0) glUniform1i(locMaxSteps, maxSteps);
    if (locEmptySpaceSkip >= 0) glUniform1f(locEmptySpaceSkip, emptySpaceSkip);
    if (locEmptyThreshold >= 0) glUniform1f(locEmptyThreshold, emptyThreshold);
    if (locTime >= 0) glUniform1f(locTime, timeSeconds);
    if (locExposure >= 0) glUniform1f(locExposure, exposure);
    if (locFireIntensity >= 0) glUniform1f(locFireIntensity, fireIntensity);
    if (locNoiseScale >= 0) glUniform1f(locNoiseScale, noiseScale);
    if (locNoiseStrength >= 0) glUniform1f(locNoiseStrength, noiseStrength);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, densityTexture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, tempTexture);

    glBindVertexArray(vao);
    
    // Disable depth write for transparent volume rendering
    glDepthMask(GL_FALSE);
    
    glDrawArrays(GL_TRIANGLES, 0, 36);
    
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
}
