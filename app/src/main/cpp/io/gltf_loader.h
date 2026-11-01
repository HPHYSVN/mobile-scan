#pragma once

#include <string>

#include "../core/mesh.h"

namespace mobile3d {

// Loads the first primitive of the first mesh from a .glb/.gltf file.
// Spike scope (M0 D3): geometry only. Materials/textures are ignored and
// missing normals are generated flat. Returns false and sets `error` on failure.
bool loadGltf(const std::string& path, Mesh* out, std::string* error);

}  // namespace mobile3d
