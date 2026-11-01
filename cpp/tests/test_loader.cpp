#include <cassert>
#include <cstdio>
#include <string>

#include "core/mesh.h"
#include "io/gltf_loader.h"

// Requires fixture path as argv[1] (wired via CTest with the 6k sphere).
int main(int argc, char** argv) {
    assert(argc > 1);
    mobile3d::Mesh mesh;
    std::string error;
    const bool ok = mobile3d::loadGltf(argv[1], &mesh, &error);
    if (!ok) std::fprintf(stderr, "load failed: %s\n", error.c_str());
    assert(ok);
    assert(mesh.vertexCount() == 3185);
    assert(mesh.triangleCount() == 6144);
    for (uint32_t idx : mesh.indices) assert(idx < mesh.vertexCount());

    mobile3d::Mesh bad;
    assert(!mobile3d::loadGltf("/nonexistent/missing.glb", &bad, &error));
    assert(!error.empty());

    std::printf("test_loader: PASS (verts=%u tris=%u)\n", mesh.vertexCount(), mesh.triangleCount());
    return 0;
}
