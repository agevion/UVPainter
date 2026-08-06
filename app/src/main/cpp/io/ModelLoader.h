// Carga de mallas sin dependencias externas: GLB / glTF 2.0 (buffers embebidos)
// y OBJ. Todo lo que entra sale como una MeshData indexada y triangulada.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Math.h"
#include "gl/GLObjects.h"

namespace uvp {

struct Submesh {
    int indexOffset = 0;
    int indexCount = 0;
    int materialIndex = -1;
    std::string name;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;
    Aabb bounds;
    std::string name;

    bool hasUVs = false;
    bool uvsOutside01 = false;  // pista de UDIM / UVs fuera del tile 0-1
    int islandCount = 0;

    bool empty() const { return vertices.empty() || indices.empty(); }
    void computeBounds();
    void computeNormalsIfMissing(bool hadNormals);

    /// Agrupa los vertices en islas UV. Como tanto glTF como OBJ duplican los
    /// vertices en las costuras, dos triangulos que comparten indice comparten
    /// tambien continuidad en el atlas: basta un union-find sobre los indices.
    void computeUvIslands();
};

// `hintExt` en minusculas y sin punto ("glb", "gltf", "obj"). Puede ir vacio:
// en ese caso se detecta por los primeros bytes.
bool loadModelFromMemory(const uint8_t* data, size_t size, const std::string& hintExt,
                         MeshData& out, std::string& error);

}  // namespace uvp
