#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

//mesh representation on the GPU
struct GpuMesh {
    GLuint vao        = 0;
    GLuint vbo        = 0;
    GLuint ebo        = 0;
    GLuint texture    = 0;
    int    indexCount = 0;
    bool   indexed    = false;
    bool   valid      = false;
    bool   textured   = false;
};

//scans a data directory for mesh files, loads them on demand and caches the resulting GpuMesh objects.
class MeshLoader {
public:
    // Scan dataDir for mesh files
    void scan(const std::string& dataDir = "data");

    // Get a GpuMesh for the given mesh file
    const GpuMesh* get(const std::string& meshFile);

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
