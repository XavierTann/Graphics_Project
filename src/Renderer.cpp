#include "Renderer.h"
#include "SceneObject.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
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
        gridVerts.insert(gridVerts.end(), { x, -half, 0.0f,  x, half, 0.0f });
        float y = -half + i * step;
        gridVerts.insert(gridVerts.end(), { -half, y, 0.0f,  half, y, 0.0f });
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

    // ---- Axis lines (+X, +Y, +Z) ----
    float axisVerts[] = {
        0.0f, 0.0f, 0.01f,   2.5f, 0.0f, 0.01f,
        0.0f, 0.0f, 0.01f,   0.0f, 2.5f, 0.01f,
        0.01f, 0.0f, 0.0f,   0.01f, 0.0f, 2.5f,
    };
    glGenVertexArrays(1, &axisVAO_);
    glGenBuffers(1, &axisVBO_);
    glBindVertexArray(axisVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVerts), axisVerts, GL_STATIC_DRAW);
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
}

void Renderer::shutdown()
{
    meshLoader_.clear();
    if (gridVAO_) { glDeleteVertexArrays(1, &gridVAO_);  gridVAO_ = 0; }
    if (gridVBO_) { glDeleteBuffers(1, &gridVBO_);        gridVBO_ = 0; }
    if (windVAO_) { glDeleteVertexArrays(1, &windVAO_);  windVAO_ = 0; }
    if (windVBO_) { glDeleteBuffers(1, &windVBO_);        windVBO_ = 0; }
    if (axisVAO_) { glDeleteVertexArrays(1, &axisVAO_); axisVAO_ = 0; }
    if (axisVBO_) { glDeleteBuffers(1, &axisVBO_);      axisVBO_ = 0; }
    if (pointVAO_) { glDeleteVertexArrays(1, &pointVAO_); pointVAO_ = 0; }
    if (pointVBO_) { glDeleteBuffers(1, &pointVBO_);       pointVBO_ = 0; }
    if (gridShader_) { glDeleteProgram(gridShader_);  gridShader_ = 0; }
    if (lineShader_) { glDeleteProgram(lineShader_);  lineShader_ = 0; }
    if (pointShader_) { glDeleteProgram(pointShader_); pointShader_ = 0; }
    if (meshShader_) { glDeleteProgram(meshShader_);  meshShader_ = 0; }
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
        "uniform vec4 uColor;\n"
        "out vec4 FragColor;\n"
        "void main(){ FragColor = uColor; }\n";
    lineShader_ = compileSimpleShader(vs, fs);
}


