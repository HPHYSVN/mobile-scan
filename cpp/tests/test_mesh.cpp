#include <cassert>
#include <cmath>
#include <cstdio>

#include "core/mesh.h"

int main() {
    const mobile3d::Mesh cube = mobile3d::makeCube();
    assert(cube.vertexCount() == 24);
    assert(cube.triangleCount() == 12);
    assert(cube.indices.size() == 36);

    for (uint32_t idx : cube.indices) assert(idx < cube.vertexCount());

    // Normals must be unit length and positions inside the unit cube.
    for (uint32_t v = 0; v < cube.vertexCount(); ++v) {
        const float* p = &cube.vertices[v * 6];
        const float* n = p + 3;
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        assert(std::fabs(len - 1.0f) < 1e-5f);
        for (int i = 0; i < 3; ++i) assert(std::fabs(p[i]) <= 0.5f + 1e-6f);
    }

    std::printf("test_mesh: PASS (verts=%u tris=%u)\n", cube.vertexCount(), cube.triangleCount());
    return 0;
}
