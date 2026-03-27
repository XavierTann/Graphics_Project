#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

class FluidSystem {
public:
    FluidSystem();
    ~FluidSystem();

    void init(int width, int height);
    void update(float dt, float buoyancy, float weight, float cooling, float dissipation);
    void injectDensity(glm::vec2 pos, float radius, float density, float temperature);
    void injectVelocity(glm::vec2 pos, float radius, glm::vec2 velocity);

    GLuint getDensityTexture() const { return densityTex[0]; }
    GLuint getTemperatureTexture() const { return temperatureTex[0]; }
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    int width, height;

    // Textures for ping-pong
    GLuint velocityTex[2];
    GLuint densityTex[2];
    GLuint temperatureTex[2];
    GLuint pressureTex[2];
    GLuint divergenceTex;

    // Compute Shaders
    GLuint advectShader;
    GLuint advectColorShader;
    GLuint buoyancyShader;
    GLuint injectColorShader;
    GLuint injectVelocityShader;
    GLuint divergenceShader;
    GLuint jacobiShader;
    GLuint subtractGradientShader;
    GLuint clearShader;

    GLuint compileComputeShader(const char* source);
    void createTexture(GLuint& tex, int w, int h, GLint internalFormat, GLenum format, GLenum type);
    void swapTextures(GLuint* texArray);
};
