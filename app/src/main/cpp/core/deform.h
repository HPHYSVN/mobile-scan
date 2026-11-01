#pragma once

#include <cstdint>
#include <vector>

#include "mesh.h"

namespace mobile3d {

// M0 spike for sculpt performance questions (not a brush):
// deforms a local vertex region, recomputes its normals from face adjacency,
// and reports which vertex ranges are dirty for GPU upload.
// All timing is done by callers; this class only does the work.
class DeformRegion {
public:
    // Builds vertex->faces adjacency (needed for normal recomputation).
    void buildAdjacency(const Mesh& mesh);

    // Selects vertices whose normal has dot(normal, dir) > cosThreshold.
    // Returns count selected. Selection is sorted ascending (upload-friendly).
    uint32_t selectByNormal(const Mesh& mesh, float dx, float dy, float dz, float cosThreshold);

    // Displaces selected vertices along their base normals by `amount`
    // (relative to the positions captured in snapshotBase()).
    void snapshotBase(const Mesh& mesh);
    void apply(Mesh* mesh, float amount) const;

    // Recomputes normals of selected vertices using adjacency faces.
    void recomputeNormals(Mesh* mesh) const;

    const std::vector<uint32_t>& selected() const { return selected_; }
    uint32_t adjacencyBuildCount() const { return static_cast<uint32_t>(vertexFaces_.size()); }

private:
    std::vector<std::vector<uint32_t>> vertexFaces_;
    std::vector<uint32_t> selected_;
    std::vector<float> base_;  // interleaved copy of mesh vertices at snapshot time
};

}  // namespace mobile3d
