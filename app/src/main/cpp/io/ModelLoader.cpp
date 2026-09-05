#include "io/ModelLoader.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include "core/Log.h"
#include "io/Json.h"

namespace uvp {

// ---------------------------------------------------------------------------
// MeshData
// ---------------------------------------------------------------------------
void MeshData::computeBounds() {
    bounds = Aabb{};
    for (const Vertex& v : vertices) bounds.expand(v.position);
}

void MeshData::computeNormalsIfMissing(bool hadNormals) {
    if (hadNormals) return;
    for (Vertex& v : vertices) v.normal = Vec3(0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vertex& a = vertices[indices[i]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];
        const Vec3 n = cross(b.position - a.position, c.position - a.position);
        a.normal += n;
        b.normal += n;
        c.normal += n;
    }
    for (Vertex& v : vertices) v.normal = normalize(v.normal);
}

void MeshData::computeUvIslands() {
    islandCount = 0;
    if (vertices.empty()) return;

    std::vector<uint32_t> parent(vertices.size());
    for (uint32_t i = 0; i < parent.size(); ++i) parent[i] = i;

    // find iterativo con compresion de camino: sin recursion para no arriesgar
    // la pila en mallas grandes.
    auto find = [&parent](uint32_t x) {
        uint32_t root = x;
        while (parent[root] != root) root = parent[root];
        while (parent[x] != root) {
            const uint32_t next = parent[x];
            parent[x] = root;
            x = next;
        }
        return root;
    };

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t a = find(indices[i]);
        const uint32_t b = find(indices[i + 1]);
        const uint32_t c = find(indices[i + 2]);
        if (a != b) parent[b] = a;
        if (a != c) parent[find(c)] = a;
    }

    // Compactamos las raices a indices consecutivos desde 0.
    std::unordered_map<uint32_t, int> islandOf;
    islandOf.reserve(vertices.size() / 8 + 1);
    for (uint32_t i = 0; i < vertices.size(); ++i) {
        const uint32_t root = find(i);
        auto it = islandOf.find(root);
        int island;
        if (it == islandOf.end()) {
            island = islandCount++;
            islandOf.emplace(root, island);
        } else {
            island = it->second;
        }
        vertices[i].island = static_cast<float>(island);
    }
}

