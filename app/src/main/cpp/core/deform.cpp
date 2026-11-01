#include "deform.h"

#include <cmath>

namespace mobile3d {

void DeformRegion::buildAdjacency(const Mesh& mesh) {
    vertexFaces_.assign(mesh.vertexCount(), {});
    const uint32_t faceCount = mesh.triangleCount();
    for (uint32_t f = 0; f < faceCount; ++f) {
        for (int k = 0; k < 3; ++k) {
            vertexFaces_[mesh.indices[f * 3 + k]].push_back(f);
        }
    }
}

uint32_t DeformRegion::selectByNormal(const Mesh& mesh, float dx, float dy, float dz, float cosThreshold) {
    selected_.clear();
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len == 0.0f) return 0;
    dx /= len; dy /= len; dz /= len;
    for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
        const float* n = &mesh.vertices[i * 6] + 3;
        if (n[0] * dx + n[1] * dy + n[2] * dz > cosThreshold) selected_.push_back(i);
    }
    return static_cast<uint32_t>(selected_.size());
}

void DeformRegion::snapshotBase(const Mesh& mesh) { base_ = mesh.vertices; }

void DeformRegion::apply(Mesh* mesh, float amount) const {
    if (base_.size() != mesh->vertices.size()) return;
    for (uint32_t idx : selected_) {
        float* dst = &mesh->vertices[idx * 6];
        const float* base = &base_[idx * 6];
        dst[0] = base[0] + base[3] * amount;
        dst[1] = base[1] + base[4] * amount;
        dst[2] = base[2] + base[5] * amount;
    }
}

void DeformRegion::recomputeNormals(Mesh* mesh) const {
    if (vertexFaces_.size() != mesh->vertexCount()) return;
    // Zero affected normals, accumulate from adjacent faces, normalize.
    for (uint32_t idx : selected_) {
        float* n = &mesh->vertices[idx * 6] + 3;
        n[0] = n[1] = n[2] = 0.0f;
    }
    // For each selected vertex, sum normals of its adjacent faces.
    for (uint32_t idx : selected_) {
        float* n = &mesh->vertices[idx * 6] + 3;
        for (uint32_t f : vertexFaces_[idx]) {
            const float* a = &mesh->vertices[mesh->indices[f * 3] * 6];
            const float* b = &mesh->vertices[mesh->indices[f * 3 + 1] * 6];
            const float* c = &mesh->vertices[mesh->indices[f * 3 + 2] * 6];
            const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
            const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
            n[0] += uy * vz - uz * vy;
            n[1] += uz * vx - ux * vz;
            n[2] += ux * vy - uy * vx;
        }
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }
    }
}

}  // namespace mobile3d
