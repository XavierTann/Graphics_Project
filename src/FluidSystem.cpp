#include "FluidSystem.h"
#include <iostream>
#include <vector>

// --- Compute Shader Sources ---

static const char* csAdvect = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D uVelocity;
layout(binding = 1) uniform sampler2D uSource;
layout(rgba16f, binding = 2) uniform image2D uDest;

uniform float dt;
uniform float dissipation;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(uVelocity, 0);
    if (coord.x >= size.x || coord.y >= size.y) return;

    vec2 pos = vec2(coord) + 0.5;
    vec2 vel = texelFetch(uVelocity, coord, 0).xy;
    
    // Trace back
    vec2 pastPos = pos - vel * dt;
    vec2 tc = pastPos / vec2(size);
    
    vec4 result = texture(uSource, tc) * dissipation;
    imageStore(uDest, coord, result);
}
)";

static const char* csBuoyancy = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D uVelocity;
layout(binding = 1) uniform sampler2D uTemperature;
layout(binding = 2) uniform sampler2D uDensity;
layout(rgba16f, binding = 3) uniform image2D uDest;

uniform float dt;
uniform float buoyancy;
uniform float weight;
uniform float ambientTemperature = 0.0;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(uVelocity, 0);
    if (coord.x >= size.x || coord.y >= size.y) return;

    float T = texelFetch(uTemperature, coord, 0).x;
    float D = texelFetch(uDensity, coord, 0).x;
    vec2 V = texelFetch(uVelocity, coord, 0).xy;

    if (T > ambientTemperature) {
        float force = (T - ambientTemperature) * buoyancy - D * weight;
        V.y += force * dt;
    }
    
    imageStore(uDest, coord, vec4(V, 0.0, 0.0));
}
)";

static const char* csInject = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba16f, binding = 0) uniform image2D uDest;

uniform vec2 injectPos;
uniform float injectRadius;
uniform vec4 injectValue; // could be color/density/temp or velocity

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uDest);
    if (coord.x >= size.x || coord.y >= size.y) return;

    vec2 pos = vec2(coord) + 0.5;
    float dist = distance(pos, injectPos);
    
    if (dist < injectRadius) {
        float falloff = 1.0 - (dist / injectRadius);
        vec4 oldVal = imageLoad(uDest, coord);
        imageStore(uDest, coord, oldVal + injectValue * falloff);
    }
}
)";

static const char* csDivergence = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D uVelocity;
layout(r16f, binding = 1) uniform image2D uDest;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(uVelocity, 0);
    if (coord.x >= size.x || coord.y >= size.y) return;

    float L = texelFetch(uVelocity, ivec2(max(0, coord.x - 1), coord.y), 0).x;
    float R = texelFetch(uVelocity, ivec2(min(size.x - 1, coord.x + 1), coord.y), 0).x;
    float B = texelFetch(uVelocity, ivec2(coord.x, max(0, coord.y - 1)), 0).y;
    float T = texelFetch(uVelocity, ivec2(coord.x, min(size.y - 1, coord.y + 1)), 0).y;

    float div = 0.5 * ((R - L) + (T - B));
    imageStore(uDest, coord, vec4(div, 0.0, 0.0, 0.0));
}
)";

static const char* csJacobi = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D uPressure;
layout(binding = 1) uniform sampler2D uDivergence;
layout(r16f, binding = 2) uniform image2D uDest;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(uPressure, 0);
    if (coord.x >= size.x || coord.y >= size.y) return;

    float L = texelFetch(uPressure, ivec2(max(0, coord.x - 1), coord.y), 0).x;
    float R = texelFetch(uPressure, ivec2(min(size.x - 1, coord.x + 1), coord.y), 0).x;
    float B = texelFetch(uPressure, ivec2(coord.x, max(0, coord.y - 1)), 0).x;
    float T = texelFetch(uPressure, ivec2(coord.x, min(size.y - 1, coord.y + 1)), 0).x;
    
    float bC = texelFetch(uDivergence, coord, 0).x;

    float pNew = (L + R + B + T - bC) * 0.25;
    imageStore(uDest, coord, vec4(pNew, 0.0, 0.0, 0.0));
}
)";

