#define STB_IMAGE_IMPLEMENTATION
#include "MeshLoader.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <iostream>
#include <functional>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace fs = std::filesystem;


// used to scan the data directory for mesh files, load meshes on demand, and cache them in GPU memory
void MeshLoader::scan(const std::string& dataDir)
{
    availableMeshes.clear();

    fs::path dir(dataDir);
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        fs::path probe = fs::current_path();
        for (int i = 0; i < 6; ++i) {
            fs::path candidate = probe / dataDir;
            if (fs::exists(candidate) && fs::is_directory(candidate)) {
                dir = candidate;
                break;
            }
            if (!probe.has_parent_path()) break;
            probe = probe.parent_path();
        }
    }
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;
    dataDir_ = dir.string();

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        if (ext == ".glb")
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
    if (ext == ".glb")      buildGlb(meshFile, m);

    cache_.emplace(meshFile, m);
    return &cache_.find(meshFile)->second;
}

MeshLoader::MeshSettings MeshLoader::settingsFor(const std::string& meshFile) const
{
    MeshSettings s;
    if (meshFile == "campfire.glb") {
        s.desiredMaxExtent = 0.8f;
        s.scaleMultiplier = 1.0f;
        s.upMode = 0;
        s.fixedScale = 0.00075f;
        return s;
    }
    if (meshFile == "grass.glb") {
        s.desiredMaxExtent = 0.8f;
        s.scaleMultiplier = 1.0f;
        s.upMode = 1;
        return s;
    }
    return s;
}


// Free all GPU resources
void MeshLoader::clear()
{
    for (auto& kv : cache_) {
        GpuMesh& m = kv.second;
        for (auto& p : m.parts) {
            if (p.texture) glDeleteTextures(1, &p.texture);
            if (p.ebo) glDeleteBuffers(1, &p.ebo);
            if (p.vbo) glDeleteBuffers(1, &p.vbo);
            if (p.vao) glDeleteVertexArrays(1, &p.vao);
        }
        m = GpuMesh{};
    }
    cache_.clear();
}


