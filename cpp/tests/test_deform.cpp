#include <cassert>
#include <cmath>
#include <cstdio>

#include "core/deform.h"
#include "core/mesh.h"

int main() {
    mobile3d::Mesh cube = mobile3d::makeCube();
    mobile3d::DeformRegion deform;
    deform.buildAdjacency(cube);
    assert(deform.adjacencyBuildCount() == 24);

    // +Z face of the cube has normal (0,0,1): 4 vertices.
    const uint32_t count = deform.selectByNormal(cube, 0, 0, 1, 0.9f);
    assert(count == 4);

    deform.snapshotBase(cube);
    deform.apply(&cube, 0.25f);
    for (uint32_t idx : deform.selected()) {
        const float* p = &cube.vertices[idx * 6];
        assert(std::fabs(p[2] - 0.75f) < 1e-6f);
    }
    // Untouched -Z face stays at z=-0.5.
    bool sawBack = false;
    for (uint32_t i = 0; i < cube.vertexCount(); ++i) {
        const float* n = &cube.vertices[i * 6] + 3;
        if (n[0] == 0.0f && n[1] == 0.0f && n[2] == -1.0f) {
            assert(std::fabs(cube.vertices[i * 6 + 2] + 0.5f) < 1e-6f);
            sawBack = true;
        }
    }
    assert(sawBack);

    deform.recomputeNormals(&cube);
    for (uint32_t idx : deform.selected()) {
        const float* n = &cube.vertices[idx * 6] + 3;
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        assert(std::fabs(len - 1.0f) < 1e-4f);
        assert(n[2] > 0.9f);  // still roughly +Z after symmetric displacement
    }

    std::printf("test_deform: PASS (region=%u)\n", count);
    return 0;
}
