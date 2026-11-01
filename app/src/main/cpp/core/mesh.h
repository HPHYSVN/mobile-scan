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

// Ground plane in XZ (normal +Y), size 1x1 centered at origin, 4 verts / 2 tris.
Mesh makePlane();

// Cylinder along Y, radius 0.5, height 1, centered at origin, with caps.
Mesh makeCylinder(uint32_t segments = 24);

// UV sphere radius 0.5 centered at origin.
Mesh makeSphere(uint32_t lon = 32, uint32_t lat = 24);

}  // namespace mobile3d