bool MeshLoader::buildGlb(const std::string& meshFile, GpuMesh& out)
{
    struct BufferView { int buffer=0; size_t byteOffset=0, byteLength=0, byteStride=0; };
    struct Accessor { int bufferView=-1; size_t byteOffset=0; int componentType=0; size_t count=0; std::string type; };

    std::vector<unsigned char> fileData;
    if (!loadBinaryFile(dataDir_ + "/" + meshFile, fileData)) return false;
    if (fileData.size() < 12) return false;

    auto readU32 = [&](size_t off) -> uint32_t {
        uint32_t v = 0;
        std::memcpy(&v, fileData.data() + off, sizeof(uint32_t));
        return v;
    };

    const uint32_t magic = readU32(0);
    const uint32_t version = readU32(4);
    const uint32_t length = readU32(8);
    if (magic != 0x46546C67) return false;
    if (version != 2) return false;
    if (length > fileData.size()) return false;

    std::string jsonText;
    std::vector<unsigned char> binChunk;

    size_t offset = 12;
    while (offset + 8 <= length) {
        uint32_t chunkLength = readU32(offset);
        uint32_t chunkType = readU32(offset + 4);
        offset += 8;
        if (offset + chunkLength > length) return false;
        if (chunkType == 0x4E4F534A) {
            jsonText.assign((const char*)fileData.data() + offset, (size_t)chunkLength);
        } else if (chunkType == 0x004E4942) {
            binChunk.assign(fileData.begin() + offset, fileData.begin() + offset + chunkLength);
        }
        offset += chunkLength;
    }

    if (jsonText.empty()) return false;

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

    std::vector<std::vector<unsigned char>> buffers(buffersJ->a.size());
    for (size_t b = 0; b < buffersJ->a.size(); ++b) {
        const JsonValue* uriJ = jsonGet(buffersJ->a[b], "uri");
        if (uriJ && uriJ->type == JsonValue::Type::String) {
            if (!loadBinaryFile(dataDir_ + "/" + uriJ->s, buffers[b])) return false;
        } else {
            if (b == 0) {
                if (binChunk.empty()) return false;
                buffers[b] = binChunk;
            } else {
                return false;
            }
        }
    }

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

    struct Vertex { float px, py, pz, u, v; };

    auto readAccessorFloat = [&](int accessorIndex, int expectedComponentType, const std::string& expectedType,
        int expectedComponents, std::vector<float>& outFloats) -> bool
        {
            if (accessorIndex < 0 || accessorIndex >= (int)accessors.size()) return false;
            const Accessor& a = accessors[accessorIndex];
            if (a.componentType != expectedComponentType) return false;
            if (a.type != expectedType) return false;
            if (a.count == 0) return false;
            if (a.bufferView < 0 || a.bufferView >= (int)views.size()) return false;
            const BufferView& v = views[a.bufferView];
            if (v.buffer < 0 || v.buffer >= (int)buffers.size()) return false;
            const auto& buf = buffers[v.buffer];
            size_t stride = v.byteStride ? v.byteStride : sizeof(float) * (size_t)expectedComponents;
            size_t base = v.byteOffset + a.byteOffset;
            if (base + (a.count - 1) * stride + sizeof(float) * (size_t)expectedComponents > buf.size()) return false;
            outFloats.resize(a.count * (size_t)expectedComponents);
            for (size_t iVert = 0; iVert < a.count; ++iVert) {
                const float* fp = (const float*)(buf.data() + base + iVert * stride);
                for (int c = 0; c < expectedComponents; ++c)
                    outFloats[iVert * (size_t)expectedComponents + (size_t)c] = fp[c];
            }
            return true;
        };

    auto readAccessorIndices = [&](int accessorIndex, std::vector<unsigned int>& outIndices) -> bool
        {
            outIndices.clear();
            if (accessorIndex < 0 || accessorIndex >= (int)accessors.size()) return false;
            const Accessor& ia = accessors[accessorIndex];
            if (ia.type != "SCALAR" || ia.count == 0) return false;
            if (ia.bufferView < 0 || ia.bufferView >= (int)views.size()) return false;
            const BufferView& iv = views[ia.bufferView];
            if (iv.buffer < 0 || iv.buffer >= (int)buffers.size()) return false;
            const auto& ibuf = buffers[iv.buffer];
            size_t iBase = iv.byteOffset + ia.byteOffset;
            if (iBase >= ibuf.size()) return false;

            outIndices.resize(ia.count);
            if (ia.componentType == 5121) {
                const uint8_t* ip = (const uint8_t*)(ibuf.data() + iBase);
                for (size_t k = 0; k < ia.count; ++k) outIndices[k] = (unsigned int)ip[k];
            }
            else if (ia.componentType == 5123) {
                const uint16_t* ip = (const uint16_t*)(ibuf.data() + iBase);
                for (size_t k = 0; k < ia.count; ++k) outIndices[k] = (unsigned int)ip[k];
            }
            else if (ia.componentType == 5125) {
                const uint32_t* ip = (const uint32_t*)(ibuf.data() + iBase);
                for (size_t k = 0; k < ia.count; ++k) outIndices[k] = (unsigned int)ip[k];
            }
            else {
                return false;
            }
            return true;
        };

    auto readVec3 = [&](const JsonValue& arr, glm::vec3 def) -> glm::vec3 {
        if (arr.type != JsonValue::Type::Array || arr.a.size() < 3) return def;
        return glm::vec3((float)arr.a[0].n, (float)arr.a[1].n, (float)arr.a[2].n);
        };

    auto readQuat = [&](const JsonValue& arr) -> glm::quat {
        if (arr.type != JsonValue::Type::Array || arr.a.size() < 4) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        float x = (float)arr.a[0].n;
        float y = (float)arr.a[1].n;
        float z = (float)arr.a[2].n;
        float w = (float)arr.a[3].n;
        return glm::quat(w, x, y, z);
        };

    auto nodeLocalMatrix = [&](const JsonValue& node) -> glm::mat4 {
        const JsonValue* matJ = jsonGet(node, "matrix");
        if (matJ && matJ->type == JsonValue::Type::Array && matJ->a.size() >= 16) {
            glm::mat4 m(1.0f);
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    m[c][r] = (float)matJ->a[(size_t)(c * 4 + r)].n;
                }
            }
            return m;
        }
        glm::vec3 t(0.0f), s(1.0f);
        glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
        const JsonValue* tJ = jsonGet(node, "translation");
        const JsonValue* sJ = jsonGet(node, "scale");
        const JsonValue* rJ = jsonGet(node, "rotation");
        if (tJ) t = readVec3(*tJ, t);
        if (sJ) s = readVec3(*sJ, s);
        if (rJ) q = readQuat(*rJ);
        glm::mat4 mt = glm::translate(glm::mat4(1.0f), t);
        glm::mat4 mr = glm::mat4_cast(q);
        glm::mat4 ms = glm::scale(glm::mat4(1.0f), s);
        return mt * mr * ms;
        };

    auto getPrimitiveMaterial = [&](const JsonValue& prim, glm::vec4& baseColor, int& imageIndex) -> void {
        baseColor = glm::vec4(1.0f);
        imageIndex = -1;
        const JsonValue* matsJ = jsonGet(root, "materials");
        const JsonValue* texsJ = jsonGet(root, "textures");
        const JsonValue* imgsJ = jsonGet(root, "images");
        const JsonValue* matJ = jsonGet(prim, "material");
        int materialIndex = (matJ && matJ->type == JsonValue::Type::Number) ? (int)matJ->n : -1;
        if (!matsJ || matsJ->type != JsonValue::Type::Array) return;
        if (materialIndex < 0 || materialIndex >= (int)matsJ->a.size()) return;
        if (!texsJ || !imgsJ || texsJ->type != JsonValue::Type::Array || imgsJ->type != JsonValue::Type::Array) return;

        const JsonValue& material = matsJ->a[(size_t)materialIndex];

        glm::vec3 emissiveFactor(0.0f);
        float emissiveStrength = 1.0f;
        {
            const JsonValue* ef = jsonGet(material, "emissiveFactor");
            if (ef && ef->type == JsonValue::Type::Array && ef->a.size() >= 3) {
                emissiveFactor = glm::vec3((float)ef->a[0].n, (float)ef->a[1].n, (float)ef->a[2].n);
            }
            const JsonValue* ext = jsonGet(material, "extensions");
            const JsonValue* khr = ext ? jsonGet(*ext, "KHR_materials_emissive_strength") : nullptr;
            const JsonValue* es = khr ? jsonGet(*khr, "emissiveStrength") : nullptr;
            if (es && es->type == JsonValue::Type::Number) emissiveStrength = (float)es->n;
        }

        const JsonValue* pbr = jsonGet(material, "pbrMetallicRoughness");
        if (pbr) {
            const JsonValue* bcf = jsonGet(*pbr, "baseColorFactor");
            if (bcf && bcf->type == JsonValue::Type::Array && bcf->a.size() >= 4) {
                baseColor = glm::vec4((float)bcf->a[0].n, (float)bcf->a[1].n, (float)bcf->a[2].n, (float)bcf->a[3].n);
            }
        }

        auto textureIndexToImageIndex = [&](int textureIndex) -> int {
            if (textureIndex < 0 || textureIndex >= (int)texsJ->a.size()) return -1;
            const JsonValue& tex = texsJ->a[(size_t)textureIndex];
            const JsonValue* srcJ = jsonGet(tex, "source");
            int ii = (srcJ && srcJ->type == JsonValue::Type::Number) ? (int)srcJ->n : -1;
            if (ii < 0 || ii >= (int)imgsJ->a.size()) return -1;
            return ii;
        };

        int baseTexIdx = -1;
        if (pbr) {
            const JsonValue* bct = jsonGet(*pbr, "baseColorTexture");
            const JsonValue* ti = bct ? jsonGet(*bct, "index") : nullptr;
            if (ti && ti->type == JsonValue::Type::Number) baseTexIdx = (int)ti->n;
        }

        int emissiveTexIdx = -1;
        {
            const JsonValue* et = jsonGet(material, "emissiveTexture");
            const JsonValue* ti = et ? jsonGet(*et, "index") : nullptr;
            if (ti && ti->type == JsonValue::Type::Number) emissiveTexIdx = (int)ti->n;
        }

        if (baseTexIdx >= 0) {
            imageIndex = textureIndexToImageIndex(baseTexIdx);
        }
        else if (emissiveTexIdx >= 0) {
            imageIndex = textureIndexToImageIndex(emissiveTexIdx);
            baseColor = glm::vec4(emissiveFactor * emissiveStrength, 1.0f);
        }
        };

    std::unordered_map<int, GLuint> texCache;
    auto loadTextureFromImageIndex = [&](int imageIndex) -> GLuint {
        if (imageIndex < 0) return 0;
        auto it = texCache.find(imageIndex);
        if (it != texCache.end()) return it->second;
        const JsonValue* imgsJ = jsonGet(root, "images");
        if (!imgsJ || imgsJ->type != JsonValue::Type::Array) return 0;
        if (imageIndex >= (int)imgsJ->a.size()) return 0;
        const JsonValue& img = imgsJ->a[(size_t)imageIndex];

        std::vector<unsigned char> imgBytes;
        const JsonValue* bvJ = jsonGet(img, "bufferView");
        const JsonValue* uriJ = jsonGet(img, "uri");
        if (bvJ && bvJ->type == JsonValue::Type::Number) {
            int bvIndex = (int)bvJ->n;
            if (bvIndex >= 0 && bvIndex < (int)views.size()) {
                const BufferView& iv = views[(size_t)bvIndex];
                if (iv.buffer >= 0 && iv.buffer < (int)buffers.size()) {
                    const auto& ib = buffers[(size_t)iv.buffer];
                    if (iv.byteOffset + iv.byteLength <= ib.size()) {
                        imgBytes.assign(ib.begin() + iv.byteOffset, ib.begin() + iv.byteOffset + iv.byteLength);
                    }
                }
            }
        }
        else if (uriJ && uriJ->type == JsonValue::Type::String) {
            std::string uri = uriJ->s;
            loadBinaryFile(dataDir_ + "/" + uri, imgBytes);
        }

        if (imgBytes.empty()) return 0;
        int w = 0, h = 0, comp = 0;
        unsigned char* pixels = stbi_load_from_memory(imgBytes.data(), (int)imgBytes.size(), &w, &h, &comp, 4);
        if (!pixels || w <= 0 || h <= 0) {
            if (pixels) stbi_image_free(pixels);
            return 0;
        }

        GLuint texID = 0;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels);
        texCache.emplace(imageIndex, texID);
        return texID;
        };

    out.parts.clear();
    out.cpuPositions.clear();
    out.cpuIndices.clear();

    const JsonValue* scenesJ = jsonGet(root, "scenes");
    const JsonValue* nodesJ = jsonGet(root, "nodes");
    int sceneIndex = 0;
    const JsonValue* sceneJ = jsonGet(root, "scene");
    if (sceneJ && sceneJ->type == JsonValue::Type::Number) sceneIndex = (int)sceneJ->n;

    std::vector<int> rootNodes;
    if (scenesJ && scenesJ->type == JsonValue::Type::Array &&
        sceneIndex >= 0 && sceneIndex < (int)scenesJ->a.size()) {
        const JsonValue& sc = scenesJ->a[(size_t)sceneIndex];
        const JsonValue* sn = jsonGet(sc, "nodes");
        if (sn && sn->type == JsonValue::Type::Array) {
            for (const auto& v : sn->a) {
                if (v.type == JsonValue::Type::Number) rootNodes.push_back((int)v.n);
            }
        }
    }
    if (rootNodes.empty() && nodesJ && nodesJ->type == JsonValue::Type::Array) {
        for (int iNode = 0; iNode < (int)nodesJ->a.size(); ++iNode) rootNodes.push_back(iNode);
    }

    auto appendPrimitive = [&](const JsonValue& prim, const glm::mat4& xform) -> void
        {
            const JsonValue* modeJ = jsonGet(prim, "mode");
            if (modeJ && modeJ->type == JsonValue::Type::Number && (int)modeJ->n != 4) return;
            const JsonValue* attrsJ = jsonGet(prim, "attributes");
            if (!attrsJ || attrsJ->type != JsonValue::Type::Object) return;
            const JsonValue* posAccJ = jsonGet(*attrsJ, "POSITION");
            if (!posAccJ || posAccJ->type != JsonValue::Type::Number) return;
            int posAccIndex = (int)posAccJ->n;

            int uvAccIndex = -1;
            const JsonValue* uvAccJ = jsonGet(*attrsJ, "TEXCOORD_0");
            if (uvAccJ && uvAccJ->type == JsonValue::Type::Number) uvAccIndex = (int)uvAccJ->n;

            std::vector<float> posFloats;
            if (!readAccessorFloat(posAccIndex, 5126, "VEC3", 3, posFloats)) return;
            size_t vertCount = posFloats.size() / 3;

            std::vector<float> uvFloats;
            bool hasUv = false;
            if (uvAccIndex >= 0) hasUv = readAccessorFloat(uvAccIndex, 5126, "VEC2", 2, uvFloats);

            int idxAccIndex = -1;
            const JsonValue* indicesJ = jsonGet(prim, "indices");
            if (indicesJ && indicesJ->type == JsonValue::Type::Number) idxAccIndex = (int)indicesJ->n;
            std::vector<unsigned int> indices;
            bool hasIndices = (idxAccIndex >= 0) && readAccessorIndices(idxAccIndex, indices);
            if (!hasIndices) {
                indices.resize(vertCount);
                for (size_t i = 0; i < vertCount; ++i) indices[i] = (unsigned int)i;
            }
            if (indices.size() < 3) return;

            std::vector<Vertex> verts;
            verts.resize(vertCount);
            for (size_t i = 0; i < vertCount; ++i) {
                glm::vec4 lp(posFloats[i * 3 + 0], posFloats[i * 3 + 1], posFloats[i * 3 + 2], 1.0f);
                glm::vec4 wp = xform * lp;
                verts[i].px = wp.x;
                verts[i].py = wp.y;
                verts[i].pz = wp.z;
                if (hasUv && (i * 2 + 1) < uvFloats.size()) {
                    verts[i].u = uvFloats[i * 2 + 0];
                    verts[i].v = uvFloats[i * 2 + 1];
                }
                else {
                    verts[i].u = 0.0f;
                    verts[i].v = 0.0f;
                }
            }

            glm::vec4 baseColor(1.0f);
            int imageIndex = -1;
            getPrimitiveMaterial(prim, baseColor, imageIndex);
            GLuint texID = (hasUv && imageIndex >= 0) ? loadTextureFromImageIndex(imageIndex) : 0;

            GpuSubMesh part;
            part.baseColorFactor = baseColor;
            part.texture = texID;
            part.textured = (texID != 0);

            glGenVertexArrays(1, &part.vao);
            glGenBuffers(1, &part.vbo);
            glBindVertexArray(part.vao);
            glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 3));
            glEnableVertexAttribArray(1);
            if (!indices.empty()) {
                glGenBuffers(1, &part.ebo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
                part.indexCount = (int)indices.size();
                part.indexed = true;
            }
            else {
                part.indexCount = (int)vertCount;
                part.indexed = false;
            }
            glBindVertexArray(0);

            unsigned int baseVertex = (unsigned int)out.cpuPositions.size();
            out.cpuPositions.reserve(out.cpuPositions.size() + verts.size());
            for (const auto& v : verts) out.cpuPositions.push_back(glm::vec3(v.px, v.py, v.pz));
            out.cpuIndices.reserve(out.cpuIndices.size() + indices.size());
            for (auto ii : indices) out.cpuIndices.push_back(baseVertex + ii);

            out.parts.push_back(part);
        };

    if (nodesJ && nodesJ->type == JsonValue::Type::Array) {
        int maxDepth = (int)nodesJ->a.size() + 4;
        std::function<void(int, const glm::mat4&, int)> visit = [&](int nodeIndex, const glm::mat4& parent, int depth) {
            if (nodeIndex < 0 || nodeIndex >= (int)nodesJ->a.size()) return;
            if (depth > maxDepth) return;
            const JsonValue& node = nodesJ->a[(size_t)nodeIndex];
            glm::mat4 local = nodeLocalMatrix(node);
            glm::mat4 world = parent * local;
            const JsonValue* meshJ = jsonGet(node, "mesh");
            int meshIndex = (meshJ && meshJ->type == JsonValue::Type::Number) ? (int)meshJ->n : -1;
            if (meshIndex >= 0 && meshIndex < (int)meshesJ->a.size()) {
                const JsonValue& mesh = meshesJ->a[(size_t)meshIndex];
                const JsonValue* primsJ = jsonGet(mesh, "primitives");
                if (primsJ && primsJ->type == JsonValue::Type::Array) {
                    for (const auto& prim : primsJ->a) appendPrimitive(prim, world);
                }
            }
            const JsonValue* childrenJ = jsonGet(node, "children");
            if (childrenJ && childrenJ->type == JsonValue::Type::Array) {
                for (const auto& c : childrenJ->a) {
                    if (c.type == JsonValue::Type::Number) visit((int)c.n, world, depth + 1);
                }
            }
            };

        for (int rn : rootNodes) visit(rn, glm::mat4(1.0f), 0);
    }
    else {
        for (const auto& mesh : meshesJ->a) {
            const JsonValue* primsJ = jsonGet(mesh, "primitives");
            if (!primsJ || primsJ->type != JsonValue::Type::Array) continue;
            for (const auto& prim : primsJ->a) appendPrimitive(prim, glm::mat4(1.0f));
        }
    }

    out.boundsValid = false;
    out.authoredZUp = false;
    if (!out.cpuPositions.empty()) {
        glm::vec3 mn(1e9f), mx(-1e9f);
        for (const auto& p : out.cpuPositions) {
            mn = glm::min(mn, p);
            mx = glm::max(mx, p);
        }
        out.aabbMin = mn;
        out.aabbMax = mx;
        out.boundsValid = true;

        glm::vec3 ext = mx - mn;
        float maxXY = std::max(ext.x, ext.y);
        float maxOther = std::max(maxXY, ext.z);
        if (maxOther > 1e-6f) {
            float ratioZ = ext.z / std::max(1e-6f, std::max(ext.x, ext.y));
            out.authoredZUp = (ratioZ < 0.18f);
        }
    }

    out.triCdf.clear();
    out.triAreaSum = 0.0f;
    if (!out.cpuPositions.empty()) {
        auto triArea = [&](unsigned int ia, unsigned int ib, unsigned int ic) -> float {
            if (ia >= out.cpuPositions.size() || ib >= out.cpuPositions.size() || ic >= out.cpuPositions.size())
                return 0.0f;
            const glm::vec3& a = out.cpuPositions[ia];
            const glm::vec3& b = out.cpuPositions[ib];
            const glm::vec3& c = out.cpuPositions[ic];
            return 0.5f * glm::length(glm::cross(b - a, c - a));
            };

        if (!out.cpuIndices.empty() && out.cpuIndices.size() >= 3) {
            size_t triCount = out.cpuIndices.size() / 3;
            out.triCdf.reserve(triCount);
            for (size_t t = 0; t < triCount; ++t) {
                unsigned int ia = out.cpuIndices[t * 3 + 0];
                unsigned int ib = out.cpuIndices[t * 3 + 1];
                unsigned int ic = out.cpuIndices[t * 3 + 2];
                float a = triArea(ia, ib, ic);
                out.triAreaSum += a;
                out.triCdf.push_back(out.triAreaSum);
            }
        }
        else if (out.cpuPositions.size() >= 3 && (out.cpuPositions.size() % 3) == 0) {
            size_t triCount = out.cpuPositions.size() / 3;
            out.triCdf.reserve(triCount);
            for (size_t t = 0; t < triCount; ++t) {
                unsigned int ia = (unsigned int)(t * 3 + 0);
                unsigned int ib = (unsigned int)(t * 3 + 1);
                unsigned int ic = (unsigned int)(t * 3 + 2);
                float a = triArea(ia, ib, ic);
                out.triAreaSum += a;
                out.triCdf.push_back(out.triAreaSum);
            }
        }
    }

    out.valid = !out.parts.empty();
    return out.valid;
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
        out.o.emplace(std::move(key), std::make_shared<JsonValue>(std::move(val)));
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
    return (it==obj.o.end() || !it->second) ? nullptr : it->second.get();
}
