#pragma once

#include <string>

#include "../core/mesh.h"

namespace mobile3d {

// Recomputes smooth (area-weighted) normals in place.
void recomputeSmoothNormals(Mesh* mesh);

// Midpoint subdivision: each triangle becomes 4. Levels above 3 are clamped.
Mesh subdivideMesh(const Mesh& mesh, int levels);

// Simplifies towards targetRatio (0..1) of the current triangle count using
// meshoptimizer (MIT). Returns false if the result would be empty.
bool simplifyMesh(const Mesh& mesh, float targetRatio, Mesh* out, std::string* error);

// Wavefront OBJ export. If mergeQuads, coplanar adjacent triangle pairs are
// merged into quad faces (f v//v quads); otherwise all faces are triangles.
bool exportObj(const Mesh& mesh, const std::string& path, bool mergeQuads, std::string* error);

}  // namespace mobile3d
