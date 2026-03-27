#include "VolumeRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const char* volumeVertexShaderSource = R"(
#version 330 core
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

const char* volumeFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;

uniform vec3 cameraPos;
uniform vec3 volumeMin;
uniform vec3 volumeMax;
uniform float stepSize;
uniform float densityScale;
uniform float tempScale;

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

// Simple color ramp for fire based on temperature
vec3 getFireColor(float temp) {
    vec3 color1 = vec3(0.0, 0.0, 0.0);       // Black
    vec3 color2 = vec3(1.0, 0.1, 0.0);       // Red
    vec3 color3 = vec3(1.0, 0.6, 0.0);       // Orange
    vec3 color4 = vec3(1.0, 1.0, 0.6);       // Yellow/White

    float t = temp * tempScale;
    
    if (t < 0.1) return mix(color1, color2, t / 0.1);
    if (t < 0.5) return mix(color2, color3, (t - 0.1) / 0.4);
    return mix(color3, color4, clamp((t - 0.5) / 0.5, 0.0, 1.0));
}

void main() {
    vec3 rayDir = normalize(WorldPos - cameraPos);
    vec2 tBounds = intersectAABB(cameraPos, rayDir, volumeMin, volumeMax);
    
    if (tBounds.x >= tBounds.y || tBounds.y < 0.0) {
        discard;
    }

    float t = max(tBounds.x, 0.0);
    float tMax = tBounds.y;

    vec4 accumColor = vec4(0.0);

    vec3 boxSize = volumeMax - volumeMin;

    // Raymarching loop
    int maxSteps = 256;
    float currentStepSize = length(boxSize) / float(maxSteps);

    for (int i = 0; i < maxSteps; i++) {
        if (t >= tMax || accumColor.a >= 0.99) break;

        vec3 p = cameraPos + t * rayDir;
        // Map world position to 3D texture coordinates [0, 1]
        vec3 texCoords = (p - volumeMin) / boxSize;

        float density = texture(densityTex, texCoords).r;
        float temp = texture(tempTex, texCoords).r;

        // Smoke
        float smokeAlpha = clamp(density * densityScale * currentStepSize, 0.0, 1.0);
        vec4 smokeColor = vec4(vec3(0.2), smokeAlpha); // Dark grey smoke

        // Fire
        vec3 fireRGB = getFireColor(temp);
        float fireAlpha = clamp(temp * tempScale, 0.0, 1.0) * currentStepSize * 25.0; // Adjusted multiplier
        vec4 fireColor = vec4(fireRGB, fireAlpha);

        // Blend fire and smoke (fire emits light, smoke absorbs)
        // We use pre-multiplied alpha for accumulation
        vec4 sampleColor;
        sampleColor.rgb = fireColor.rgb * fireColor.a + smokeColor.rgb * smokeColor.a * (1.0 - fireColor.a);
        sampleColor.a = fireColor.a + smokeColor.a * (1.0 - fireColor.a);

        // Front-to-back blending
        accumColor.rgb += (1.0 - accumColor.a) * sampleColor.rgb;
        accumColor.a += (1.0 - accumColor.a) * sampleColor.a;

        t += currentStepSize;
    }

    if (accumColor.a < 0.001) {
        discard;
    }

    FragColor = accumColor;
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

    int N = solver->getSize();

    glGenTextures(1, &densityTexture);
    glBindTexture(GL_TEXTURE_3D, densityTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, N, N, N, 0, GL_RED, GL_FLOAT, nullptr);

    glGenTextures(1, &tempTexture);
    glBindTexture(GL_TEXTURE_3D, tempTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, N, N, N, 0, GL_RED, GL_FLOAT, nullptr);

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

void VolumeRenderer::render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, const glm::vec3& volumePos, float volumeScale) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Premultiplied alpha blending
    glDisable(GL_CULL_FACE);

    volumeShader.use();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, volumePos);
    model = glm::scale(model, glm::vec3(volumeScale));

    glUniformMatrix4fv(glGetUniformLocation(volumeShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(volumeShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(volumeShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

    glUniform3fv(glGetUniformLocation(volumeShader.ID, "cameraPos"), 1, glm::value_ptr(cameraPos));
    
    glm::vec3 minBounds = volumePos - glm::vec3(volumeScale * 0.5f);
    glm::vec3 maxBounds = volumePos + glm::vec3(volumeScale * 0.5f);
    
    glUniform3fv(glGetUniformLocation(volumeShader.ID, "volumeMin"), 1, glm::value_ptr(minBounds));
    glUniform3fv(glGetUniformLocation(volumeShader.ID, "volumeMax"), 1, glm::value_ptr(maxBounds));
    
    glUniform1f(glGetUniformLocation(volumeShader.ID, "stepSize"), stepSize);
    glUniform1f(glGetUniformLocation(volumeShader.ID, "densityScale"), densityScale);
    glUniform1f(glGetUniformLocation(volumeShader.ID, "tempScale"), temperatureScale);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, densityTexture);
    glUniform1i(glGetUniformLocation(volumeShader.ID, "densityTex"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, tempTexture);
    glUniform1i(glGetUniformLocation(volumeShader.ID, "tempTex"), 1);

    glBindVertexArray(vao);
    
    // Disable depth write for transparent volume rendering
    glDepthMask(GL_FALSE);
    
    glDrawArrays(GL_TRIANGLES, 0, 36);
    
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
}
