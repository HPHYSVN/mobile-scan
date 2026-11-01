#pragma once

#include <string>

#include "scene.h"

namespace mobile3d {

// Project directory format (see ARCHITECTURE.md section 5):
//   <dir>/manifest.json  {"schemaVersion":1, ...}
//   <dir>/scene.json     {"nodes":[...]}
//   <dir>/meshes/        mesh files referenced by Node::meshRef
// Mesh files themselves are copied by the caller; this module owns only
// manifest + scene so it stays host-testable without asset plumbing.
inline constexpr int kProjectSchemaVersion = 1;

bool saveProject(const std::string& dir, const Scene& scene, std::string* error);
bool loadProject(const std::string& dir, Scene* out, std::string* error);

}  // namespace mobile3d
