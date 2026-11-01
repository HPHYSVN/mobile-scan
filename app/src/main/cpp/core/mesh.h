#pragma once

#include <cstdint>
#include <vector>

namespace mobile3d {

// Interleaved position (xyz) + normal (xyz). Platform-independent on purpose:
// this is the unit under host test in cpp/tests.
struct Mesh {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    uint32_t vertexCount() const { return static_cast<uint32_t>(vertices.size() / 6); }
    uint32_t triangleCount() const { return static_cast<uint32_t>(indices.size() / 3); }
};

// Unit cube centered at origin, outward normals, 24 verts / 12 tris.
Mesh makeCube();

}  // namespace mobile3d