static const char* csSubtractGradient = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D uVelocity;
layout(binding = 1) uniform sampler2D uPressure;
layout(rgba16f, binding = 2) uniform image2D uDest;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(uVelocity, 0);
    if (coord.x >= size.x || coord.y >= size.y) return;

    float L = texelFetch(uPressure, ivec2(max(0, coord.x - 1), coord.y), 0).x;
    float R = texelFetch(uPressure, ivec2(min(size.x - 1, coord.x + 1), coord.y), 0).x;
    float B = texelFetch(uPressure, ivec2(coord.x, max(0, coord.y - 1)), 0).x;
    float T = texelFetch(uPressure, ivec2(coord.x, min(size.y - 1, coord.y + 1)), 0).x;

    vec2 V = texelFetch(uVelocity, coord, 0).xy;
    V.xy -= vec2(R - L, T - B) * 0.5;

    imageStore(uDest, coord, vec4(V, 0.0, 0.0, 0.0));
}
)";

static const char* csClear = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(r16f, binding = 0) uniform image2D uDest;
void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uDest);
    if (coord.x >= size.x || coord.y >= size.y) return;
    imageStore(uDest, coord, vec4(0.0));
}
)";

// --- FluidSystem Implementation ---

FluidSystem::FluidSystem() {
    width = 0;
    height = 0;
}

FluidSystem::~FluidSystem() {
    // Delete textures
    glDeleteTextures(2, velocityTex);
    glDeleteTextures(2, densityTex);
    glDeleteTextures(2, temperatureTex);
    glDeleteTextures(2, pressureTex);
    glDeleteTextures(1, &divergenceTex);

    // Delete shaders
    glDeleteProgram(advectShader);
    glDeleteProgram(buoyancyShader);
    glDeleteProgram(injectColorShader);
    glDeleteProgram(divergenceShader);
    glDeleteProgram(jacobiShader);
    glDeleteProgram(subtractGradientShader);
    glDeleteProgram(clearShader);
}

GLuint FluidSystem::compileComputeShader(const char* source) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Compute Shader Compilation Error:\n" << infoLog << std::endl;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);
    
    return program;
}

