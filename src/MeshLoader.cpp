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
#include <stb_image.h>

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


// Free all GPU resources
void MeshLoader::clear()
{
    for (auto& kv : cache_) {
        GpuMesh& m = kv.second;
        if (m.texture) glDeleteTextures(1, &m.texture);
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
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

    const JsonValue& mesh0  = meshesJ->a[0];
    const JsonValue* primsJ = jsonGet(mesh0, "primitives");
    if (!primsJ || primsJ->type != JsonValue::Type::Array || primsJ->a.empty()) return false;
    const JsonValue& prim0  = primsJ->a[0];
    const JsonValue* attrsJ = jsonGet(prim0, "attributes");
    if (!attrsJ || attrsJ->type != JsonValue::Type::Object) return false;
    const JsonValue* posAccJ = jsonGet(*attrsJ, "POSITION");
    if (!posAccJ || posAccJ->type != JsonValue::Type::Number) return false;
    int posAccIndex = (int)posAccJ->n;

    int uvAccIndex = -1;
    const JsonValue* uvAccJ = jsonGet(*attrsJ, "TEXCOORD_0");
    if (uvAccJ && uvAccJ->type == JsonValue::Type::Number) uvAccIndex = (int)uvAccJ->n;

    int idxAccIndex = -1;
    const JsonValue* indicesJ = jsonGet(prim0, "indices");
    if (indicesJ && indicesJ->type == JsonValue::Type::Number)
        idxAccIndex = (int)indicesJ->n;

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

    std::vector<float> uvs;
    bool hasUv = false;
    size_t uvStride = 0;
    size_t uvBase = 0;
    const unsigned char* uvBuf = nullptr;
    if (uvAccIndex >= 0 && uvAccIndex < (int)accessors.size()) {
        const Accessor& uvAcc = accessors[uvAccIndex];
        if (uvAcc.componentType == 5126 && uvAcc.type == "VEC2" && uvAcc.count == posAcc.count) {
            if (uvAcc.bufferView >= 0 && uvAcc.bufferView < (int)views.size()) {
                const BufferView& uvv = views[uvAcc.bufferView];
                if (uvv.buffer >= 0 && uvv.buffer < (int)buffers.size()) {
                    const auto& ubuf = buffers[uvv.buffer];
                    uvStride = uvv.byteStride ? uvv.byteStride : sizeof(float) * 2;
                    uvBase = uvv.byteOffset + uvAcc.byteOffset;
                    if (uvBase + (uvAcc.count - 1) * uvStride + sizeof(float) * 2 <= ubuf.size()) {
                        uvBuf = ubuf.data();
                        hasUv = true;
                        uvs.resize(uvAcc.count * 2);
                        for (size_t iVert = 0; iVert < uvAcc.count; ++iVert) {
                            const float* fp = (const float*)(uvBuf + uvBase + iVert * uvStride);
                            uvs[iVert * 2 + 0] = fp[0];
                            uvs[iVert * 2 + 1] = fp[1];
                        }
                    }
                }
            }
        }
    }

    struct Vertex { float px, py, pz, u, v; };
    std::vector<Vertex> verts;
    verts.resize(posAcc.count);
    for (size_t iVert = 0; iVert < posAcc.count; ++iVert) {
        const float* fp = (const float*)(pbuf.data() + pBase + iVert*pStride);
        verts[iVert].px = fp[0];
        verts[iVert].py = fp[1];
        verts[iVert].pz = fp[2];
        if (hasUv) {
            verts[iVert].u = uvs[iVert * 2 + 0];
            verts[iVert].v = uvs[iVert * 2 + 1];
        } else {
            verts[iVert].u = 0.0f;
            verts[iVert].v = 0.0f;
        }
    }

    std::vector<unsigned int> indices;
    if (idxAccIndex >= 0 && idxAccIndex < (int)accessors.size()) {
        const Accessor& ia = accessors[idxAccIndex];
        if (ia.type == "SCALAR" && ia.count > 0) {
            const BufferView& iv = views[ia.bufferView];
            const auto& ibuf = buffers[iv.buffer];
            size_t iBase = iv.byteOffset + ia.byteOffset;
            indices.resize(ia.count);
            if (ia.componentType == 5121) {
                const uint8_t* ip = (const uint8_t*)(ibuf.data()+iBase);
                for (size_t k=0;k<ia.count;++k) indices[k]=(unsigned int)ip[k];
            } else if (ia.componentType == 5123) {
                const uint16_t* ip = (const uint16_t*)(ibuf.data()+iBase);
                for (size_t k=0;k<ia.count;++k) indices[k]=(unsigned int)ip[k];
            } else if (ia.componentType == 5125) {
                const uint32_t* ip = (const uint32_t*)(ibuf.data()+iBase);
                for (size_t k=0;k<ia.count;++k) indices[k]=(unsigned int)ip[k];
            } else {
                return false;
            }
        }
    }

    GLuint texID = 0;
    if (hasUv) {
        const JsonValue* matsJ = jsonGet(root, "materials");
        const JsonValue* texsJ = jsonGet(root, "textures");
        const JsonValue* imgsJ = jsonGet(root, "images");
        int materialIndex = -1;
        const JsonValue* matJ = jsonGet(prim0, "material");
        if (matJ && matJ->type == JsonValue::Type::Number) materialIndex = (int)matJ->n;
        if (matsJ && texsJ && imgsJ &&
            matsJ->type == JsonValue::Type::Array &&
            texsJ->type == JsonValue::Type::Array &&
            imgsJ->type == JsonValue::Type::Array &&
            materialIndex >= 0 && materialIndex < (int)matsJ->a.size())
        {
            const JsonValue& material = matsJ->a[materialIndex];
            const JsonValue* pbr = jsonGet(material, "pbrMetallicRoughness");
            const JsonValue* bct = pbr ? jsonGet(*pbr, "baseColorTexture") : nullptr;
            const JsonValue* ti = bct ? jsonGet(*bct, "index") : nullptr;
            int textureIndex = (ti && ti->type == JsonValue::Type::Number) ? (int)ti->n : -1;
            if (textureIndex >= 0 && textureIndex < (int)texsJ->a.size()) {
                const JsonValue& tex = texsJ->a[textureIndex];
                const JsonValue* srcJ = jsonGet(tex, "source");
                int imageIndex = (srcJ && srcJ->type == JsonValue::Type::Number) ? (int)srcJ->n : -1;
                if (imageIndex >= 0 && imageIndex < (int)imgsJ->a.size()) {
                    const JsonValue& img = imgsJ->a[imageIndex];
                    std::vector<unsigned char> imgBytes;
                    const JsonValue* bvJ = jsonGet(img, "bufferView");
                    const JsonValue* uriJ = jsonGet(img, "uri");
                    if (bvJ && bvJ->type == JsonValue::Type::Number) {
                        int bvIndex = (int)bvJ->n;
                        if (bvIndex >= 0 && bvIndex < (int)views.size()) {
                            const BufferView& iv = views[bvIndex];
                            if (iv.buffer >= 0 && iv.buffer < (int)buffers.size()) {
                                const auto& ib = buffers[iv.buffer];
                                if (iv.byteOffset + iv.byteLength <= ib.size()) {
                                    imgBytes.assign(ib.begin() + iv.byteOffset, ib.begin() + iv.byteOffset + iv.byteLength);
                                }
                            }
                        }
                    } else if (uriJ && uriJ->type == JsonValue::Type::String) {
                        loadBinaryFile(dataDir_ + "/" + uriJ->s, imgBytes);
                    }

                    if (!imgBytes.empty()) {
                        int w = 0, h = 0, comp = 0;
                        unsigned char* pixels = stbi_load_from_memory(imgBytes.data(), (int)imgBytes.size(), &w, &h, &comp, 4);
                        if (pixels && w > 0 && h > 0) {
                            glGenTextures(1, &texID);
                            glBindTexture(GL_TEXTURE_2D, texID);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                            glGenerateMipmap(GL_TEXTURE_2D);
                            glBindTexture(GL_TEXTURE_2D, 0);
                        }
                        if (pixels) stbi_image_free(pixels);
                    }
                }
            }
        }
    }

    glGenVertexArrays(1, &out.vao);
    glGenBuffers(1, &out.vbo);
    glBindVertexArray(out.vao);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);
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
    out.texture = texID;
    out.textured = (texID != 0);
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
