#include "mesh.h"

namespace mobile3d {

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

}  // namespace mobile3d