namespace {

// ---------------------------------------------------------------------------
// Base64 (para glTF con buffers embebidos en data: URI)
// ---------------------------------------------------------------------------
int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<uint8_t> decodeBase64(const std::string& in) {
    std::vector<uint8_t> out;
    out.reserve(in.size() * 3 / 4);
    int accum = 0;
    int bits = 0;
    for (const char c : in) {
        const int v = base64Value(c);
        if (v < 0) continue;  // saltamos '=', saltos de linea, etc.
        accum = (accum << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((accum >> bits) & 0xFF));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// glTF
// ---------------------------------------------------------------------------
struct GltfContext {
    const JsonValue* root = nullptr;
    std::vector<std::vector<uint8_t>> buffers;
};

int componentSize(int componentType) {
    switch (componentType) {
        case 5120: case 5121: return 1;  // byte / unsigned byte
        case 5122: case 5123: return 2;  // short / unsigned short
        case 5125: case 5126: return 4;  // unsigned int / float
        default: return 0;
    }
}

int componentCount(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT4") return 16;
    return 0;
}

// Lee un accessor y lo normaliza a float. Devuelve `count` elementos de
// `comps` floats cada uno.
bool readAccessorFloats(const GltfContext& ctx, int accessorIndex, std::vector<float>& out,
                        int& comps, int& count) {
    const JsonValue& accessors = ctx.root->get("accessors");
    if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= accessors.size()) return false;
    const JsonValue& acc = accessors[static_cast<size_t>(accessorIndex)];

    const int componentType = acc.intField("componentType");
    const std::string type = acc.get("type").asString();
    comps = componentCount(type);
    count = acc.intField("count");
    const int compSize = componentSize(componentType);
    if (comps == 0 || compSize == 0 || count <= 0) return false;

    out.assign(static_cast<size_t>(count) * static_cast<size_t>(comps), 0.0f);

    const JsonValue* bvIndex = acc.find("bufferView");
    if (bvIndex == nullptr) return true;  // accessor sin datos: todo ceros (valido en glTF)

    const JsonValue& bufferViews = ctx.root->get("bufferViews");
    const int bvi = bvIndex->asInt(-1);
    if (bvi < 0 || static_cast<size_t>(bvi) >= bufferViews.size()) return false;
    const JsonValue& bv = bufferViews[static_cast<size_t>(bvi)];

    const int bufferIndex = bv.intField("buffer");
    if (bufferIndex < 0 || static_cast<size_t>(bufferIndex) >= ctx.buffers.size()) return false;
    const std::vector<uint8_t>& buf = ctx.buffers[static_cast<size_t>(bufferIndex)];

    const size_t viewOffset = static_cast<size_t>(bv.intField("byteOffset", 0));
    const size_t accOffset = static_cast<size_t>(acc.intField("byteOffset", 0));
    const int declaredStride = bv.intField("byteStride", 0);
    const size_t elementSize = static_cast<size_t>(compSize) * static_cast<size_t>(comps);
    const size_t stride = declaredStride > 0 ? static_cast<size_t>(declaredStride) : elementSize;

    const bool normalized = acc.get("normalized").asBool(false);
    const size_t base = viewOffset + accOffset;
    if (base + stride * static_cast<size_t>(count - 1) + elementSize > buf.size()) {
        LOGE("Accessor %d se sale del buffer", accessorIndex);
        return false;
    }

    for (int i = 0; i < count; ++i) {
        const uint8_t* src = buf.data() + base + stride * static_cast<size_t>(i);
        for (int c = 0; c < comps; ++c) {
            const uint8_t* e = src + static_cast<size_t>(c) * static_cast<size_t>(compSize);
            float value = 0.0f;
            switch (componentType) {
                case 5126: {
                    float f;
                    std::memcpy(&f, e, 4);
                    value = f;
                    break;
                }
                case 5125: {
                    uint32_t u;
                    std::memcpy(&u, e, 4);
                    value = static_cast<float>(u);
                    break;
                }
                case 5123: {
                    uint16_t u;
                    std::memcpy(&u, e, 2);
                    value = normalized ? static_cast<float>(u) / 65535.0f : static_cast<float>(u);
                    break;
                }
                case 5122: {
                    int16_t s;
                    std::memcpy(&s, e, 2);
                    value = normalized ? std::max(static_cast<float>(s) / 32767.0f, -1.0f)
                                       : static_cast<float>(s);
                    break;
                }
                case 5121:
                    value = normalized ? static_cast<float>(*e) / 255.0f : static_cast<float>(*e);
                    break;
                case 5120: {
                    const int8_t s = static_cast<int8_t>(*e);
                    value = normalized ? std::max(static_cast<float>(s) / 127.0f, -1.0f)
                                       : static_cast<float>(s);
                    break;
                }
                default:
                    return false;
            }
            out[static_cast<size_t>(i) * static_cast<size_t>(comps) + static_cast<size_t>(c)] =
                value;
        }
    }
    return true;
}

Mat4 nodeLocalMatrix(const JsonValue& node) {
    const JsonValue* matrix = node.find("matrix");
    if (matrix != nullptr && matrix->size() == 16) {
        Mat4 m;
        for (size_t i = 0; i < 16; ++i) m.m[i] = static_cast<float>((*matrix)[i].asNumber());
        return m;
    }

    Mat4 t;
    const JsonValue* translation = node.find("translation");
    if (translation != nullptr && translation->size() == 3) {
        t = translate({static_cast<float>((*translation)[0].asNumber()),
                       static_cast<float>((*translation)[1].asNumber()),
                       static_cast<float>((*translation)[2].asNumber())});
    }

    Mat4 r;
    const JsonValue* rotation = node.find("rotation");
    if (rotation != nullptr && rotation->size() == 4) {
        r = quatToMat(static_cast<float>((*rotation)[0].asNumber()),
                      static_cast<float>((*rotation)[1].asNumber()),
                      static_cast<float>((*rotation)[2].asNumber()),
                      static_cast<float>((*rotation)[3].asNumber()));
    }

    Mat4 s;
    const JsonValue* scale = node.find("scale");
    if (scale != nullptr && scale->size() == 3) {
        s = scaleMat({static_cast<float>((*scale)[0].asNumber()),
                      static_cast<float>((*scale)[1].asNumber()),
                      static_cast<float>((*scale)[2].asNumber())});
    }

    return t * r * s;
}

bool appendPrimitive(const GltfContext& ctx, const JsonValue& prim, const Mat4& world,
                     const Mat4& normalMat, MeshData& out, bool& hadNormals) {
    // mode 4 = TRIANGLES. Los demas modos no se usan en assets de juego.
    if (prim.intField("mode", 4) != 4) return true;

    const JsonValue& attrs = prim.get("attributes");
    const JsonValue* posAcc = attrs.find("POSITION");
    if (posAcc == nullptr) return true;

    std::vector<float> positions;
    int comps = 0, count = 0;
    if (!readAccessorFloats(ctx, posAcc->asInt(-1), positions, comps, count) || comps < 3) {
        return false;
    }

    std::vector<float> normals;
    int nComps = 0, nCount = 0;
    const JsonValue* nrmAcc = attrs.find("NORMAL");
    const bool hasNormals =
        nrmAcc != nullptr && readAccessorFloats(ctx, nrmAcc->asInt(-1), normals, nComps, nCount) &&
        nComps >= 3 && nCount == count;
    if (hasNormals) hadNormals = true;

    std::vector<float> uvs;
    int uComps = 0, uCount = 0;
    const JsonValue* uvAcc = attrs.find("TEXCOORD_0");
    const bool hasUVs =
        uvAcc != nullptr && readAccessorFloats(ctx, uvAcc->asInt(-1), uvs, uComps, uCount) &&
        uComps >= 2 && uCount == count;
    if (hasUVs) out.hasUVs = true;

    const uint32_t baseVertex = static_cast<uint32_t>(out.vertices.size());
    out.vertices.reserve(out.vertices.size() + static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Vertex v;
        const Vec4 p = world * Vec4(positions[static_cast<size_t>(i) * comps + 0],
                                    positions[static_cast<size_t>(i) * comps + 1],
                                    positions[static_cast<size_t>(i) * comps + 2], 1.0f);
        v.position = p.xyz();
        if (hasNormals) {
            const Vec4 n = normalMat * Vec4(normals[static_cast<size_t>(i) * nComps + 0],
                                            normals[static_cast<size_t>(i) * nComps + 1],
                                            normals[static_cast<size_t>(i) * nComps + 2], 0.0f);
            v.normal = normalize(n.xyz());
        }
        if (hasUVs) {
            v.uv = {uvs[static_cast<size_t>(i) * uComps + 0],
                    uvs[static_cast<size_t>(i) * uComps + 1]};
            if (v.uv.x < -0.001f || v.uv.x > 1.001f || v.uv.y < -0.001f || v.uv.y > 1.001f) {
                out.uvsOutside01 = true;
            }
        }
        out.vertices.push_back(v);
    }

    Submesh sub;
    sub.indexOffset = static_cast<int>(out.indices.size());
    sub.materialIndex = prim.intField("material", -1);

    const JsonValue* idxAcc = prim.find("indices");
    if (idxAcc != nullptr) {
        std::vector<float> idx;
        int iComps = 0, iCount = 0;
        if (!readAccessorFloats(ctx, idxAcc->asInt(-1), idx, iComps, iCount)) return false;
        out.indices.reserve(out.indices.size() + static_cast<size_t>(iCount));
        for (int i = 0; i < iCount; ++i) {
            out.indices.push_back(baseVertex + static_cast<uint32_t>(idx[static_cast<size_t>(i)]));
        }
    } else {
        out.indices.reserve(out.indices.size() + static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) out.indices.push_back(baseVertex + static_cast<uint32_t>(i));
    }

    sub.indexCount = static_cast<int>(out.indices.size()) - sub.indexOffset;
    out.submeshes.push_back(sub);
    return true;
}

void traverseNode(const GltfContext& ctx, int nodeIndex, const Mat4& parent, MeshData& out,
                  bool& hadNormals, int depth) {
    if (depth > 64) return;
    const JsonValue& nodes = ctx.root->get("nodes");
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= nodes.size()) return;
    const JsonValue& node = nodes[static_cast<size_t>(nodeIndex)];

    const Mat4 world = parent * nodeLocalMatrix(node);

    const JsonValue* meshRef = node.find("mesh");
    if (meshRef != nullptr) {
        const JsonValue& meshes = ctx.root->get("meshes");
        const int mi = meshRef->asInt(-1);
        if (mi >= 0 && static_cast<size_t>(mi) < meshes.size()) {
            const Mat4 nrm = normalMatrix(world);
            const JsonValue& prims = meshes[static_cast<size_t>(mi)].get("primitives");
            for (size_t i = 0; i < prims.size(); ++i) {
                appendPrimitive(ctx, prims[i], world, nrm, out, hadNormals);
            }
        }
    }

    const JsonValue* children = node.find("children");
    if (children != nullptr) {
        for (size_t i = 0; i < children->size(); ++i) {
            traverseNode(ctx, (*children)[i].asInt(-1), world, out, hadNormals, depth + 1);
        }
    }
}

bool loadGltfJson(const std::string& json, const std::vector<uint8_t>& binChunk, MeshData& out,
                  std::string& error) {
    JsonValue root;
    if (!parseJson(json.c_str(), json.size(), root) || !root.isObject()) {
        error = "err.gltf_bad_json";
        return false;
    }

    GltfContext ctx;
    ctx.root = &root;

    const JsonValue& buffers = root.get("buffers");
    ctx.buffers.resize(buffers.size());
    for (size_t i = 0; i < buffers.size(); ++i) {
        const JsonValue& b = buffers[i];
        const JsonValue* uri = b.find("uri");
        if (uri == nullptr) {
            // Sin URI = el chunk BIN del GLB.
            ctx.buffers[i] = binChunk;
        } else {
            const std::string& u = uri->asString();
            const size_t comma = u.find(",");
            if (u.rfind("data:", 0) == 0 && comma != std::string::npos) {
                ctx.buffers[i] = decodeBase64(u.substr(comma + 1));
            } else {
                error =
                    "Este .gltf referencia un buffer externo (" + u +
                    "). Exportalo como .glb o con los datos embebidos.";
                return false;
            }
        }
    }

    int sceneIndex = root.intField("scene", 0);
    const JsonValue& scenes = root.get("scenes");
    bool hadNormals = false;

    if (sceneIndex >= 0 && static_cast<size_t>(sceneIndex) < scenes.size()) {
        const JsonValue& sceneNodes = scenes[static_cast<size_t>(sceneIndex)].get("nodes");
        for (size_t i = 0; i < sceneNodes.size(); ++i) {
            traverseNode(ctx, sceneNodes[i].asInt(-1), Mat4::identity(), out, hadNormals, 0);
        }
    } else {
        // Sin escena declarada: recorremos todos los nodos raiz que tengan malla.
        const JsonValue& nodes = root.get("nodes");
        for (size_t i = 0; i < nodes.size(); ++i) {
            traverseNode(ctx, static_cast<int>(i), Mat4::identity(), out, hadNormals, 0);
        }
    }

    if (out.empty()) {
        error = "err.no_triangle_mesh";
        return false;
    }

    out.computeNormalsIfMissing(hadNormals);
    out.computeBounds();
    return true;
}

bool loadGlb(const uint8_t* data, size_t size, MeshData& out, std::string& error) {
    if (size < 12) {
        error = "err.glb_truncated";
        return false;
    }
    uint32_t magic = 0, version = 0, total = 0;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 4);
    std::memcpy(&total, data + 8, 4);
    if (magic != 0x46546C67u) {  // "glTF"
        error = "err.not_glb";
        return false;
    }
    if (version != 2) {
        error = "err.gltf2_only";
        return false;
    }

