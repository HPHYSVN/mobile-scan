#include "raycast.h"

#include <cmath>
#include <limits>

namespace mobile3d {

RayHit raycastMesh(const Mesh& mesh, const float origin[3], const float dir[3]) {
    RayHit best;
    best.t = std::numeric_limits<float>::max();
    const float eps = 1e-7f;

    for (uint32_t f = 0; f < mesh.triangleCount(); ++f) {
        const float* a = &mesh.vertices[mesh.indices[f * 3] * 6];
        const float* b = &mesh.vertices[mesh.indices[f * 3 + 1] * 6];
        const float* c = &mesh.vertices[mesh.indices[f * 3 + 2] * 6];

        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float p[3] = {dir[1] * e2[2] - dir[2] * e2[1],
                            dir[2] * e2[0] - dir[0] * e2[2],
                            dir[0] * e2[1] - dir[1] * e2[0]};
        const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
        if (det > -eps && det < eps) continue;
        const float inv = 1.0f / det;
        const float tv[3] = {origin[0] - a[0], origin[1] - a[1], origin[2] - a[2]};
        const float u = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) * inv;
        if (u < 0.0f || u > 1.0f) continue;
        const float q[3] = {tv[1] * e1[2] - tv[2] * e1[1],
                            tv[2] * e1[0] - tv[0] * e1[2],
                            tv[0] * e1[1] - tv[1] * e1[0]};
        const float v = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
        if (v < 0.0f || u + v > 1.0f) continue;
        const float t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
        if (t < eps || t >= best.t) continue;

        best.hit = true;
        best.t = t;
        best.face = f;
        for (int k = 0; k < 3; ++k) best.point[k] = origin[k] + dir[k] * t;
        float n[3] = {e1[1] * e2[2] - e1[2] * e2[1],
                      e1[2] * e2[0] - e1[0] * e2[2],
                      e1[0] * e2[1] - e1[1] * e2[0]};
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }
        for (int k = 0; k < 3; ++k) best.normal[k] = n[k];
    }
    if (!best.hit) best.t = 0.0f;
    return best;
}

}  // namespace mobile3d
