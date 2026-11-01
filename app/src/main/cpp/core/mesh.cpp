#include "mesh.h"

#include <cmath>

namespace mobile3d {
namespace {

void pushVertex(Mesh* mesh, float x, float y, float z, float nx, float ny, float nz) {
    mesh->vertices.insert(mesh->vertices.end(), {x, y, z, nx, ny, nz});
}

}  // namespace

Mesh makeCube() {
    // Per-face verts so normals are exact. Faces: +X,-X,+Y,-Y,+Z,-Z.
    const float h = 0.5f;
    const float faces[6][4][6] = {
        {{ h,-h,-h, 1,0,0}, { h, h,-h, 1,0,0}, { h, h, h, 1,0,0}, { h,-h, h, 1,0,0}},
        {{-h,-h, h, -1,0,0}, {-h, h, h, -1,0,0}, {-h, h,-h, -1,0,0}, {-h,-h,-h, -1,0,0}},
        {{-h, h,-h, 0,1,0}, {-h, h, h, 0,1,0}, { h, h, h, 0,1,0}, { h, h,-h, 0,1,0}},
        {{-h,-h, h, 0,-1,0}, {-h,-h,-h, 0,-1,0}, { h,-h,-h, 0,-1,0}, { h,-h, h, 0,-1,0}},
        {{-h,-h, h, 0,0,1}, { h,-h, h, 0,0,1}, { h, h, h, 0,0,1}, {-h, h, h, 0,0,1}},
        {{ h,-h,-h, 0,0,-1}, {-h,-h,-h, 0,0,-1}, {-h, h,-h, 0,0,-1}, { h, h,-h, 0,0,-1}},
    };
    Mesh mesh;
    mesh.vertices.reserve(24 * 6);
    mesh.indices.reserve(36);
    for (uint32_t f = 0; f < 6; ++f) {
        for (const auto& v : faces[f]) mesh.vertices.insert(mesh.vertices.end(), v, v + 6);
        const uint32_t b = f * 4;
        mesh.indices.insert(mesh.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    return mesh;
}

Mesh makePlane() {
    Mesh mesh;
    pushVertex(&mesh, -0.5f, 0.0f, -0.5f, 0, 1, 0);
    pushVertex(&mesh, 0.5f, 0.0f, -0.5f, 0, 1, 0);
    pushVertex(&mesh, 0.5f, 0.0f, 0.5f, 0, 1, 0);
    pushVertex(&mesh, -0.5f, 0.0f, 0.5f, 0, 1, 0);
    mesh.indices = {0, 2, 1, 0, 3, 2};
    return mesh;
}

Mesh makeCylinder(uint32_t segments) {
    if (segments < 3) segments = 3;
    Mesh mesh;
    const float r = 0.5f, h = 0.5f;
    const float pi = 3.14159265358979323846f;
    // Side: 2 verts per segment step (bottom, top), duplicated seam.
    for (uint32_t i = 0; i <= segments; ++i) {
        const float a = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = r * std::cos(a), z = r * std::sin(a);
        pushVertex(&mesh, x, -h, z, std::cos(a), 0, std::sin(a));
        pushVertex(&mesh, x, h, z, std::cos(a), 0, std::sin(a));
    }
    for (uint32_t i = 0; i < segments; ++i) {
        const uint32_t b = i * 2;
        mesh.indices.insert(mesh.indices.end(), {b, b + 1, b + 2, b + 1, b + 3, b + 2});
    }
    // Caps: center + ring per cap.
    for (int cap = 0; cap < 2; ++cap) {
        const float y = cap == 0 ? -h : h;
        const float ny = cap == 0 ? -1.0f : 1.0f;
        const uint32_t center = mesh.vertexCount();
        pushVertex(&mesh, 0, y, 0, 0, ny, 0);
        const uint32_t ringStart = mesh.vertexCount();
        for (uint32_t i = 0; i < segments; ++i) {
            const float a = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
            pushVertex(&mesh, r * std::cos(a), y, r * std::sin(a), 0, ny, 0);
        }
        for (uint32_t i = 0; i < segments; ++i) {
            const uint32_t a = ringStart + i;
            const uint32_t b = ringStart + (i + 1) % segments;
            if (cap == 0) mesh.indices.insert(mesh.indices.end(), {center, b, a});
            else mesh.indices.insert(mesh.indices.end(), {center, a, b});
        }
    }
    return mesh;
}

Mesh makeSphere(uint32_t lon, uint32_t lat) {
    if (lon < 3) lon = 3;
    if (lat < 2) lat = 2;
    Mesh mesh;
    const float pi = 3.14159265358979323846f;
    for (uint32_t j = 0; j <= lat; ++j) {
        const float phi = pi * static_cast<float>(j) / static_cast<float>(lat);
        for (uint32_t i = 0; i <= lon; ++i) {
            const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(lon);
            const float x = std::sin(phi) * std::cos(theta);
            const float y = std::cos(phi);
            const float z = std::sin(phi) * std::sin(theta);
            pushVertex(&mesh, x * 0.5f, y * 0.5f, z * 0.5f, x, y, z);
        }
    }
    for (uint32_t j = 0; j < lat; ++j) {
        for (uint32_t i = 0; i < lon; ++i) {
            const uint32_t a = j * (lon + 1) + i;
            const uint32_t b = a + lon + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, a + 1, b, b + 1, a + 1});
        }
    }
    return mesh;
}

}  // namespace mobile3d