void Renderer::ensurePointShader()
{
    if (pointShader_) return;
    const char* vs =
        "#version 330\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 MVP;\n"
        "uniform float uPointSize;\n"
        "void main(){ gl_Position = MVP * vec4(aPos,1.0); gl_PointSize=uPointSize; }\n";
    const char* fs =
        "#version 330\n"
        "uniform vec4 uColor;\n"
        "out vec4 FragColor;\n"
        "void main(){ FragColor = uColor; }\n";
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
    ensureLineShader();
    glEnable(GL_BLEND);
    glm::mat4 MVP = proj * view;

    // Grid
    glUseProgram(gridShader_);
    glUniformMatrix4fv(glGetUniformLocation(gridShader_, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glBindVertexArray(gridVAO_);
    glDrawArrays(GL_LINES, 0, gridVertexCount_);
    glLineWidth(2.5f);
    glUseProgram(lineShader_);
    glUniformMatrix4fv(glGetUniformLocation(lineShader_, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    int locCol = glGetUniformLocation(lineShader_, "uColor");

    glBindVertexArray(axisVAO_);
    glUniform4f(locCol, 0.85f, 0.18f, 0.18f, 1.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glUniform4f(locCol, 0.12f, 0.88f, 0.22f, 1.0f);
    glDrawArrays(GL_LINES, 2, 2);
    glUniform4f(locCol, 0.22f, 0.48f, 0.90f, 1.0f);
    glDrawArrays(GL_LINES, 4, 2);
    glLineWidth(1.0f);

    glBindVertexArray(0);
}

void Renderer::drawMarkerPoint(const glm::mat4& view, const glm::mat4& proj,
    const glm::vec3& pos, const glm::vec4& color, float size)
{
    ensurePointShader();
    float pt[3] = { pos.x, pos.y, pos.z };
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pt), pt);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(pointShader_);
    glm::mat4 MVP = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(pointShader_, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniform4fv(glGetUniformLocation(pointShader_, "uColor"), 1, &color[0]);
    glUniform1f(glGetUniformLocation(pointShader_, "uPointSize"), size);
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
    glUniform4f(glGetUniformLocation(lineShader_, "uColor"), 0.0f, 1.0f, 1.0f, 1.0f);
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    if (locTex >= 0) glUniform1i(locTex, 0);

    for (const auto& obj : objects) {
        const GpuMesh* mesh = meshLoader_.get(obj.meshFile);
        if (!mesh || !mesh->valid) continue;

        MeshLoader::MeshSettings tuning = meshLoader_.settingsFor(obj.meshFile);
        bool authoredZUp = mesh->authoredZUp;
        if (tuning.upMode == 1) authoredZUp = true;
        if (tuning.upMode == 0) authoredZUp = false;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), obj.pos);
        if (!authoredZUp)
            model = model * glm::rotate(glm::mat4(1.0f), 1.57079632679f, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(obj.markerSize));
        glm::mat4 mvp = proj * view * model;

        glm::vec4 col;
        float ashT = obj.ash;

        if (!obj.burning && ashT < 0.01f) {
            col = mesh->textured ? glm::vec4(1.0f) : glm::vec4(0.1f, 0.8f, 0.2f, 1.0f);
        }
        else if (ashT < 0.15f) {
            float t = ashT / 0.15f;
            glm::vec4 base = mesh->textured ? glm::vec4(1.0f) : glm::vec4(0.1f, 0.8f, 0.2f, 1.0f);
            col = glm::mix(base, glm::vec4(1.0f, 0.95f, 0.6f, 1.0f), t);
        }
        else if (ashT < 0.40f) {
            float t = (ashT - 0.15f) / 0.25f;
            col = glm::mix(glm::vec4(1.0f, 0.95f, 0.6f, 1.0f),
                glm::vec4(1.0f, 0.35f, 0.02f, 1.0f), t);
        }
        else if (ashT < 0.70f) {
            float t = (ashT - 0.40f) / 0.30f;
            col = glm::mix(glm::vec4(1.0f, 0.35f, 0.02f, 1.0f),
                glm::vec4(0.35f, 0.06f, 0.01f, 1.0f), t);
        }
        else if (ashT < 0.90f) {
            float t = (ashT - 0.70f) / 0.20f;
            col = glm::mix(glm::vec4(0.35f, 0.06f, 0.01f, 1.0f),
                glm::vec4(0.18f, 0.16f, 0.14f, 1.0f), t);
        }
        else {
            float t = (ashT - 0.90f) / 0.10f;
            col = glm::mix(glm::vec4(0.18f, 0.16f, 0.14f, 1.0f),
                glm::vec4(0.10f, 0.10f, 0.10f, 1.0f), t);
        }

        // Flicker while actively burning
        if (obj.burning && ashT < 0.85f) {
            float flicker = 0.92f + 0.08f * std::sin(ashT * 47.3f + obj.fuel);
            col.r *= flicker;
            col.g *= flicker * 0.85f;
        }
        col.a *= std::clamp(obj.alpha, 0.0f, 1.0f);

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
    const glm::vec3& camRight, const glm::vec3& camUp,
    const BillboardLighting& lighting)
{
    billboards_.uploadInstances(data);
    sh.use();
    glUniformMatrix4fv(glGetUniformLocation(sh.ID, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(sh.ID, "view"), 1, GL_FALSE, &view[0][0]);
    glUniform3fv(glGetUniformLocation(sh.ID, "camRight"), 1, &camRight[0]);
    glUniform3fv(glGetUniformLocation(sh.ID, "camUp"), 1, &camUp[0]);
    glUniform3fv(glGetUniformLocation(sh.ID, "cameraPos"), 1, &lighting.cameraPos[0]);
    glUniform3fv(glGetUniformLocation(sh.ID, "fireLightPos"), 1, &lighting.fireLightPos[0]);
    glUniform3fv(glGetUniformLocation(sh.ID, "fireLightColor"), 1, &lighting.fireLightColor[0]);
    glUniform1f(glGetUniformLocation(sh.ID, "fireLightIntensity"), lighting.fireLightIntensity);
    glUniform1f(glGetUniformLocation(sh.ID, "fireLightRange"), lighting.fireLightRange);
    glUniform1f(glGetUniformLocation(sh.ID, "uTime"), lighting.time);
}

void Renderer::drawFlames(const std::vector<InstanceAttrib>& data,
    shader& flameShader,
    const glm::mat4& proj, const glm::mat4& view,
    const glm::vec3& camRight, const glm::vec3& camUp,
    const BillboardLighting& lighting)
{
    if (data.empty()) return;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    uploadAndDraw(data, flameShader, proj, view, camRight, camUp, lighting);
    billboards_.drawInstanced((int)data.size());
    glDepthMask(GL_TRUE);
}

void Renderer::drawSmoke(const std::vector<InstanceAttrib>& data,
    shader& smokeShader,
    const glm::mat4& proj, const glm::mat4& view,
    const glm::vec3& camRight, const glm::vec3& camUp,
    const BillboardLighting& lighting)
{
    if (data.empty()) return;
    std::vector<InstanceAttrib> sorted = data;
    std::stable_sort(sorted.begin(), sorted.end(),
        [&view](const InstanceAttrib& a, const InstanceAttrib& b) {
            float az = (view * glm::vec4(a.pos, 1.0f)).z;
            float bz = (view * glm::vec4(b.pos, 1.0f)).z;
            return az < bz;
        });
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    uploadAndDraw(sorted, smokeShader, proj, view, camRight, camUp, lighting);
    billboards_.drawInstanced((int)sorted.size());
    glDepthMask(GL_TRUE);
}

void Renderer::loadDecorationMesh(const std::string& path, const glm::vec3& pos, float scale)
{
    for (const auto& d : decorations_) {
        if (d.meshFile == path && d.pos == pos && d.scale == scale) return;
    }
    decorations_.push_back({ path, pos, scale });
}

void Renderer::drawDecorations(const glm::mat4& view, const glm::mat4& proj)
{
    if (decorations_.empty()) return;
    ensureMeshShader();

    glUseProgram(meshShader_);
    int locMVP = glGetUniformLocation(meshShader_, "MVP");
    int locCol = glGetUniformLocation(meshShader_, "uColor");
    int locUseTex = glGetUniformLocation(meshShader_, "uUseTex");
    int locTex = glGetUniformLocation(meshShader_, "uTex");

    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    if (locTex >= 0) glUniform1i(locTex, 0);

    for (const auto& dec : decorations_) {
        const GpuMesh* mesh = meshLoader_.get(dec.meshFile);
        if (!mesh || !mesh->valid) continue;

        MeshLoader::MeshSettings tuning = meshLoader_.settingsFor(dec.meshFile);
        bool authoredZUp = mesh->authoredZUp;
        if (tuning.upMode == 1) authoredZUp = true;
        if (tuning.upMode == 0) authoredZUp = false;
        float scale = dec.scale * tuning.scaleMultiplier;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), dec.pos);
        if (!authoredZUp)
            model = model * glm::rotate(glm::mat4(1.0f), 1.57079632679f, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(scale));
        glm::mat4 mvp = proj * view * model;

        glUniformMatrix4fv(locMVP, 1, GL_FALSE, &mvp[0][0]);

        if (mesh->textured && mesh->texture) {
            glUniform4f(locCol, 1.0f, 1.0f, 1.0f, 1.0f);
            if (locUseTex >= 0) glUniform1i(locUseTex, 1);
            glBindTexture(GL_TEXTURE_2D, mesh->texture);
        }
        else {
            // Neutral brown tint for untextured campfire logs
            glUniform4f(locCol, 0.55f, 0.38f, 0.22f, 1.0f);
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