void FluidSystem::createTexture(GLuint& tex, int w, int h, GLint internalFormat, GLenum format, GLenum type) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void FluidSystem::init(int w, int h) {
    width = w;
    height = h;

    createTexture(velocityTex[0], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    createTexture(velocityTex[1], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    
    createTexture(densityTex[0], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    createTexture(densityTex[1], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);

    createTexture(temperatureTex[0], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    createTexture(temperatureTex[1], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);

    createTexture(pressureTex[0], w, h, GL_R16F, GL_RED, GL_FLOAT);
    createTexture(pressureTex[1], w, h, GL_R16F, GL_RED, GL_FLOAT);

    createTexture(divergenceTex, w, h, GL_R16F, GL_RED, GL_FLOAT);

    advectShader = compileComputeShader(csAdvect);
    buoyancyShader = compileComputeShader(csBuoyancy);
    injectColorShader = compileComputeShader(csInject);
    divergenceShader = compileComputeShader(csDivergence);
    jacobiShader = compileComputeShader(csJacobi);
    subtractGradientShader = compileComputeShader(csSubtractGradient);
    clearShader = compileComputeShader(csClear);
}

void FluidSystem::swapTextures(GLuint* texArray) {
    GLuint temp = texArray[0];
    texArray[0] = texArray[1];
    texArray[1] = temp;
}

void FluidSystem::injectDensity(glm::vec2 pos, float radius, float density, float temperature) {
    glUseProgram(injectColorShader);
    
    glUniform2f(glGetUniformLocation(injectColorShader, "injectPos"), pos.x, pos.y);
    glUniform1f(glGetUniformLocation(injectColorShader, "injectRadius"), radius);
    
    // Inject Density
    glBindImageTexture(0, densityTex[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
    glUniform4f(glGetUniformLocation(injectColorShader, "injectValue"), density, 0.0f, 0.0f, 0.0f);
    glDispatchCompute((width + 15) / 16, (height + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Inject Temperature
    glBindImageTexture(0, temperatureTex[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
    glUniform4f(glGetUniformLocation(injectColorShader, "injectValue"), temperature, 0.0f, 0.0f, 0.0f);
    glDispatchCompute((width + 15) / 16, (height + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void FluidSystem::injectVelocity(glm::vec2 pos, float radius, glm::vec2 velocity) {
    glUseProgram(injectColorShader);
    glUniform2f(glGetUniformLocation(injectColorShader, "injectPos"), pos.x, pos.y);
    glUniform1f(glGetUniformLocation(injectColorShader, "injectRadius"), radius);
    
    glBindImageTexture(0, velocityTex[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
    glUniform4f(glGetUniformLocation(injectColorShader, "injectValue"), velocity.x, velocity.y, 0.0f, 0.0f);
    glDispatchCompute((width + 15) / 16, (height + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void FluidSystem::update(float dt, float buoyancy, float weight, float cooling, float dissipation) {
    int groupsX = (width + 15) / 16;
    int groupsY = (height + 15) / 16;

    // 1. Advect Velocity
    glUseProgram(advectShader);
    glUniform1f(glGetUniformLocation(advectShader, "dt"), dt);
    glUniform1f(glGetUniformLocation(advectShader, "dissipation"), 1.0f); // velocity doesn't dissipate here usually
    glUniform1i(glGetUniformLocation(advectShader, "uVelocity"), 0);
    glUniform1i(glGetUniformLocation(advectShader, "uSource"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, velocityTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, velocityTex[0]);
    glBindImageTexture(2, velocityTex[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    swapTextures(velocityTex);

    // 2. Advect Density
    glUseProgram(advectShader);
    glUniform1f(glGetUniformLocation(advectShader, "dt"), dt);
    glUniform1f(glGetUniformLocation(advectShader, "dissipation"), dissipation);
    glUniform1i(glGetUniformLocation(advectShader, "uVelocity"), 0);
    glUniform1i(glGetUniformLocation(advectShader, "uSource"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, velocityTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, densityTex[0]);
    glBindImageTexture(2, densityTex[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    swapTextures(densityTex);

    // 3. Advect Temperature
    glUseProgram(advectShader);
    glUniform1f(glGetUniformLocation(advectShader, "dt"), dt);
    glUniform1f(glGetUniformLocation(advectShader, "dissipation"), cooling);
    glUniform1i(glGetUniformLocation(advectShader, "uVelocity"), 0);
    glUniform1i(glGetUniformLocation(advectShader, "uSource"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, velocityTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, temperatureTex[0]);
    glBindImageTexture(2, temperatureTex[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    swapTextures(temperatureTex);

    // 4. Apply Buoyancy
    glUseProgram(buoyancyShader);
    glUniform1f(glGetUniformLocation(buoyancyShader, "dt"), dt);
    glUniform1f(glGetUniformLocation(buoyancyShader, "buoyancy"), buoyancy);
    glUniform1f(glGetUniformLocation(buoyancyShader, "weight"), weight);
    glUniform1i(glGetUniformLocation(buoyancyShader, "uVelocity"), 0);
    glUniform1i(glGetUniformLocation(buoyancyShader, "uTemperature"), 1);
    glUniform1i(glGetUniformLocation(buoyancyShader, "uDensity"), 2);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, velocityTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, temperatureTex[0]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, densityTex[0]);
    glBindImageTexture(3, velocityTex[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    swapTextures(velocityTex);

    // 5. Compute Divergence
    glUseProgram(divergenceShader);
    glUniform1i(glGetUniformLocation(divergenceShader, "uVelocity"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, velocityTex[0]);
    glBindImageTexture(1, divergenceTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Clear Pressure (compute-based, compatible with GL 4.3)
    glUseProgram(clearShader);
    glBindImageTexture(0, pressureTex[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 6. Jacobi Iteration (Pressure Solve)
    glUseProgram(jacobiShader);
    glUniform1i(glGetUniformLocation(jacobiShader, "uPressure"), 0);
    glUniform1i(glGetUniformLocation(jacobiShader, "uDivergence"), 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, divergenceTex);
    for (int i = 0; i < 40; ++i) { // 40 iterations is usually enough
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pressureTex[0]);
        glBindImageTexture(2, pressureTex[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        swapTextures(pressureTex);
    }

    // 7. Subtract Gradient
    glUseProgram(subtractGradientShader);
    glUniform1i(glGetUniformLocation(subtractGradientShader, "uVelocity"), 0);
    glUniform1i(glGetUniformLocation(subtractGradientShader, "uPressure"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, velocityTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pressureTex[0]);
    glBindImageTexture(2, velocityTex[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    swapTextures(velocityTex);
}
