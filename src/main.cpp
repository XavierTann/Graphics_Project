////////////////////////////////////////////////////////////////////////
//
//
//  Fire Simulation
//
//
////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cctype>
#include <unordered_map>
#include <fstream>
#include <sstream>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "math.h"
#include "shaderSource.h"
#include "shader.h"
#include "Particles.h"
#include "BillboardRenderer.h"
#include "Config.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm> // For std::max, std::clamp

using namespace std;
namespace fs = std::filesystem;

#define _Z_NEAR                     0.001f
#define _Z_FAR                      100.0f

/***********************************************************************/
/**************************   global variables   ***********************/
/***********************************************************************/

// declaration
void processInput(GLFWwindow *window);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW Error " << error << ": " << (description ? description : "") << std::endl;
}

// Window size
unsigned int winWidth  = 1280;
unsigned int winHeight = 720;

// Camera
glm::vec3 camera_position = glm::vec3(0.0f, 0.5f, 2.5f);
glm::vec3 camera_target = glm::vec3(0.0f, 0.5f, 0.0f);
glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
float camera_fovy = 45.0f;
float camera_yaw = -90.0f;
float camera_pitch = 0.0f;
float camera_radius = 2.5f;
bool isDragging = false;
bool isPanning = false;
float lastMouseX = 0.0f, lastMouseY = 0.0f;

glm::mat4 projection;
glm::mat4 gView;
glm::mat4 gProj;
glm::mat4 gViewProj;
float lastTime = 0.0f;

// Wind Visualization
bool showWind = false;
bool enableWind = true;
float windStrength = 1.0f;
bool tornadoMode = false;
float tornadoStrength = 4.0f;
float tornadoRadius = 2.5f;
float tornadoInflow = 1.5f;
float tornadoUpdraft = 2.0f;
unsigned int windVAO = 0, windVBO = 0;
unsigned int pointVAO = 0, pointVBO = 0;
unsigned int gridVAO = 0, gridVBO = 0;
int gridVertexCount = 0;

// Fuel / lifetime (overall fire)
bool fuelEnabled = true;
bool fuelBlowAway = true;
bool fuelInfinite = false;
float fuelMax = 20.0f;
float fuel = 20.0f;
float fuelBurnRate = 1.0f;
float addFuelAmount = 5.0f;

struct SceneObject {
    std::string meshFile;
    glm::vec3 pos = glm::vec3(0.0f);
    float markerSize = 0.5f;
    float disturbRadius = 0.8f;
    float disturbStrength = 2.0f;
    float burnability = 0.7f;
    float fuelMax = 8.0f;
    float fuel = 8.0f;
    float burnRate = 0.6f;
    bool burning = false;
    float ash = 0.0f;
    float fireEmitAcc = 0.0f;
};

static std::vector<std::string> availableMeshes;
static std::vector<SceneObject> sceneObjects;
static int selectedMeshIndex = -1;
static int selectedObjectIndex = -1;
static bool isObjectDragging = false;
static int draggingObjectIndex = -1;

struct GpuMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    int indexCount = 0;
    bool indexed = false;
    bool valid = false;
};

static std::unordered_map<std::string, GpuMesh> meshCache;

static void scanMeshFiles()
{
    availableMeshes.clear();
    fs::path dataDir = fs::path("data");
    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) return;

    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        if (ext == ".obj" || ext == ".glb" || ext == ".gltf" || ext == ".fbx") {
            availableMeshes.push_back(entry.path().filename().string());
        }
    }
    std::sort(availableMeshes.begin(), availableMeshes.end());
    if (selectedMeshIndex >= (int)availableMeshes.size()) selectedMeshIndex = -1;
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<JsonValue> a;
    std::unordered_map<std::string, JsonValue> o;
};

static void jsonSkipWs(const std::string& src, size_t& i) {
    while (i < src.size()) {
        unsigned char c = (unsigned char)src[i];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') { i++; continue; }
        break;
    }
}

static bool jsonParseString(const std::string& src, size_t& i, std::string& out) {
    if (i >= src.size() || src[i] != '"') return false;
    i++;
    out.clear();
    while (i < src.size()) {
        char c = src[i++];
        if (c == '"') return true;
        if (c == '\\') {
            if (i >= src.size()) return false;
            char e = src[i++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            default: return false;
            }
        } else {
            out.push_back(c);
        }
    }
    return false;
}

static bool jsonParseNumber(const std::string& src, size_t& i, double& out) {
    size_t start = i;
    if (i < src.size() && (src[i] == '-' || src[i] == '+')) i++;
    while (i < src.size() && std::isdigit((unsigned char)src[i])) i++;
    if (i < src.size() && src[i] == '.') {
        i++;
        while (i < src.size() && std::isdigit((unsigned char)src[i])) i++;
    }
    if (i < src.size() && (src[i] == 'e' || src[i] == 'E')) {
        i++;
        if (i < src.size() && (src[i] == '-' || src[i] == '+')) i++;
        while (i < src.size() && std::isdigit((unsigned char)src[i])) i++;
    }
    if (i == start) return false;
    try {
        out = std::stod(src.substr(start, i - start));
        return true;
    } catch (...) {
        return false;
    }
}

static bool jsonParseValue(const std::string& src, size_t& i, JsonValue& out);

static bool jsonParseArray(const std::string& src, size_t& i, JsonValue& out) {
    if (i >= src.size() || src[i] != '[') return false;
    i++;
    out.type = JsonValue::Type::Array;
    out.a.clear();
    jsonSkipWs(src, i);
    if (i < src.size() && src[i] == ']') { i++; return true; }
    while (i < src.size()) {
        JsonValue v;
        jsonSkipWs(src, i);
        if (!jsonParseValue(src, i, v)) return false;
        out.a.push_back(std::move(v));
        jsonSkipWs(src, i);
        if (i >= src.size()) return false;
        if (src[i] == ',') { i++; continue; }
        if (src[i] == ']') { i++; return true; }
        return false;
    }
    return false;
}

static bool jsonParseObject(const std::string& src, size_t& i, JsonValue& out) {
    if (i >= src.size() || src[i] != '{') return false;
    i++;
    out.type = JsonValue::Type::Object;
    out.o.clear();
    jsonSkipWs(src, i);
    if (i < src.size() && src[i] == '}') { i++; return true; }
    while (i < src.size()) {
        jsonSkipWs(src, i);
        std::string key;
        if (!jsonParseString(src, i, key)) return false;
        jsonSkipWs(src, i);
        if (i >= src.size() || src[i] != ':') return false;
        i++;
        JsonValue val;
        jsonSkipWs(src, i);
        if (!jsonParseValue(src, i, val)) return false;
        out.o.emplace(std::move(key), std::move(val));
        jsonSkipWs(src, i);
        if (i >= src.size()) return false;
        if (src[i] == ',') { i++; continue; }
        if (src[i] == '}') { i++; return true; }
        return false;
    }
    return false;
}

