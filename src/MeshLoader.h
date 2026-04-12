#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

//mesh representation on the GPU
struct GpuSubMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint texture = 0;
    int indexCount = 0;
    bool indexed = false;
    bool textured = false;
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
};

struct GpuMesh {
    bool   valid      = false;
    std::vector<GpuSubMesh> parts;
    std::vector<glm::vec3> cpuPositions;
    std::vector<unsigned int> cpuIndices;
    std::vector<float> triCdf;
    float triAreaSum = 0.0f;
    glm::vec3 aabbMin = glm::vec3(0.0f);
    glm::vec3 aabbMax = glm::vec3(0.0f);
    bool boundsValid = false;
    bool authoredZUp = false;
};

//scans a data directory for mesh files, loads them on demand and caches the resulting GpuMesh objects.
class MeshLoader {
public:
    struct MeshSettings {
        float desiredMaxExtent = 0.8f;
        float scaleMultiplier = 1.0f;
        int upMode = -1;
        float fixedScale = -1.0f;
    };

    // Scan dataDir for mesh files
    void scan(const std::string& dataDir = "data");

    // Get a GpuMesh for the given mesh file
    const GpuMesh* get(const std::string& meshFile);

    MeshSettings settingsFor(const std::string& meshFile) const;

    // Free all GPU resources.
    void clear();

    // List of available meshes
    std::vector<std::string> availableMeshes;

private:
	// Cache of loaded meshes
    std::unordered_map<std::string, GpuMesh> cache_;
    std::string dataDir_ = "data";

    // Build a GpuMesh from a glb file
    bool buildGlb(const std::string& meshFile, GpuMesh& out);


    // Minimal JSON parser for the embedded JSON chunk in GLB files
    struct JsonValue {
        enum class Type { Null, Bool, Number, String, Array, Object };
        Type type = Type::Null;
        bool   b = false;
        double n = 0.0;
        std::string s;
        std::vector<JsonValue> a;
        std::unordered_map<std::string, JsonValue> o;
    };

    // JSON parsing functions
    static void        jsonSkipWs(const std::string& src, size_t& i);
    static bool        jsonParseString(const std::string& src, size_t& i, std::string& out);
    static bool        jsonParseNumber(const std::string& src, size_t& i, double& out);
    static bool        jsonParseValue (const std::string& src, size_t& i, JsonValue& out);
    static bool        jsonParseArray (const std::string& src, size_t& i, JsonValue& out);
    static bool        jsonParseObject(const std::string& src, size_t& i, JsonValue& out);
    static const JsonValue* jsonGet(const JsonValue& obj, const char* key);

    static bool loadTextFile  (const std::string& path, std::string& out);
    static bool loadBinaryFile(const std::string& path, std::vector<unsigned char>& out);
};
