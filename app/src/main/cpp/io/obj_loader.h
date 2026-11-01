#pragma once

#include <string>

#include "../core/mesh.h"

namespace mobile3d {

// Minimal Wavefront OBJ loader (M0 D8 scope): v/vn/f lines, polygonal faces
// triangulated as a fan, negative indices supported. Vertices are expanded
// (non-indexed source combos become unique verts). Missing normals are
// computed smooth. Materials/groups/textures are ignored.
bool loadObj(const std::string& path, Mesh* out, std::string* error);

}  // namespace mobile3d