static bool jsonParseValue(const std::string& src, size_t& i, JsonValue& out) {
    jsonSkipWs(src, i);
    if (i >= src.size()) return false;
    char c = src[i];
    if (c == '{') return jsonParseObject(src, i, out);
    if (c == '[') return jsonParseArray(src, i, out);
    if (c == '"') {
        out.type = JsonValue::Type::String;
        return jsonParseString(src, i, out.s);
    }
    if (c == 't' && src.compare(i, 4, "true") == 0) { out.type = JsonValue::Type::Bool; out.b = true; i += 4; return true; }
    if (c == 'f' && src.compare(i, 5, "false") == 0) { out.type = JsonValue::Type::Bool; out.b = false; i += 5; return true; }
    if (c == 'n' && src.compare(i, 4, "null") == 0) { out.type = JsonValue::Type::Null; i += 4; return true; }
    out.type = JsonValue::Type::Number;
    return jsonParseNumber(src, i, out.n);
}

static const JsonValue* jsonGet(const JsonValue& obj, const char* key) {
    if (obj.type != JsonValue::Type::Object) return nullptr;
    auto it = obj.o.find(key);
    if (it == obj.o.end()) return nullptr;
    return &it->second;
}

static bool loadTextFile(const fs::path& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool loadBinaryFile(const fs::path& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    if (len <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize((size_t)len);
    f.read((char*)out.data(), len);
    return f.good();
}

static unsigned int getMeshShader() {
    static unsigned int shaderID = 0;
    if (shaderID != 0) return shaderID;
    const char* vs = "#version 330\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 MVP;\n"
        "void main(){ gl_Position = MVP * vec4(aPos, 1.0); }\n";
    const char* fs = "#version 330\n"
        "uniform vec4 uColor;\n"
        "out vec4 FragColor;\n"
        "void main(){ FragColor = uColor; }\n";
    unsigned int v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, NULL);
    glCompileShader(v);
    unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, NULL);
    glCompileShader(f);
    shaderID = glCreateProgram();
    glAttachShader(shaderID, v);
    glAttachShader(shaderID, f);
    glLinkProgram(shaderID);
    glDeleteShader(v);
    glDeleteShader(f);
    return shaderID;
}

