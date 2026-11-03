#pragma once

#include <cstdint>

#include "mesh.h"

namespace mobile3d {

struct RayHit {
    bool hit = false;
    float t = 0.0f;          // distance along ray (ray dir must be normalized)
    uint32_t face = 0;
    float point[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};  // face normal, normalized
};

// Moller-Trumbore against all triangles. Brute force: callers throttle
// stroke events; a BVH is the known next step (see RISK_REGISTER R1/R2 notes).
RayHit raycastMesh(const Mesh& mesh, const float origin[3], const float dir[3]);

}  // namespace mobile3d
