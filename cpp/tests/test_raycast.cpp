#include <cassert>
#include <cmath>
#include <cstdio>

#include "core/mesh.h"
#include "core/raycast.h"

int main() {
    const mobile3d::Mesh cube = mobile3d::makeCube();

    // Ray from +Z towards origin must hit the +Z face at z=0.5.
    const float origin[3] = {0.0f, 0.0f, 3.0f};
    const float dir[3] = {0.0f, 0.0f, -1.0f};
    const auto hit = mobile3d::raycastMesh(cube, origin, dir);
    assert(hit.hit);
    assert(std::fabs(hit.t - 2.5f) < 1e-5f);
    assert(std::fabs(hit.point[2] - 0.5f) < 1e-5f);
    assert(std::fabs(hit.normal[2] - 1.0f) < 1e-5f);

    // Ray missing the cube.
    const float origin2[3] = {3.0f, 3.0f, 3.0f};
    assert(!mobile3d::raycastMesh(cube, origin2, dir).hit);

    // Ray from inside pointing +X hits at x=0.5.
    const float inside[3] = {0.0f, 0.0f, 0.0f};
    const float dirX[3] = {1.0f, 0.0f, 0.0f};
    const auto hitX = mobile3d::raycastMesh(cube, inside, dirX);
    assert(hitX.hit && std::fabs(hitX.point[0] - 0.5f) < 1e-5f);

    std::printf("test_raycast: PASS\n");
    return 0;
}
