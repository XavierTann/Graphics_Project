#include "Renderer.h"
#include "SceneObject.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstring>

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

void Renderer::init()
{
    billboards_.init();
    meshLoader_.scan("data");

    // ---- Grid ----
    std::vector<float> gridVerts;
    const float size = 10.0f;
    const int   slices = 20;
    const float step = size / slices;
    const float half = size * 0.5f;
    for (int i = 0; i <= slices; ++i) {
        float x = -half + i * step;
        gridVerts.insert(gridVerts.end(), { x, 0.0f, -half,  x, 0.0f, half });
        float z = -half + i * step;
        gridVerts.insert(gridVerts.end(), { -half, 0.0f, z,  half, 0.0f, z });
    }
    gridVertexCount_ = (int)gridVerts.size() / 3;

    glGenVertexArrays(1, &gridVAO_);
    glGenBuffers(1, &gridVBO_);
    glBindVertexArray(gridVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO_);
    glBufferData(GL_ARRAY_BUFFER, gridVerts.size() * sizeof(float),
        gridVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ---- Wind arrow (2 vertices, dynamic) ----
    glGenVertexArrays(1, &windVAO_);
    glGenBuffers(1, &windVBO_);
    glBindVertexArray(windVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, windVBO_);
    glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ---- Origin point (1 vertex, dynamic) ----
    glGenVertexArrays(1, &pointVAO_);
    glGenBuffers(1, &pointVBO_);
    glBindVertexArray(pointVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO_);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ---- Fluid Quad ----
    float quadVertices[] = {
        // pos        // tex
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
    };
    glGenVertexArrays(1, &fluidVAO_);
    glGenBuffers(1, &fluidVBO_);
    glBindVertexArray(fluidVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, fluidVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void Renderer::shutdown()
{
    meshLoader_.clear();
    if (gridVAO_) { glDeleteVertexArrays(1, &gridVAO_);  gridVAO_ = 0; }
    if (gridVBO_) { glDeleteBuffers(1, &gridVBO_);        gridVBO_ = 0; }
    if (windVAO_) { glDeleteVertexArrays(1, &windVAO_);  windVAO_ = 0; }
    if (windVBO_) { glDeleteBuffers(1, &windVBO_);        windVBO_ = 0; }
    if (pointVAO_) { glDeleteVertexArrays(1, &pointVAO_); pointVAO_ = 0; }
    if (pointVBO_) { glDeleteBuffers(1, &pointVBO_);       pointVBO_ = 0; }
    if (fluidVAO_) { glDeleteVertexArrays(1, &fluidVAO_); fluidVAO_ = 0; }
    if (fluidVBO_) { glDeleteBuffers(1, &fluidVBO_);       fluidVBO_ = 0; }
    if (gridShader_) { glDeleteProgram(gridShader_);  gridShader_ = 0; }
    if (lineShader_) { glDeleteProgram(lineShader_);  lineShader_ = 0; }
    if (pointShader_) { glDeleteProgram(pointShader_); pointShader_ = 0; }
    if (meshShader_) { glDeleteProgram(meshShader_);  meshShader_ = 0; }
    if (fluidShader_) { glDeleteProgram(fluidShader_); fluidShader_ = 0; }
}

// ---------------------------------------------------------------------------
// Shader helpers
// ---------------------------------------------------------------------------

GLuint Renderer::compileSimpleShader(const char* vs, const char* fs)
{
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        return s;
        };
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

void Renderer::ensureGridShader()
{
    if (gridShader_) return;
    const char* vs =
        "#version 330\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 MVP;\n"
        "void main(){ gl_Position = MVP * vec4(aPos,1.0); }\n";
    const char* fs =
        "#version 330\n"
        "out vec4 FragColor;\n"
        "void main(){ FragColor = vec4(0.0,0.5,0.0,0.5); }\n";
    gridShader_ = compileSimpleShader(vs, fs);
}

void Renderer::ensureLineShader()
{
    if (lineShader_) return;
    const char* vs =
        "#version 330\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 MVP;\n"
        "void main(){ gl_Position = MVP * vec4(aPos,1.0); }\n";
    const char* fs =
        "#version 330\n"
        "out vec4 FragColor;\n"
        "void main(){ FragColor = vec4(0.0,1.0,1.0,1.0); }\n"; // cyan
    lineShader_ = compileSimpleShader(vs, fs);
}

void Renderer::ensurePointShader()
{
    if (pointShader_) return;
    const char* vs =
        "#version 330\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 MVP;\n"
        "void main(){ gl_Position = MVP * vec4(aPos,1.0); gl_PointSize=10.0; }\n";
    const char* fs =
        "#version 330\n"
        "out vec4 FragColor;\n"
        "void main(){ FragColor = vec4(0.0,1.0,0.0,1.0); }\n"; // green
    pointShader_ = compileSimpleShader(vs, fs);
}

void Renderer::ensureMeshShader()
{
    if (meshShader_) return;
    const char* vs =
        "#version 330\n"
        "layout(location=0) in vec3 aPos;\n"
        "layout(location=1) in vec2 aUV;\n"
        "uniform mat4 MVP;\n"
        "out vec2 vUV;\n"
        "void main(){ vUV = aUV; gl_Position = MVP * vec4(aPos,1.0); }\n";
    const char* fs =
        "#version 330\n"
        "uniform vec4 uColor;\n"
        "uniform sampler2D uTex;\n"
        "uniform int uUseTex;\n"
        "in vec2 vUV;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    vec4 base = uColor;\n"
        "    if (uUseTex == 1) base *= texture(uTex, vUV);\n"
        "    FragColor = base;\n"
        "}\n";
    meshShader_ = compileSimpleShader(vs, fs);
}

// ---------------------------------------------------------------------------
// Draw calls
// ---------------------------------------------------------------------------

void Renderer::drawGrid(const glm::mat4& view, const glm::mat4& proj)
{
    ensureGridShader();
    glEnable(GL_BLEND);
    glUseProgram(gridShader_);
    glm::mat4 MVP = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(gridShader_, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glBindVertexArray(gridVAO_);
    glDrawArrays(GL_LINES, 0, gridVertexCount_);
    glBindVertexArray(0);
}

void Renderer::drawOriginPoint(const glm::mat4& view, const glm::mat4& proj,
    const glm::vec3& pos)
{
    ensurePointShader();
    float pt[3] = { pos.x, pos.y, pos.z };
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pt), pt);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(pointShader_);
    glm::mat4 MVP = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(pointShader_, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glBindVertexArray(pointVAO_);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void Renderer::drawWindArrow(const glm::mat4& view, const glm::mat4& proj,
    const glm::vec3& windVec, const glm::vec3& origin)
{
    if (glm::length(windVec) < 0.01f) return;
    ensureLineShader();

    glm::vec3 end = origin + windVec * 2.0f;
    float lines[6] = { origin.x, origin.y, origin.z, end.x, end.y, end.z };
    glBindBuffer(GL_ARRAY_BUFFER, windVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lines), lines);

    glUseProgram(lineShader_);
    glm::mat4 MVP = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(lineShader_, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glBindVertexArray(windVAO_);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

void Renderer::drawMeshes(const glm::mat4& view, const glm::mat4& proj,
    const std::vector<SceneObject>& objects)
{
    if (objects.empty()) return;
    ensureMeshShader();

    glUseProgram(meshShader_);
    int locMVP = glGetUniformLocation(meshShader_, "MVP");
    int locCol = glGetUniformLocation(meshShader_, "uColor");
    int locUseTex = glGetUniformLocation(meshShader_, "uUseTex");
    int locTex = glGetUniformLocation(meshShader_, "uTex");
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    if (locTex >= 0) glUniform1i(locTex, 0);

    for (const auto& obj : objects) {
        const GpuMesh* mesh = meshLoader_.get(obj.meshFile);
        if (!mesh || !mesh->valid) continue;

        glm::mat4 model = glm::scale(
            glm::translate(glm::mat4(1.0f), obj.pos),
            glm::vec3(obj.markerSize));
        glm::mat4 mvp = proj * view * model;

        glm::vec4 col;
        if (obj.ash >= 1.0f) col = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
        else if (obj.burning)     col = glm::vec4(1.0f, 0.45f, 0.05f, 1.0f);
        else if (mesh->textured)  col = glm::vec4(1.0f);
        else                      col = glm::vec4(0.1f, 0.8f, 0.2f, 1.0f);

        glUniformMatrix4fv(locMVP, 1, GL_FALSE, &mvp[0][0]);
        glUniform4fv(locCol, 1, &col[0]);
        if (mesh->textured && mesh->texture) {
            if (locUseTex >= 0) glUniform1i(locUseTex, 1);
            glBindTexture(GL_TEXTURE_2D, mesh->texture);
        } else {
            if (locUseTex >= 0) glUniform1i(locUseTex, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glBindVertexArray(mesh->vao);
        if (mesh->indexed)
            glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
        else
            glDrawArrays(GL_TRIANGLES, 0, mesh->indexCount);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_BLEND);
}

// ---------------------------------------------------------------------------
// Billboard helpers
// ---------------------------------------------------------------------------

void Renderer::uploadAndDraw(const std::vector<InstanceAttrib>& data,
    shader& sh,
    const glm::mat4& proj, const glm::mat4& view,
    const glm::vec3& camRight, const glm::vec3& camUp)
{
    billboards_.uploadInstances(data);
    sh.use();
    glUniformMatrix4fv(glGetUniformLocation(sh.ID, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(sh.ID, "view"), 1, GL_FALSE, &view[0][0]);
    glUniform3fv(glGetUniformLocation(sh.ID, "camRight"), 1, &camRight[0]);
    glUniform3fv(glGetUniformLocation(sh.ID, "camUp"), 1, &camUp[0]);
}

void Renderer::drawObjectBillboards(const std::vector<InstanceAttrib>& data,
    shader& smokeShader,
    const glm::mat4& proj, const glm::mat4& view,
    const glm::vec3& camRight, const glm::vec3& camUp)
{
    if (data.empty()) return;
    uploadAndDraw(data, smokeShader, proj, view, camRight, camUp);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    billboards_.drawInstanced((int)data.size());
    glDepthMask(GL_TRUE);
}

void Renderer::drawFlames(const std::vector<InstanceAttrib>& data,
    shader& flameShader,
    const glm::mat4& proj, const glm::mat4& view,
    const glm::vec3& camRight, const glm::vec3& camUp)
{
    if (data.empty()) return;
    uploadAndDraw(data, flameShader, proj, view, camRight, camUp);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive for fire glow
    glDepthMask(GL_FALSE);
    billboards_.drawInstanced((int)data.size());
    glDepthMask(GL_TRUE);
}

void Renderer::drawSmoke(const std::vector<InstanceAttrib>& data,
    shader& smokeShader,
    const glm::mat4& proj, const glm::mat4& view,
    const glm::vec3& camRight, const glm::vec3& camUp)
{
    if (data.empty()) return;
    uploadAndDraw(data, smokeShader, proj, view, camRight, camUp);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    billboards_.drawInstanced((int)data.size());
    glDepthMask(GL_TRUE);
}

void Renderer::drawFluid(GLuint densityTexture, GLuint temperatureTexture,
    const glm::mat4& proj, const glm::mat4& view,
    const glm::vec3& camRight, const glm::vec3& camUp,
    const glm::vec3& origin)
{
    if (!fluidShader_) {
        const char* vs = R"(
#version 330
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
uniform mat4 projection;
uniform mat4 view;
uniform vec3 camRight;
uniform vec3 camUp;
uniform vec3 origin;
uniform float scale;
out vec2 TexCoord;
void main() {
    vec3 worldPos = origin + camRight * aPos.x * scale + camUp * aPos.y * scale;
    gl_Position = projection * view * vec4(worldPos, 1.0);
    TexCoord = aTex;
}
)";
        const char* fs = R"(
#version 330
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uDensity;
uniform sampler2D uTemperature;

vec4 fireColor(float temp, float density) {
    float t = clamp(temp * 0.2, 0.0, 1.0);
    vec3 c1 = vec3(0.0);
    vec3 c2 = vec3(1.0, 0.1, 0.0);
    vec3 c3 = vec3(1.0, 0.5, 0.0);
    vec3 c4 = vec3(1.0, 0.9, 0.5);
    vec3 color = mix(c1, c2, clamp(t * 3.0, 0.0, 1.0));
    color = mix(color, c3, clamp((t - 0.33) * 3.0, 0.0, 1.0));
    color = mix(color, c4, clamp((t - 0.66) * 3.0, 0.0, 1.0));
    float alpha = 1.0 - exp(-density * 6.0);
    return vec4(color * 1.6, alpha);
}

void main() {
    float density = texture(uDensity, TexCoord).r;
    float temp = texture(uTemperature, TexCoord).r;
    
    vec4 col = fireColor(temp, density);
    if (col.a < 0.01) discard;
    FragColor = col;
}
)";
        fluidShader_ = compileSimpleShader(vs, fs);
    }

    glUseProgram(fluidShader_);
    glUniformMatrix4fv(glGetUniformLocation(fluidShader_, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(fluidShader_, "view"), 1, GL_FALSE, &view[0][0]);
    glUniform3fv(glGetUniformLocation(fluidShader_, "camRight"), 1, &camRight[0]);
    glUniform3fv(glGetUniformLocation(fluidShader_, "camUp"), 1, &camUp[0]);
    glUniform3fv(glGetUniformLocation(fluidShader_, "origin"), 1, &origin[0]);
    glUniform1f(glGetUniformLocation(fluidShader_, "scale"), 16.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture);
    glUniform1i(glGetUniformLocation(fluidShader_, "uDensity"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, temperatureTexture);
    glUniform1i(glGetUniformLocation(fluidShader_, "uTemperature"), 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for fire
    glDepthMask(GL_FALSE);
    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) glDisable(GL_DEPTH_TEST);

    glBindVertexArray(fluidVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
}
