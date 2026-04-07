#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "BillboardRenderer.h"
#include "MeshLoader.h"
#include "shader.h"
#include "SceneObject.h"

struct BillboardLighting {
    glm::vec3 cameraPos = glm::vec3(0.0f);
    glm::vec3 fireLightPos = glm::vec3(0.0f);
    glm::vec3 fireLightColor = glm::vec3(1.0f, 0.5f, 0.2f);
    float fireLightIntensity = 0.0f;
    float fireLightRange = 1.0f;
    float time = 0.0f;
};

// Renderer owns every draw call and all GPU-side helper state
// (grid VAO, wind VAO, point VAO, mesh shader, billboard renderer).
class Renderer {
public:
    // Allocate GPU resources. Call once after a valid GL context exists.
    void init();

    // Free all GPU resources.
    void shutdown();

    // ---- Per-frame draw calls ----

    // Draw the floor grid.
    void drawGrid(const glm::mat4& view, const glm::mat4& proj);

    // Draw a single green dot at 'pos' (emitter origin indicator).
    void drawMarkerPoint(const glm::mat4& view, const glm::mat4& proj,
        const glm::vec3& pos, const glm::vec4& color, float size);

    // Draw a cyan arrow showing wind direction.
    void drawWindArrow(const glm::mat4& view, const glm::mat4& proj,
        const glm::vec3& windVec, const glm::vec3& origin);

    // Draw all scene-object meshes as solid coloured geometry.
    void drawMeshes(const glm::mat4& view, const glm::mat4& proj,
        const std::vector<SceneObject>& objects);

    // Upload + draw flame billboards (additive blend).
    void drawFlames(const std::vector<InstanceAttrib>& data,
        shader& flameShader,
        const glm::mat4& proj, const glm::mat4& view,
        const glm::vec3& camRight, const glm::vec3& camUp,
        const BillboardLighting& lighting);

    // Upload + draw smoke billboards (alpha blend).
    void drawSmoke(const std::vector<InstanceAttrib>& data,
        shader& smokeShader,
        const glm::mat4& proj, const glm::mat4& view,
        const glm::vec3& camRight, const glm::vec3& camUp,
        const BillboardLighting& lighting);

    // Expose mesh loader so UI can call scan() / get availableMeshes
    MeshLoader& meshLoader() { return meshLoader_; }

    void loadDecorationMesh(const std::string& path, const glm::vec3& pos, float scale);
    void drawDecorations(const glm::mat4& view, const glm::mat4& proj);




private:
    BillboardRenderer billboards_;
    MeshLoader        meshLoader_;

    // Grid
    GLuint gridVAO_ = 0;
    GLuint gridVBO_ = 0;
    int    gridVertexCount_ = 0;

    // Wind arrow
    GLuint windVAO_ = 0;
    GLuint windVBO_ = 0;

    // Origin point
    GLuint pointVAO_ = 0;
    GLuint pointVBO_ = 0;

    //grid
    GLuint axisVAO_ = 0;
    GLuint axisVBO_ = 0;

    // Lazily-compiled helper shaders (created once on first draw call)
    GLuint gridShader_ = 0;
    GLuint lineShader_ = 0;
    GLuint pointShader_ = 0;
    GLuint meshShader_ = 0;

    // Shader builders
    static GLuint compileSimpleShader(const char* vs, const char* fs);
    void ensureGridShader();
    void ensureLineShader();
    void ensurePointShader();
    void ensureMeshShader();

    // Billboard helper (sets uniforms then calls drawInstanced)
    void uploadAndDraw(const std::vector<InstanceAttrib>& data,
        shader& sh,
        const glm::mat4& proj, const glm::mat4& view,
        const glm::vec3& camRight, const glm::vec3& camUp,
        const BillboardLighting& lighting);

    struct DecorationEntry {
        std::string   meshFile;
        glm::vec3     pos;
        float         scale;
    };
    std::vector<DecorationEntry> decorations_;
};