    std::string json;
    std::vector<uint8_t> bin;
    size_t offset = 12;
    const size_t limit = std::min(static_cast<size_t>(total), size);
    while (offset + 8 <= limit) {
        uint32_t chunkLen = 0, chunkType = 0;
        std::memcpy(&chunkLen, data + offset, 4);
        std::memcpy(&chunkType, data + offset + 4, 4);
        offset += 8;
        if (offset + chunkLen > size) break;
        if (chunkType == 0x4E4F534Au) {  // "JSON"
            json.assign(reinterpret_cast<const char*>(data + offset), chunkLen);
        } else if (chunkType == 0x004E4942u) {  // "BIN\0"
            bin.assign(data + offset, data + offset + chunkLen);
        }
        offset += chunkLen;
        // Los chunks van alineados a 4 bytes.
        offset = (offset + 3u) & ~static_cast<size_t>(3u);
    }

    if (json.empty()) {
        error = "err.glb_no_json_chunk";
        return false;
    }
    return loadGltfJson(json, bin, out, error);
}

// ---------------------------------------------------------------------------
// OBJ
// ---------------------------------------------------------------------------
struct ObjKey {
    int v, vt, vn;
    bool operator==(const ObjKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
};

struct ObjKeyHash {
    size_t operator()(const ObjKey& k) const {
        return (static_cast<size_t>(k.v) * 73856093u) ^ (static_cast<size_t>(k.vt) * 19349663u) ^
               (static_cast<size_t>(k.vn) * 83492791u);
    }
};

int resolveObjIndex(int raw, size_t currentSize) {
    if (raw > 0) return raw - 1;
    if (raw < 0) return static_cast<int>(currentSize) + raw;
    return -1;
}

bool loadObj(const uint8_t* data, size_t size, MeshData& out, std::string& error) {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::unordered_map<ObjKey, uint32_t, ObjKeyHash> lookup;

    const char* p = reinterpret_cast<const char*>(data);
    const char* end = p + size;

    Submesh current;
    current.indexOffset = 0;

    auto parseFloat = [](const char*& c, const char* e) -> float {
        while (c < e && (*c == ' ' || *c == '\t')) ++c;
        char* stop = nullptr;
        const float v = std::strtof(c, &stop);
        if (stop != nullptr && stop > c) c = stop;
        return v;
    };

    while (p < end) {
        const char* lineEnd = p;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;

        const char* c = p;
        while (c < lineEnd && (*c == ' ' || *c == '\t')) ++c;

        if (c + 1 < lineEnd && c[0] == 'v' && c[1] == ' ') {
            c += 2;
            const float x = parseFloat(c, lineEnd);
            const float y = parseFloat(c, lineEnd);
            const float z = parseFloat(c, lineEnd);
            positions.push_back({x, y, z});
        } else if (c + 2 < lineEnd && c[0] == 'v' && c[1] == 't') {
            c += 2;
            const float u = parseFloat(c, lineEnd);
            const float v = parseFloat(c, lineEnd);
            uvs.push_back({u, v});
        } else if (c + 2 < lineEnd && c[0] == 'v' && c[1] == 'n') {
            c += 2;
            const float x = parseFloat(c, lineEnd);
            const float y = parseFloat(c, lineEnd);
            const float z = parseFloat(c, lineEnd);
            normals.push_back({x, y, z});
        } else if (c + 1 < lineEnd && c[0] == 'f' && (c[1] == ' ' || c[1] == '\t')) {
            c += 1;
            std::vector<uint32_t> face;
            while (c < lineEnd) {
                while (c < lineEnd && (*c == ' ' || *c == '\t')) ++c;
                if (c >= lineEnd) break;

                int vi = 0, ti = 0, ni = 0;
                char* stop = nullptr;
                vi = static_cast<int>(std::strtol(c, &stop, 10));
                if (stop == c) break;
                c = stop;
                if (c < lineEnd && *c == '/') {
                    ++c;
                    if (c < lineEnd && *c != '/') {
                        ti = static_cast<int>(std::strtol(c, &stop, 10));
                        c = stop;
                    }
                    if (c < lineEnd && *c == '/') {
                        ++c;
                        ni = static_cast<int>(std::strtol(c, &stop, 10));
                        c = stop;
                    }
                }

                const ObjKey key{resolveObjIndex(vi, positions.size()),
                                 resolveObjIndex(ti, uvs.size()),
                                 resolveObjIndex(ni, normals.size())};
                auto it = lookup.find(key);
                if (it == lookup.end()) {
                    Vertex vert;
                    if (key.v >= 0 && static_cast<size_t>(key.v) < positions.size()) {
                        vert.position = positions[static_cast<size_t>(key.v)];
                    }
                    if (key.vt >= 0 && static_cast<size_t>(key.vt) < uvs.size()) {
                        vert.uv = uvs[static_cast<size_t>(key.vt)];
                        out.hasUVs = true;
                        if (vert.uv.x < -0.001f || vert.uv.x > 1.001f || vert.uv.y < -0.001f ||
                            vert.uv.y > 1.001f) {
                            out.uvsOutside01 = true;
                        }
                    }
                    if (key.vn >= 0 && static_cast<size_t>(key.vn) < normals.size()) {
                        vert.normal = normals[static_cast<size_t>(key.vn)];
                    }
                    const uint32_t newIndex = static_cast<uint32_t>(out.vertices.size());
                    out.vertices.push_back(vert);
                    lookup.emplace(key, newIndex);
                    face.push_back(newIndex);
                } else {
                    face.push_back(it->second);
                }
            }
            // Triangulacion en abanico: vale para quads y n-gonos convexos.
            for (size_t i = 2; i < face.size(); ++i) {
                out.indices.push_back(face[0]);
                out.indices.push_back(face[i - 1]);
                out.indices.push_back(face[i]);
            }
        } else if (c + 6 < lineEnd && std::strncmp(c, "usemtl", 6) == 0) {
            current.indexCount = static_cast<int>(out.indices.size()) - current.indexOffset;
            if (current.indexCount > 0) out.submeshes.push_back(current);
            current = Submesh{};
            current.indexOffset = static_cast<int>(out.indices.size());
            const char* nameStart = c + 6;
            while (nameStart < lineEnd && (*nameStart == ' ' || *nameStart == '\t')) ++nameStart;
            current.name.assign(nameStart, static_cast<size_t>(lineEnd - nameStart));
            while (!current.name.empty() &&
                   (current.name.back() == '\r' || current.name.back() == ' ')) {
                current.name.pop_back();
            }
        }

        p = lineEnd + 1;
    }

    current.indexCount = static_cast<int>(out.indices.size()) - current.indexOffset;
    if (current.indexCount > 0) out.submeshes.push_back(current);

    if (out.empty()) {
        error = "err.obj_no_faces";
        return false;
    }

    out.computeNormalsIfMissing(!normals.empty());
    out.computeBounds();
    return true;
}

}  // namespace

bool loadModelFromMemory(const uint8_t* data, size_t size, const std::string& hintExt,
                         MeshData& out, std::string& error) {
    out = MeshData{};
    if (data == nullptr || size < 8) {
        error = "err.file_too_short";
        return false;
    }

    const bool looksGlb = size >= 4 && data[0] == 'g' && data[1] == 'l' && data[2] == 'T' &&
                          data[3] == 'F';

    if (looksGlb || hintExt == "glb") {
        return loadGlb(data, size, out, error);
    }
    if (hintExt == "gltf" || (data[0] == '{')) {
        std::string json(reinterpret_cast<const char*>(data), size);
        return loadGltfJson(json, {}, out, error);
    }
    if (hintExt == "obj" || hintExt.empty()) {
        return loadObj(data, size, out, error);
    }

    error = "err.unknown_format|" + hintExt;
    return false;
}

}  // namespace uvp