static bool buildGltfMesh(const std::string& meshFile, GpuMesh& out) {
    fs::path dataDir = fs::path("data");
    fs::path gltfPath = dataDir / meshFile;
    std::string jsonText;
    if (!loadTextFile(gltfPath, jsonText)) return false;

    JsonValue root;
    size_t idx = 0;
    if (!jsonParseValue(jsonText, idx, root)) return false;

    const JsonValue* buffersJ = jsonGet(root, "buffers");
    const JsonValue* bufferViewsJ = jsonGet(root, "bufferViews");
    const JsonValue* accessorsJ = jsonGet(root, "accessors");
    const JsonValue* meshesJ = jsonGet(root, "meshes");
    if (!buffersJ || !bufferViewsJ || !accessorsJ || !meshesJ) return false;
    if (buffersJ->type != JsonValue::Type::Array || bufferViewsJ->type != JsonValue::Type::Array || accessorsJ->type != JsonValue::Type::Array || meshesJ->type != JsonValue::Type::Array) return false;
    if (meshesJ->a.empty()) return false;

    std::vector<std::vector<unsigned char>> buffers;
    buffers.resize(buffersJ->a.size());
    for (size_t b = 0; b < buffersJ->a.size(); ++b) {
        const JsonValue& bj = buffersJ->a[b];
        const JsonValue* uriJ = jsonGet(bj, "uri");
        if (!uriJ || uriJ->type != JsonValue::Type::String) return false;
        fs::path bufPath = dataDir / uriJ->s;
        if (!loadBinaryFile(bufPath, buffers[b])) return false;
    }

    struct BufferView { int buffer = 0; size_t byteOffset = 0; size_t byteLength = 0; size_t byteStride = 0; };
    std::vector<BufferView> views;
    views.resize(bufferViewsJ->a.size());
    for (size_t v = 0; v < bufferViewsJ->a.size(); ++v) {
        const JsonValue& vj = bufferViewsJ->a[v];
        const JsonValue* buf = jsonGet(vj, "buffer");
        const JsonValue* off = jsonGet(vj, "byteOffset");
        const JsonValue* len = jsonGet(vj, "byteLength");
        const JsonValue* stride = jsonGet(vj, "byteStride");
        if (!buf || !len) return false;
        views[v].buffer = (int)buf->n;
        views[v].byteOffset = off ? (size_t)off->n : 0;
        views[v].byteLength = (size_t)len->n;
        views[v].byteStride = stride ? (size_t)stride->n : 0;
    }

    struct Accessor { int bufferView = -1; size_t byteOffset = 0; int componentType = 0; size_t count = 0; std::string type; };
    std::vector<Accessor> accessors;
    accessors.resize(accessorsJ->a.size());
    for (size_t a = 0; a < accessorsJ->a.size(); ++a) {
        const JsonValue& aj = accessorsJ->a[a];
        const JsonValue* bv = jsonGet(aj, "bufferView");
        const JsonValue* off = jsonGet(aj, "byteOffset");
        const JsonValue* ct = jsonGet(aj, "componentType");
        const JsonValue* c = jsonGet(aj, "count");
        const JsonValue* t = jsonGet(aj, "type");
        if (!bv || !ct || !c || !t) return false;
        accessors[a].bufferView = (int)bv->n;
        accessors[a].byteOffset = off ? (size_t)off->n : 0;
        accessors[a].componentType = (int)ct->n;
        accessors[a].count = (size_t)c->n;
        accessors[a].type = t->s;
    }

    const JsonValue& mesh0 = meshesJ->a[0];
    const JsonValue* primsJ = jsonGet(mesh0, "primitives");
    if (!primsJ || primsJ->type != JsonValue::Type::Array || primsJ->a.empty()) return false;
    const JsonValue& prim0 = primsJ->a[0];
    const JsonValue* attrsJ = jsonGet(prim0, "attributes");
    if (!attrsJ || attrsJ->type != JsonValue::Type::Object) return false;
    const JsonValue* posAccJ = jsonGet(*attrsJ, "POSITION");
    if (!posAccJ || posAccJ->type != JsonValue::Type::Number) return false;
    int posAccIndex = (int)posAccJ->n;

    int idxAccIndex = -1;
    const JsonValue* indicesJ = jsonGet(prim0, "indices");
    if (indicesJ && indicesJ->type == JsonValue::Type::Number) idxAccIndex = (int)indicesJ->n;

    if (posAccIndex < 0 || posAccIndex >= (int)accessors.size()) return false;
    const Accessor& posAcc = accessors[posAccIndex];
    if (posAcc.componentType != 5126) return false;
    if (posAcc.type != "VEC3") return false;
    if (posAcc.count == 0) return false;
    if (posAcc.bufferView < 0 || posAcc.bufferView >= (int)views.size()) return false;
    const BufferView& pv = views[posAcc.bufferView];
    if (pv.buffer < 0 || pv.buffer >= (int)buffers.size()) return false;
    const std::vector<unsigned char>& pbuf = buffers[pv.buffer];
    if (pv.byteOffset > pbuf.size()) return false;
    if (pv.byteLength > 0 && pv.byteOffset + pv.byteLength > pbuf.size()) return false;
    size_t pStride = pv.byteStride ? pv.byteStride : sizeof(float) * 3;
    size_t pBase = pv.byteOffset + posAcc.byteOffset;
    if (pStride < sizeof(float) * 3) return false;
    size_t last = pBase + (posAcc.count - 1) * pStride + sizeof(float) * 3;
    if (last < pBase) return false;
    if (last > pbuf.size()) return false;

    std::vector<float> positions;
    positions.resize(posAcc.count * 3);
    for (size_t iVert = 0; iVert < posAcc.count; ++iVert) {
        const unsigned char* ptr = pbuf.data() + pBase + iVert * pStride;
        const float* fp = (const float*)ptr;
        positions[iVert * 3 + 0] = fp[0];
        positions[iVert * 3 + 1] = fp[1];
        positions[iVert * 3 + 2] = fp[2];
    }

    std::vector<unsigned int> indices;
    if (idxAccIndex >= 0) {
        if (idxAccIndex < 0 || idxAccIndex >= (int)accessors.size()) return false;
        const Accessor& ia = accessors[idxAccIndex];
        if (ia.type != "SCALAR") return false;
        if (ia.count == 0) {
            idxAccIndex = -1;
        } else {
            if (ia.bufferView < 0 || ia.bufferView >= (int)views.size()) return false;
            const BufferView& iv = views[ia.bufferView];
            if (iv.buffer < 0 || iv.buffer >= (int)buffers.size()) return false;
            const std::vector<unsigned char>& ibuf = buffers[iv.buffer];
            if (iv.byteOffset > ibuf.size()) return false;
            if (iv.byteLength > 0 && iv.byteOffset + iv.byteLength > ibuf.size()) return false;
        size_t iBase = iv.byteOffset + ia.byteOffset;
        indices.resize(ia.count);
        if (ia.componentType == 5121) {
            if (iBase + ia.count * sizeof(uint8_t) > ibuf.size()) return false;
            const uint8_t* ip = (const uint8_t*)(ibuf.data() + iBase);
            for (size_t k = 0; k < ia.count; ++k) indices[k] = (unsigned int)ip[k];
        } else if (ia.componentType == 5123) {
            if (iBase + ia.count * sizeof(uint16_t) > ibuf.size()) return false;
            const uint16_t* ip = (const uint16_t*)(ibuf.data() + iBase);
            for (size_t k = 0; k < ia.count; ++k) indices[k] = (unsigned int)ip[k];
        } else if (ia.componentType == 5125) {
            if (iBase + ia.count * sizeof(uint32_t) > ibuf.size()) return false;
            const uint32_t* ip = (const uint32_t*)(ibuf.data() + iBase);
            for (size_t k = 0; k < ia.count; ++k) indices[k] = (unsigned int)ip[k];
        } else {
            return false;
        }
        }
    }

    glGenVertexArrays(1, &out.vao);
    glGenBuffers(1, &out.vbo);
    glBindVertexArray(out.vao);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    if (!indices.empty()) {
        glGenBuffers(1, &out.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        out.indexCount = (int)indices.size();
        out.indexed = true;
    } else {
        out.indexCount = (int)posAcc.count;
        out.indexed = false;
    }
    glBindVertexArray(0);
    out.valid = true;
    return true;
}

static const GpuMesh* getOrLoadMesh(const std::string& meshFile) {
    auto it = meshCache.find(meshFile);
    if (it != meshCache.end()) return &it->second;
    GpuMesh m;
    std::string ext = fs::path(meshFile).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    if (ext == ".gltf") buildGltfMesh(meshFile, m);
    meshCache.emplace(meshFile, m);
    return &meshCache.find(meshFile)->second;
}

void initWindVisualizer() {
    glGenVertexArrays(1, &windVAO);
    glGenBuffers(1, &windVBO);
    glBindVertexArray(windVAO);
    glBindBuffer(GL_ARRAY_BUFFER, windVBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void initGridVisualizer() {
    std::vector<float> gridVertices;
    float size = 10.0f;
    int slices = 20;
    float step = size / slices;
    float half = size * 0.5f;

    for (int i = 0; i <= slices; ++i) {
        float x = -half + i * step;
        // Z-lines
        gridVertices.push_back(x); gridVertices.push_back(0.0f); gridVertices.push_back(-half);
        gridVertices.push_back(x); gridVertices.push_back(0.0f); gridVertices.push_back(half);
        
        float z = -half + i * step;
        // X-lines
        gridVertices.push_back(-half); gridVertices.push_back(0.0f); gridVertices.push_back(z);
        gridVertices.push_back(half);  gridVertices.push_back(0.0f); gridVertices.push_back(z);
    }
    gridVertexCount = (int)gridVertices.size() / 3;

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void drawGridVisualizer(const glm::mat4& view, const glm::mat4& proj) {
    static unsigned int gridShaderID = 0;
    if (gridShaderID == 0) {
        const char* vs = "#version 330\n"
            "layout(location=0) in vec3 aPos;\n"
            "uniform mat4 MVP;\n"
            "void main(){ gl_Position = MVP * vec4(aPos, 1.0); }";
        const char* fs = "#version 330\n"
            "out vec4 FragColor;\n"
            "void main(){ FragColor = vec4(0.0, 0.5, 0.0, 0.5); }"; // Darker green, semi-transparent
        
        unsigned int v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        gridShaderID = glCreateProgram(); glAttachShader(gridShaderID, v); glAttachShader(gridShaderID, f); glLinkProgram(gridShaderID);
        glDeleteShader(v); glDeleteShader(f);
    }

    glEnable(GL_BLEND);
    glUseProgram(gridShaderID);
    glm::mat4 MVP = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(gridShaderID, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);
}

void initPointVisualizer() {
    glGenVertexArrays(1, &pointVAO);
    glGenBuffers(1, &pointVBO);
    glBindVertexArray(pointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void drawPointVisualizer(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& pos) {
    static unsigned int pointShaderID = 0;
    if (pointShaderID == 0) {
        const char* vs = "#version 330\n"
            "layout(location=0) in vec3 aPos;\n"
            "uniform mat4 MVP;\n"
            "void main(){ gl_Position = MVP * vec4(aPos, 1.0); gl_PointSize = 10.0; }";
        const char* fs = "#version 330\n"
            "out vec4 FragColor;\n"
            "void main(){ FragColor = vec4(0.0, 1.0, 0.0, 1.0); }"; // Green point
        
        unsigned int v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        pointShaderID = glCreateProgram(); glAttachShader(pointShaderID, v); glAttachShader(pointShaderID, f); glLinkProgram(pointShaderID);
        glDeleteShader(v); glDeleteShader(f);
    }

    float point[] = { pos.x, pos.y, pos.z };
    
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(point), point);
    
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(pointShaderID);
    glm::mat4 MVP = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(pointShaderID, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    
    glBindVertexArray(pointVAO);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void drawWindVisualizer(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& windDir, const glm::vec3& origin) {
    if (glm::length(windDir) < 0.01f) return;
    
    // This function renders a debug line to visualize wind direction.
    // A dedicated lightweight shader program is created once and reused to avoid interacting with the particle shaders.
    
    static unsigned int lineShaderID = 0;
    if (lineShaderID == 0) {
        const char* vs = "#version 330\n"
            "layout(location=0) in vec3 aPos;\n"
            "uniform mat4 MVP;\n"
            "void main(){ gl_Position = MVP * vec4(aPos, 1.0); }";
        const char* fs = "#version 330\n"
            "out vec4 FragColor;\n"
            "void main(){ FragColor = vec4(0.0, 1.0, 1.0, 1.0); }"; // Cyan line
        
        // This block compiles and links a minimal shader program for GL_LINES rendering.
        unsigned int v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        lineShaderID = glCreateProgram(); glAttachShader(lineShaderID, v); glAttachShader(lineShaderID, f); glLinkProgram(lineShaderID);
        glDeleteShader(v); glDeleteShader(f);
    }

    glm::vec3 end = origin + windDir * 2.0f; // Scale for visibility
    float lines[] = { origin.x, origin.y, origin.z, end.x, end.y, end.z };
    
    glBindBuffer(GL_ARRAY_BUFFER, windVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lines), lines);
    
    glUseProgram(lineShaderID);
    glm::mat4 MVP = proj * view;
    glUniformMatrix4fv(glGetUniformLocation(lineShaderID, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    
    glBindVertexArray(windVAO);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

GlobalParams globals;
EmitterSettings emitter;
GlobalParams smokeGlobals;
EmitterSettings smokeEmitter;

ParticleSystem flames;
ParticleSystem smokeSys;
BillboardRenderer billboards;

std::vector<InstanceAttrib> instData;
std::vector<InstanceAttrib> smokeInstData;
std::vector<InstanceAttrib> objectInstData;

shader flameShader;
shader smokeShader;

///=========================================================================================///
///                                    Callback Functions
///=========================================================================================///

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    // Camera movement (simple implementation for debugging/viewing)
    float cameraSpeed = 2.5f * 0.016f; // approximate dt
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        camera_target.y += cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        camera_target.y -= cameraSpeed;
    // Zoom
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        camera_radius -= cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        camera_radius += cameraSpeed;
    
    if (camera_radius < 0.5f) camera_radius = 0.5f;
    if (camera_radius > 20.0f) camera_radius = 20.0f;

    // Config shortcuts
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
        saveConfig("config.txt", emitter, globals);
    }
    if (glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS) {
        loadConfig("config.txt", emitter, globals);
        flames.configure(emitter, globals);
    }
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    winWidth  = width;
    winHeight = height;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            lastMouseX = (float)xpos;
            lastMouseY = (float)ypos;
            isObjectDragging = false;
            draggingObjectIndex = -1;
            float bestDist2 = 18.0f * 18.0f;
            for (int i = 0; i < (int)sceneObjects.size(); ++i) {
                const SceneObject& obj = sceneObjects[i];
                glm::vec4 clip = gViewProj * glm::vec4(obj.pos, 1.0f);
                if (clip.w <= 0.0f) continue;
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                float sx = (ndc.x * 0.5f + 0.5f) * (float)winWidth;
                float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)winHeight;
                float dx = sx - (float)xpos;
                float dy = sy - (float)ypos;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    draggingObjectIndex = i;
                }
            }
            if (draggingObjectIndex >= 0) {
                isObjectDragging = true;
                selectedObjectIndex = draggingObjectIndex;
                isDragging = false;
            } else {
                isDragging = true;
            }
        } else if (action == GLFW_RELEASE) {
            isDragging = false;
            isObjectDragging = false;
            draggingObjectIndex = -1;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            isPanning = true;
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            lastMouseX = (float)xpos;
            lastMouseY = (float)ypos;
        } else if (action == GLFW_RELEASE) {
            isPanning = false;
        }
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    float xoffset = (float)xpos - lastMouseX;
    float yoffset = lastMouseY - (float)ypos; 
    
    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;

    if (isObjectDragging && draggingObjectIndex >= 0 && draggingObjectIndex < (int)sceneObjects.size()) {
        SceneObject& obj = sceneObjects[draggingObjectIndex];
        float mx = (float)xpos;
        float my = (float)ypos;
        float x = (2.0f * mx) / (float)winWidth - 1.0f;
        float y = 1.0f - (2.0f * my) / (float)winHeight;
        glm::mat4 invVP = glm::inverse(gViewProj);
        glm::vec4 nearP = invVP * glm::vec4(x, y, -1.0f, 1.0f);
        glm::vec4 farP = invVP * glm::vec4(x, y, 1.0f, 1.0f);
        if (nearP.w != 0.0f) nearP /= nearP.w;
        if (farP.w != 0.0f) farP /= farP.w;
        glm::vec3 rayO = glm::vec3(nearP);
        glm::vec3 rayD = glm::normalize(glm::vec3(farP - nearP));
        float planeY = obj.pos.y;
        if (std::abs(rayD.y) > 1e-5f) {
            float t = (planeY - rayO.y) / rayD.y;
            if (t > 0.0f) {
                glm::vec3 hit = rayO + rayD * t;
                obj.pos.x = hit.x;
                obj.pos.z = hit.z;
            }
        }
        return;
    }

    if (isDragging) {
        float sensitivity = 0.5f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        camera_yaw += xoffset;
        camera_pitch += yoffset;

        if (camera_pitch > 89.0f) camera_pitch = 89.0f;
        if (camera_pitch < -89.0f) camera_pitch = -89.0f;
    }

    if (isPanning) {
        glm::vec3 forward;
        forward.x = cos(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
        forward.y = sin(glm::radians(camera_pitch));
        forward.z = sin(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(forward, camera_up));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        float panSpeed = camera_radius * 0.002f;
        camera_target += (right * xoffset + up * yoffset) * panSpeed;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (ImGui::GetIO().WantCaptureMouse) return;

    camera_radius -= (float)yoffset * 0.2f;
    if (camera_radius < 0.5f)
        camera_radius = 0.5f;
    if (camera_radius > 20.0f)
        camera_radius = 20.0f;
}

///=========================================================================================///
///                                      Main Function
///=========================================================================================///

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    // This block requests an OpenGL core profile context with a version that is supported across platforms.
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(winWidth, winHeight, "Ember Engine", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetWindowShouldClose(window, false);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture the mouse (optional, disabled for GUI interaction)
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // construct the shaders
    flameShader.setUpShader(particleVertexShaderSource, particleFragmentShaderSource);
    smokeShader.setUpShader(particleVertexShaderSource, smokeFragmentShaderSource);

    billboards.init();
    initWindVisualizer();
    initPointVisualizer();
    initGridVisualizer();
    scanMeshFiles();
    
    // Default configuration
    emitter.origin = glm::vec3(0.0f, 0.0f, 0.0f);
    emitter.radius = 0.15f;
    emitter.initialSpeedMin = 0.1f;
    emitter.initialSpeedMax = 0.5f;
    emitter.baseSize = 0.08f;
    emitter.lifetimeBase = 1.0f;
    
    globals.wind = glm::vec3(0.0f, 0.0f, 0.0f);
    globals.buoyancy = 1.6f;
    globals.cooling = 0.25f;
    globals.humidity = 0.0f;
    globals.turbAmp = 0.4f;
    globals.turbFreq = 1.2f;
    
    flames.configure(emitter, globals);
    flames.setSmoke(false);
    flames.spawn(500);
    
    smokeEmitter = emitter;
    smokeEmitter.baseSize = 0.12f;
    smokeGlobals = globals;
    smokeGlobals.buoyancy = 0.6f;
    smokeGlobals.cooling = 0.1f;
    smokeSys.configure(smokeEmitter, smokeGlobals);
    smokeSys.setSmoke(true);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            const float PAD = 10.0f;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos;
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos, window_pos_pivot;
            window_pos.x = work_pos.x + PAD;
            window_pos.y = work_pos.y + PAD;
            window_pos_pivot.x = 0.0f;
            window_pos_pivot.y = 0.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            float panelWidth = std::min(320.0f, std::max(200.0f, work_size.x - 2.0f * PAD));
            float panelHeight = std::max(120.0f, work_size.y - 2.0f * PAD);
            ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

            ImGui::Begin("Objects", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            if (ImGui::Button("Refresh")) {
                scanMeshFiles();
            }

            if (availableMeshes.empty()) {
                ImGui::Text("No objects found.");
            } else {
                ImGui::Text("Data folder: ./data");
                ImGui::Separator();
                for (int i = 0; i < (int)availableMeshes.size(); ++i) {
                    bool selected = (selectedMeshIndex == i);
                    if (ImGui::Selectable(availableMeshes[i].c_str(), selected)) {
                        selectedMeshIndex = i;
                    }
                }
                if (selectedMeshIndex >= 0 && selectedMeshIndex < (int)availableMeshes.size()) {
                    if (ImGui::Button("Add Selected")) {
                        SceneObject obj;
                        obj.meshFile = availableMeshes[selectedMeshIndex];
                        obj.pos = emitter.origin;
                        obj.pos.y = 0.0f;
                        sceneObjects.push_back(obj);
                        selectedObjectIndex = (int)sceneObjects.size() - 1;
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("Scene Objects");
            if (sceneObjects.empty()) {
                ImGui::Text("None");
            } else {
                for (int i = 0; i < (int)sceneObjects.size(); ++i) {
                    std::string label = std::to_string(i) + ": " + sceneObjects[i].meshFile;
                    bool selected = (selectedObjectIndex == i);
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        selectedObjectIndex = i;
                    }
                }
                if (selectedObjectIndex >= 0 && selectedObjectIndex < (int)sceneObjects.size()) {
                    SceneObject& obj = sceneObjects[selectedObjectIndex];
                    ImGui::Separator();
                    ImGui::DragFloat3("Position", (float*)&obj.pos, 0.02f);
                    ImGui::SliderFloat("Burnability", &obj.burnability, 0.0f, 1.0f);
                    ImGui::SliderFloat("Fuel", &obj.fuel, 0.0f, obj.fuelMax);
                    ImGui::SliderFloat("Fuel Max", &obj.fuelMax, 0.1f, 50.0f);
                    if (obj.fuel > obj.fuelMax) obj.fuel = obj.fuelMax;
                    ImGui::SliderFloat("Burn Rate", &obj.burnRate, 0.0f, 10.0f);
                    ImGui::SliderFloat("Disturb Radius", &obj.disturbRadius, 0.0f, 10.0f);
                    ImGui::SliderFloat("Disturb Strength", &obj.disturbStrength, 0.0f, 20.0f);
                    if (ImGui::Button("Remove")) {
                        sceneObjects.erase(sceneObjects.begin() + selectedObjectIndex);
                        if (selectedObjectIndex >= (int)sceneObjects.size()) selectedObjectIndex = (int)sceneObjects.size() - 1;
                    }
                }
            }

            ImGui::End();
        }

        // This window provides interactive controls for the simulation.
        {
            // Constrain UI to right side
            const float PAD = 10.0f;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos; // Use WorkArea to avoid menu-bars/task-bars, if any!
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos, window_pos_pivot;
            window_pos.x = work_pos.x + work_size.x - PAD;
            window_pos.y = work_pos.y + PAD;
            window_pos_pivot.x = 1.0f;
            window_pos_pivot.y = 0.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            float panelWidth = std::min(320.0f, std::max(200.0f, work_size.x - 2.0f * PAD));
            float panelHeight = std::max(120.0f, work_size.y - 2.0f * PAD);
            ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
            
            ImGui::Begin("Ember Engine Controls", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            
            if (ImGui::CollapsingHeader("Global Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enable Wind", &enableWind);
                if (enableWind) {
                    ImGui::Checkbox("Show Wind Direction", &showWind);
                    ImGui::SliderFloat("Wind Strength", &windStrength, 0.0f, 10.0f);
                    ImGui::SliderFloat3("Wind Dir", (float*)&globals.wind, -1.0f, 1.0f);
                    if (ImGui::Button("Reset Wind")) globals.wind = glm::vec3(0.0f);
                    ImGui::Checkbox("Tornado Mode", &tornadoMode);
                    if (tornadoMode) {
                        ImGui::SliderFloat("Tornado Strength", &tornadoStrength, 0.0f, 20.0f);
                        ImGui::SliderFloat("Tornado Radius", &tornadoRadius, 0.2f, 20.0f);
                        ImGui::SliderFloat("Tornado Inflow", &tornadoInflow, 0.0f, 10.0f);
                        ImGui::SliderFloat("Tornado Updraft", &tornadoUpdraft, 0.0f, 10.0f);
                    }
                }
                ImGui::SliderFloat("Buoyancy", &globals.buoyancy, 0.0f, 5.0f);
                ImGui::SliderFloat("Cooling", &globals.cooling, 0.01f, 1.0f);
                ImGui::SliderFloat("Turbulence Amp", &globals.turbAmp, 0.0f, 2.0f);
                ImGui::SliderFloat("Turbulence Freq", &globals.turbFreq, 0.1f, 5.0f);
            }
            
            if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Radius", &emitter.radius, 0.01f, 1.0f);
                ImGui::SliderFloat("Base Size", &emitter.baseSize, 0.01f, 0.5f);
                ImGui::SliderFloat("Lifetime", &emitter.lifetimeBase, 0.1f, 5.0f);
                ImGui::SliderFloat("Speed Min", &emitter.initialSpeedMin, 0.0f, 5.0f);
                ImGui::SliderFloat("Speed Max", &emitter.initialSpeedMax, 0.0f, 5.0f);
            }

            ImGui::Separator();
            ImGui::Text("Presets");
            if (ImGui::Button("Lighter")) {
                emitter.radius = 0.05f;
                emitter.baseSize = 0.06f;
                emitter.lifetimeBase = 1.6f;
                emitter.initialSpeedMin = 0.08f;
                emitter.initialSpeedMax = 0.22f;
                globals.buoyancy = 1.0f;
                globals.turbAmp = 0.25f;
                globals.turbFreq = 1.4f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Campfire")) {
                emitter.radius = 0.18f;
                emitter.baseSize = 0.09f;
                emitter.lifetimeBase = 3.0f;
                emitter.initialSpeedMin = 0.08f;
                emitter.initialSpeedMax = 0.25f;
                globals.buoyancy = 0.9f;
                globals.turbAmp = 0.6f;
                globals.turbFreq = 1.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Wildfire")) {
                emitter.radius = 0.45f;
                emitter.baseSize = 0.11f;
                emitter.lifetimeBase = 3.5f;
                emitter.initialSpeedMin = 0.15f;
                emitter.initialSpeedMax = 0.45f;
                globals.buoyancy = 1.4f;
                globals.turbAmp = 1.0f;
                globals.turbFreq = 1.3f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Iris Fire")) {
                emitter.radius = 0.16f;
                emitter.initialSpeedMin = 0.3f;
                emitter.initialSpeedMax = 0.5f;
                emitter.baseSize = 0.08f;
                emitter.lifetimeBase = 1.0f;
                globals.wind = glm::vec3(0.0f);
                globals.turbAmp = 0.0f;
                globals.cooling = 0.5f;
                enableWind = false; // "Iris Fire" typically doesn't have external wind
            }

            if (ImGui::CollapsingHeader("Fuel", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enable Fuel", &fuelEnabled);
                ImGui::Checkbox("Infinite Fuel", &fuelInfinite);
                ImGui::Checkbox("Blow Away When Out", &fuelBlowAway);
                ImGui::SliderFloat("Fuel Max", &fuelMax, 1.0f, 200.0f);
                if (fuelMax < 1.0f) fuelMax = 1.0f;
                if (fuel > fuelMax) fuel = fuelMax;
                ImGui::SliderFloat("Fuel", &fuel, 0.0f, fuelMax);
                ImGui::SliderFloat("Burn Rate", &fuelBurnRate, 0.0f, 20.0f);
                ImGui::SliderFloat("Add Fuel", &addFuelAmount, 0.0f, 50.0f);
                if (ImGui::Button("Add Fuel Now")) {
                    fuel = std::min(fuelMax, fuel + addFuelAmount);
                }
                ImGui::ProgressBar(fuelEnabled ? (fuel / fuelMax) : 1.0f, ImVec2(-1.0f, 0.0f));
            }

            ImGui::Separator();
            if (ImGui::Button("Restart")) {
                flames.reset();
                smokeSys.reset();
                flames.setSmoke(false);
                smokeSys.setSmoke(true);
                fuel = fuelMax;
                flames.spawn(500); // Initial spawn
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Config (F5)")) saveConfig("config.txt", emitter, globals);
            ImGui::SameLine();
            if (ImGui::Button("Load Config (F9)")) {
                loadConfig("config.txt", emitter, globals);
                flames.configure(emitter, globals);
                flames.setSmoke(false);
                smokeEmitter = emitter; smokeEmitter.baseSize = 0.12f;
                smokeGlobals = globals; smokeGlobals.buoyancy = 0.6f; smokeGlobals.cooling = 0.1f;
                smokeSys.configure(smokeEmitter, smokeGlobals);
                smokeSys.setSmoke(true);
            }

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // ------
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        if (dt > 0.1f) dt = 0.016f;
        lastTime = now;
        
        // Compute camera matrices
        
        // Update camera position based on orbit
        float camX = camera_radius * cos(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
        float camY = camera_radius * sin(glm::radians(camera_pitch));
        float camZ = camera_radius * sin(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
        camera_position = camera_target + glm::vec3(camX, camY, camZ);
        
        projection = glm::perspective(glm::radians(camera_fovy), (float)winWidth / (float)winHeight, _Z_NEAR, _Z_FAR);
        glm::mat4 view = glm::lookAt(camera_position, camera_target, camera_up);
        gView = view;
        gProj = projection;
        gViewProj = projection * view;

        glm::vec3 camForward = glm::normalize(camera_target - camera_position);
        glm::vec3 camRight = glm::normalize(glm::cross(camForward, camera_up));
        glm::vec3 camUpVec = glm::normalize(glm::cross(camRight, camForward));
        
        // Pass wind state
        if (fuelEnabled) {
            if (fuelInfinite) {
                fuel = fuelMax;
            } else if (fuel > 0.0f) {
                fuel = std::max(0.0f, fuel - dt * fuelBurnRate);
            }
        }

        float intensity = 1.0f;
        if (fuelEnabled) {
            intensity = (fuelMax > 0.0f) ? std::clamp(fuel / fuelMax, 0.0f, 1.0f) : 0.0f;
        }

        GlobalParams currentGlobals = globals;
        glm::vec3 windDir = globals.wind;
        float windLen = glm::length(windDir);
        if (windLen > 1e-4f) windDir /= windLen;
        glm::vec3 windVec = windDir * windStrength;
        if (!enableWind) windVec = glm::vec3(0.0f);
        currentGlobals.wind = windVec;

        EmitterSettings fueledEmitter = emitter;
        fueledEmitter.radius *= (0.6f + 0.6f * intensity);
        fueledEmitter.baseSize *= (0.6f + 0.7f * intensity);
        fueledEmitter.initialSpeedMin *= (0.45f + 0.75f * intensity);
        fueledEmitter.initialSpeedMax *= (0.45f + 0.75f * intensity);
        fueledEmitter.lifetimeBase *= (0.7f + 0.6f * intensity);

        GlobalParams fueledGlobals = currentGlobals;
        fueledGlobals.buoyancy *= (0.25f + 0.75f * intensity);
        if (fuelEnabled && intensity <= 0.0f && fuelBlowAway) {
            fueledGlobals.buoyancy *= 0.1f;
            fueledGlobals.wind *= 1.4f;
        }

        flames.configure(fueledEmitter, fueledGlobals);
        flames.setSmoke(false);
        flames.setTornado(enableWind && tornadoMode, emitter.origin, tornadoStrength, tornadoRadius, tornadoInflow, tornadoUpdraft);
        {
            std::vector<Disturber> dist;
            dist.reserve(sceneObjects.size());

            int burningCount = 0;
            for (int i = 0; i < (int)sceneObjects.size(); ++i) {
                SceneObject& obj = sceneObjects[i];
                if (obj.fuelMax < 0.1f) obj.fuelMax = 0.1f;
                if (obj.fuel > obj.fuelMax) obj.fuel = obj.fuelMax;

                if (obj.fuel <= 0.0f) {
                    obj.burning = false;
                    obj.ash = 1.0f;
                }

                bool ignite = false;
                if (!obj.burning && obj.fuel > 0.0f && obj.burnability > 0.0f) {
                    float d0 = glm::length(obj.pos - emitter.origin);
                    if (intensity > 0.2f && d0 < (1.2f + emitter.radius * 2.0f)) {
                        ignite = true;
                    } else {
                        for (int j = 0; j < (int)sceneObjects.size(); ++j) {
                            if (i == j) continue;
                            const SceneObject& other = sceneObjects[j];
                            if (!other.burning) continue;
                            float d1 = glm::length(obj.pos - other.pos);
                            if (d1 < 1.0f) { ignite = true; break; }
                        }
                    }
                }
                if (ignite) obj.burning = true;

                if (obj.burning) {
                    burningCount++;
                    float burnMul = (0.4f + intensity) * (0.2f + obj.burnability);
                    obj.fuel = std::max(0.0f, obj.fuel - dt * obj.burnRate * burnMul);
                    obj.ash = std::clamp(1.0f - (obj.fuel / obj.fuelMax), 0.0f, 1.0f);

                    float emitRate = (15.0f + 35.0f * obj.burnability) * (0.25f + intensity);
                    obj.fireEmitAcc += dt * emitRate;
                    int spawnNow = (int)obj.fireEmitAcc;
                    if (spawnNow > 0) {
                        obj.fireEmitAcc -= (float)spawnNow;
                        spawnNow = std::min(spawnNow, 12);
                        float baseSpeed = std::max(0.05f, fueledEmitter.initialSpeedMax);
                        for (int k = 0; k < spawnNow; ++k) {
                            flames.spawnAt(obj.pos, baseSpeed);
                            smokeSys.spawnAt(obj.pos, 0.25f);
                        }
                    }
                }

                if (obj.disturbRadius > 0.01f && obj.disturbStrength > 0.01f && obj.ash < 1.0f) {
                    Disturber d;
                    d.pos = obj.pos;
                    d.radius = obj.disturbRadius;
                    d.strength = obj.disturbStrength * (obj.burning ? 1.4f : 1.0f);
                    dist.push_back(d);
                }
            }
            flames.setDisturbers(dist);
            smokeSys.setDisturbers(dist);

            float smokeScale = 0.25f + 1.25f * intensity + 0.12f * (float)burningCount;
            smokeSys.setSmokeDensity(smokeScale);
        }

        static float flameSpawnAcc = 0.0f;
        const float flameSpawnRateBase = 250.0f;
        const int flameMaxParticlesBase = 700;
        float flameSpawnRate = flameSpawnRateBase * (0.15f + 0.85f * intensity);
        int flameMaxParticles = (int)(flameMaxParticlesBase * (0.2f + 0.8f * intensity));
        if (fuelEnabled && intensity <= 0.0f) {
            flameSpawnRate = 0.0f;
            flameMaxParticles = 0;
        }

        flameSpawnAcc += dt * flameSpawnRate;
        int newFlames = (int)flameSpawnAcc;
        if (newFlames > 0) {
            flameSpawnAcc -= (float)newFlames;
            int canSpawn = std::max(0, flameMaxParticles - flames.count());
            if (newFlames > canSpawn) newFlames = canSpawn;
            if (newFlames > 0) flames.spawn(newFlames);
        }

        flames.update(dt, now);
        flames.buildInstanceData(instData, projection * view);
        billboards.uploadInstances(instData);

        // Draw Wind Visualizer
        if (enableWind && showWind) {
            drawWindVisualizer(view, projection, windVec, emitter.origin);
        }
        
        // Draw Grid and Origin Point
        drawGridVisualizer(view, projection);
        drawPointVisualizer(view, projection, emitter.origin);

        if (!sceneObjects.empty()) {
            unsigned int meshShader = getMeshShader();
            glUseProgram(meshShader);
            int locMVP = glGetUniformLocation(meshShader, "MVP");
            int locCol = glGetUniformLocation(meshShader, "uColor");
            glDisable(GL_BLEND);
            for (const auto& obj : sceneObjects) {
                const GpuMesh* mesh = getOrLoadMesh(obj.meshFile);
                if (!mesh || !mesh->valid) continue;
                glm::mat4 model(1.0f);
                model = glm::translate(model, obj.pos);
                model = glm::scale(model, glm::vec3(obj.markerSize));
                glm::mat4 mvp = projection * view * model;
                glm::vec4 col;
                if (obj.ash >= 1.0f) col = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
                else if (obj.burning) col = glm::vec4(1.0f, 0.45f, 0.05f, 1.0f);
                else col = glm::vec4(0.1f, 0.8f, 0.2f, 1.0f);
                glUniformMatrix4fv(locMVP, 1, GL_FALSE, &mvp[0][0]);
                glUniform4fv(locCol, 1, &col[0]);
                glBindVertexArray(mesh->vao);
                if (mesh->indexed) glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
                else glDrawArrays(GL_TRIANGLES, 0, mesh->indexCount);
            }
            glBindVertexArray(0);
            glEnable(GL_BLEND);
        }

        objectInstData.clear();
        objectInstData.reserve(sceneObjects.size());
        for (const auto& obj : sceneObjects) {
            InstanceAttrib inst;
            inst.pos = obj.pos;
            inst.size = obj.markerSize;
            if (obj.ash >= 1.0f) {
                inst.color = glm::vec4(0.25f, 0.25f, 0.25f, 0.6f);
            } else if (obj.burning) {
                inst.color = glm::vec4(1.0f, 0.5f, 0.05f, 0.9f);
            } else {
                inst.color = glm::vec4(0.0f, 0.9f, 0.1f, 0.85f);
            }
            objectInstData.push_back(inst);
        }
        if (!objectInstData.empty()) {
            billboards.uploadInstances(objectInstData);
            smokeShader.use();
            glUniformMatrix4fv(glGetUniformLocation(smokeShader.ID, "projection"), 1, GL_FALSE, &projection[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(smokeShader.ID, "view"), 1, GL_FALSE, &view[0][0]);
            glUniform3fv(glGetUniformLocation(smokeShader.ID, "camRight"), 1, &camRight[0]);
            glUniform3fv(glGetUniformLocation(smokeShader.ID, "camUp"), 1, &camUpVec[0]);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            billboards.drawInstanced((int)objectInstData.size());
            glDepthMask(GL_TRUE);

            billboards.uploadInstances(instData);
        }

        // Draw Flames
        flameShader.use();
        glUniformMatrix4fv(glGetUniformLocation(flameShader.ID, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(flameShader.ID, "view"), 1, GL_FALSE, &view[0][0]);
        glUniform3fv(glGetUniformLocation(flameShader.ID, "camRight"), 1, &camRight[0]);
        glUniform3fv(glGetUniformLocation(flameShader.ID, "camUp"), 1, &camUpVec[0]);
        
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending (weighted by alpha) for fire
        glDepthMask(GL_FALSE); // Don't write to depth buffer for transparent particles
        billboards.drawInstanced((int)instData.size());
        glDepthMask(GL_TRUE);

        // smoke emission from cooled flames
        std::vector<glm::vec3> emitPositions;
        flames.buildSmokeEmitPositions(emitPositions);
        int smokePerEmit = 1;
        if (fuelEnabled) {
            smokePerEmit = std::max(0, (int)(intensity * 3.0f + 0.5f));
        }
        for (const auto& ep : emitPositions) {
            for (int i = 0; i < smokePerEmit; ++i) {
                smokeSys.spawnAt(ep, 0.3f);
            }
        }

        smokeEmitter = emitter;
        smokeEmitter.baseSize = emitter.baseSize * 2.2f;
        smokeGlobals = currentGlobals;
        smokeGlobals.buoyancy = globals.buoyancy * 0.35f;
        smokeGlobals.turbAmp = globals.turbAmp * 0.8f;
        smokeGlobals.turbFreq = globals.turbFreq * 0.6f;
        smokeSys.configure(smokeEmitter, smokeGlobals);
        smokeSys.setSmoke(true);
        smokeSys.setTornado(enableWind && tornadoMode, emitter.origin, tornadoStrength * 0.8f, tornadoRadius * 1.2f, tornadoInflow * 0.6f, tornadoUpdraft * 0.8f);
        
        smokeSys.update(dt, now);
        smokeSys.buildInstanceData(smokeInstData, projection * view);
        billboards.uploadInstances(smokeInstData);
        
        // Draw Smoke
        smokeShader.use();
        glUniformMatrix4fv(glGetUniformLocation(smokeShader.ID, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(smokeShader.ID, "view"), 1, GL_FALSE, &view[0][0]);
        glUniform3fv(glGetUniformLocation(smokeShader.ID, "camRight"), 1, &camRight[0]);
        glUniform3fv(glGetUniformLocation(smokeShader.ID, "camUp"), 1, &camUpVec[0]);
        
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Standard alpha blending for smoke
        glDepthMask(GL_FALSE);
        billboards.drawInstanced((int)smokeInstData.size());
        glDepthMask(GL_TRUE);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteProgram(flameShader.ID);
    glDeleteProgram(smokeShader.ID);
    {
        unsigned int meshShader = getMeshShader();
        if (meshShader) glDeleteProgram(meshShader);
        for (auto& kv : meshCache) {
            GpuMesh& m = kv.second;
            if (m.ebo) glDeleteBuffers(1, &m.ebo);
            if (m.vbo) glDeleteBuffers(1, &m.vbo);
            if (m.vao) glDeleteVertexArrays(1, &m.vao);
            m = GpuMesh();
        }
        meshCache.clear();
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}
