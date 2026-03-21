#include "MeshLoader.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <iostream>

namespace fs = std::filesystem;


// used to scan the data directory for mesh files, load meshes on demand, and cache them in GPU memory
void MeshLoader::scan(const std::string& dataDir)
{
    dataDir_ = dataDir;
    availableMeshes.clear();

    fs::path dir(dataDir_);
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        if (ext == ".obj" || ext == ".glb" || ext == ".gltf" || ext == ".fbx")
            availableMeshes.push_back(entry.path().filename().string());
    }
    std::sort(availableMeshes.begin(), availableMeshes.end());
}

// Get a GpuMesh for the given mesh file, loading and caching it if needed
const GpuMesh* MeshLoader::get(const std::string& meshFile)
{
    auto it = cache_.find(meshFile);
    if (it != cache_.end()) return &it->second;

    GpuMesh m;
    std::string ext = fs::path(meshFile).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    if (ext == ".gltf") buildGltf(meshFile, m);

    cache_.emplace(meshFile, m);
    return &cache_.find(meshFile)->second;
}


// Free all GPU resources
void MeshLoader::clear()
{
    for (auto& kv : cache_) {
        GpuMesh& m = kv.second;
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        m = GpuMesh{};
    }
    cache_.clear();
}


bool MeshLoader::buildGltf(const std::string& meshFile, GpuMesh& out)
{
    std::string gltfPath = dataDir_ + "/" + meshFile;
    std::string jsonText;
    if (!loadTextFile(gltfPath, jsonText)) return false;

    JsonValue root;
    size_t idx = 0;
    if (!jsonParseValue(jsonText, idx, root)) return false;

    const JsonValue* buffersJ     = jsonGet(root, "buffers");
    const JsonValue* bufferViewsJ = jsonGet(root, "bufferViews");
    const JsonValue* accessorsJ   = jsonGet(root, "accessors");
    const JsonValue* meshesJ      = jsonGet(root, "meshes");
    if (!buffersJ || !bufferViewsJ || !accessorsJ || !meshesJ) return false;
    if (buffersJ->type     != JsonValue::Type::Array) return false;
    if (bufferViewsJ->type != JsonValue::Type::Array) return false;
    if (accessorsJ->type   != JsonValue::Type::Array) return false;
    if (meshesJ->type      != JsonValue::Type::Array) return false;
    if (meshesJ->a.empty()) return false;

    // Load binary buffers
    std::vector<std::vector<unsigned char>> buffers(buffersJ->a.size());
    for (size_t b = 0; b < buffersJ->a.size(); ++b) {
        const JsonValue* uriJ = jsonGet(buffersJ->a[b], "uri");
        if (!uriJ || uriJ->type != JsonValue::Type::String) return false;
        if (!loadBinaryFile(dataDir_ + "/" + uriJ->s, buffers[b])) return false;
    }

    // Buffer views
    struct BufferView { int buffer=0; size_t byteOffset=0, byteLength=0, byteStride=0; };
    std::vector<BufferView> views(bufferViewsJ->a.size());
    for (size_t v = 0; v < bufferViewsJ->a.size(); ++v) {
        const JsonValue& vj = bufferViewsJ->a[v];
        const JsonValue* buf    = jsonGet(vj, "buffer");
        const JsonValue* off    = jsonGet(vj, "byteOffset");
        const JsonValue* len    = jsonGet(vj, "byteLength");
        const JsonValue* stride = jsonGet(vj, "byteStride");
        if (!buf || !len) return false;
        views[v].buffer     = (int)buf->n;
        views[v].byteOffset = off    ? (size_t)off->n    : 0;
        views[v].byteLength = (size_t)len->n;
        views[v].byteStride = stride ? (size_t)stride->n : 0;
    }

    // Accessors
    struct Accessor {
        int bufferView=-1; size_t byteOffset=0;
        int componentType=0; size_t count=0; std::string type;
    };
    std::vector<Accessor> accessors(accessorsJ->a.size());
    for (size_t a = 0; a < accessorsJ->a.size(); ++a) {
        const JsonValue& aj = accessorsJ->a[a];
        const JsonValue* bv  = jsonGet(aj, "bufferView");
        const JsonValue* off = jsonGet(aj, "byteOffset");
        const JsonValue* ct  = jsonGet(aj, "componentType");
        const JsonValue* c   = jsonGet(aj, "count");
        const JsonValue* t   = jsonGet(aj, "type");
        if (!bv || !ct || !c || !t) return false;
        accessors[a].bufferView     = (int)bv->n;
        accessors[a].byteOffset     = off ? (size_t)off->n : 0;
        accessors[a].componentType  = (int)ct->n;
        accessors[a].count          = (size_t)c->n;
        accessors[a].type           = t->s;
    }

    // First primitive of first mesh
    const JsonValue& mesh0  = meshesJ->a[0];
    const JsonValue* primsJ = jsonGet(mesh0, "primitives");
    if (!primsJ || primsJ->type != JsonValue::Type::Array || primsJ->a.empty()) return false;
    const JsonValue& prim0  = primsJ->a[0];
    const JsonValue* attrsJ = jsonGet(prim0, "attributes");
    if (!attrsJ || attrsJ->type != JsonValue::Type::Object) return false;
    const JsonValue* posAccJ = jsonGet(*attrsJ, "POSITION");
    if (!posAccJ || posAccJ->type != JsonValue::Type::Number) return false;
    int posAccIndex = (int)posAccJ->n;

    int idxAccIndex = -1;
    const JsonValue* indicesJ = jsonGet(prim0, "indices");
    if (indicesJ && indicesJ->type == JsonValue::Type::Number)
        idxAccIndex = (int)indicesJ->n;

    // Read positions
    if (posAccIndex < 0 || posAccIndex >= (int)accessors.size()) return false;
    const Accessor& posAcc = accessors[posAccIndex];
    if (posAcc.componentType != 5126 || posAcc.type != "VEC3" || posAcc.count == 0) return false;
    if (posAcc.bufferView < 0 || posAcc.bufferView >= (int)views.size()) return false;
    const BufferView& pv = views[posAcc.bufferView];
    if (pv.buffer < 0 || pv.buffer >= (int)buffers.size()) return false;
    const auto& pbuf = buffers[pv.buffer];
    size_t pStride = pv.byteStride ? pv.byteStride : sizeof(float)*3;
    size_t pBase   = pv.byteOffset + posAcc.byteOffset;
    if (pBase + (posAcc.count-1)*pStride + sizeof(float)*3 > pbuf.size()) return false;

    std::vector<float> positions(posAcc.count * 3);
    for (size_t iVert = 0; iVert < posAcc.count; ++iVert) {
        const float* fp = (const float*)(pbuf.data() + pBase + iVert*pStride);
        positions[iVert*3+0] = fp[0];
        positions[iVert*3+1] = fp[1];
        positions[iVert*3+2] = fp[2];
    }

    // Read indices (optional)
    std::vector<unsigned int> indices;
    if (idxAccIndex >= 0 && idxAccIndex < (int)accessors.size()) {
        const Accessor& ia = accessors[idxAccIndex];
        if (ia.type == "SCALAR" && ia.count > 0) {
            const BufferView& iv = views[ia.bufferView];
            const auto& ibuf = buffers[iv.buffer];
            size_t iBase = iv.byteOffset + ia.byteOffset;
            indices.resize(ia.count);
            if (ia.componentType == 5121) {       // UNSIGNED_BYTE
                const uint8_t* ip = (const uint8_t*)(ibuf.data()+iBase);
                for (size_t k=0;k<ia.count;++k) indices[k]=(unsigned int)ip[k];
            } else if (ia.componentType == 5123) { // UNSIGNED_SHORT
                const uint16_t* ip = (const uint16_t*)(ibuf.data()+iBase);
                for (size_t k=0;k<ia.count;++k) indices[k]=(unsigned int)ip[k];
            } else if (ia.componentType == 5125) { // UNSIGNED_INT
                const uint32_t* ip = (const uint32_t*)(ibuf.data()+iBase);
                for (size_t k=0;k<ia.count;++k) indices[k]=(unsigned int)ip[k];
            } else {
                return false;
            }
        }
    }

    // Upload to GPU
    glGenVertexArrays(1, &out.vao);
    glGenBuffers(1, &out.vbo);
    glBindVertexArray(out.vao);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size()*sizeof(float), positions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    if (!indices.empty()) {
        glGenBuffers(1, &out.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        out.indexCount = (int)indices.size();
        out.indexed    = true;
    } else {
        out.indexCount = (int)posAcc.count;
        out.indexed    = false;
    }
    glBindVertexArray(0);
    out.valid = true;
    return true;
}

// ---------------------------------------------------------------------------
// File helpers
// ---------------------------------------------------------------------------

bool MeshLoader::loadTextFile(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool MeshLoader::loadBinaryFile(const std::string& path, std::vector<unsigned char>& out)
{
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

// ---------------------------------------------------------------------------
// Minimal JSON parser
// ---------------------------------------------------------------------------

void MeshLoader::jsonSkipWs(const std::string& src, size_t& i)
{
    while (i < src.size()) {
        unsigned char c = (unsigned char)src[i];
        if (c==' '||c=='\n'||c=='\r'||c=='\t') { i++; continue; }
        break;
    }
}

bool MeshLoader::jsonParseString(const std::string& src, size_t& i, std::string& out)
{
    if (i >= src.size() || src[i] != '"') return false;
    i++; out.clear();
    while (i < src.size()) {
        char c = src[i++];
        if (c == '"') return true;
        if (c == '\\') {
            if (i >= src.size()) return false;
            char e = src[i++];
            switch(e) {
                case '"': out.push_back('"');  break;
                case '\\':out.push_back('\\'); break;
                case '/': out.push_back('/');  break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: return false;
            }
        } else { out.push_back(c); }
    }
    return false;
}

bool MeshLoader::jsonParseNumber(const std::string& src, size_t& i, double& out)
{
    size_t start = i;
    if (i < src.size() && (src[i]=='-'||src[i]=='+')) i++;
    while (i < src.size() && std::isdigit((unsigned char)src[i])) i++;
    if (i < src.size() && src[i]=='.') { i++; while (i<src.size()&&std::isdigit((unsigned char)src[i])) i++; }
    if (i < src.size() && (src[i]=='e'||src[i]=='E')) {
        i++;
        if (i<src.size()&&(src[i]=='-'||src[i]=='+')) i++;
        while (i<src.size()&&std::isdigit((unsigned char)src[i])) i++;
    }
    if (i == start) return false;
    try { out = std::stod(src.substr(start, i-start)); return true; }
    catch (...) { return false; }
}

bool MeshLoader::jsonParseArray(const std::string& src, size_t& i, JsonValue& out)
{
    if (i>=src.size()||src[i]!='[') return false;
    i++; out.type=JsonValue::Type::Array; out.a.clear();
    jsonSkipWs(src,i);
    if (i<src.size()&&src[i]==']') { i++; return true; }
    while (i<src.size()) {
        JsonValue v; jsonSkipWs(src,i);
        if (!jsonParseValue(src,i,v)) return false;
        out.a.push_back(std::move(v));
        jsonSkipWs(src,i);
        if (i>=src.size()) return false;
        if (src[i]==',') { i++; continue; }
        if (src[i]==']') { i++; return true; }
        return false;
    }
    return false;
}

bool MeshLoader::jsonParseObject(const std::string& src, size_t& i, JsonValue& out)
{
    if (i>=src.size()||src[i]!='{') return false;
    i++; out.type=JsonValue::Type::Object; out.o.clear();
    jsonSkipWs(src,i);
    if (i<src.size()&&src[i]=='}') { i++; return true; }
    while (i<src.size()) {
        jsonSkipWs(src,i);
        std::string key;
        if (!jsonParseString(src,i,key)) return false;
        jsonSkipWs(src,i);
        if (i>=src.size()||src[i]!=':') return false;
        i++;
        JsonValue val; jsonSkipWs(src,i);
        if (!jsonParseValue(src,i,val)) return false;
        out.o.emplace(std::move(key),std::move(val));
        jsonSkipWs(src,i);
        if (i>=src.size()) return false;
        if (src[i]==',') { i++; continue; }
        if (src[i]=='}') { i++; return true; }
        return false;
    }
    return false;
}

bool MeshLoader::jsonParseValue(const std::string& src, size_t& i, JsonValue& out)
{
    jsonSkipWs(src,i);
    if (i>=src.size()) return false;
    char c = src[i];
    if (c=='{') return jsonParseObject(src,i,out);
    if (c=='[') return jsonParseArray(src,i,out);
    if (c=='"') { out.type=JsonValue::Type::String; return jsonParseString(src,i,out.s); }
    if (c=='t'&&src.compare(i,4,"true")==0)  { out.type=JsonValue::Type::Bool; out.b=true;  i+=4; return true; }
    if (c=='f'&&src.compare(i,5,"false")==0) { out.type=JsonValue::Type::Bool; out.b=false; i+=5; return true; }
    if (c=='n'&&src.compare(i,4,"null")==0)  { out.type=JsonValue::Type::Null; i+=4; return true; }
    out.type=JsonValue::Type::Number;
    return jsonParseNumber(src,i,out.n);
}

const MeshLoader::JsonValue* MeshLoader::jsonGet(const JsonValue& obj, const char* key)
{
    if (obj.type!=JsonValue::Type::Object) return nullptr;
    auto it = obj.o.find(key);
    return (it==obj.o.end()) ? nullptr : &it->second;
}
