#pragma once

#include <string>

#include "../core/mesh.h"

namespace mobile3d {

// Exports a mesh as binary glTF (.glb): single primitive, POSITION + NORMAL,
// uint32 indices. No materials/textures (matches loader spike scope).
bool exportGlb(const Mesh& mesh, const std::string& path, std::string* error);

}  // namespace mobile3d
