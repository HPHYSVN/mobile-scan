#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "core/mesh.h"
#include "io/gltf_exporter.h"
#include "io/gltf_loader.h"
#include "io/obj_loader.h"

// Usage: test_io <tmp_dir>
int main(int argc, char** argv) {
    assert(argc > 1);
    const std::string dir = argv[1];
    std::string error;

    // GLB round-trip: cube -> export -> load -> same counts and bounds.
    const mobile3d::Mesh cube = mobile3d::makeCube();
    const std::string glbPath = dir + "/roundtrip.glb";
    bool ok = mobile3d::exportGlb(cube, glbPath, &error);
    if (!ok) std::fprintf(stderr, "export failed: %s\n", error.c_str());
    assert(ok);

    mobile3d::Mesh loaded;
    ok = mobile3d::loadGltf(glbPath, &loaded, &error);
    assert(ok);
    assert(loaded.vertexCount() == cube.vertexCount());
    assert(loaded.triangleCount() == cube.triangleCount());
    for (size_t i = 0; i < cube.vertices.size(); ++i) {
        assert(std::fabs(loaded.vertices[i] - cube.vertices[i]) < 1e-6f);
    }

    // OBJ: quad (fan triangulation -> 2 tris) with normals, negative index face.
    const std::string objPath = dir + "/quad.obj";
    {
        std::ofstream out(objPath);
        out << "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
            << "vn 0 0 1\n"
            << "f 1//1 2//1 3//1 4//1\n"
            << "f -4//1 -3//1 -2//1\n";  // same quad area via negative indices
    }
    mobile3d::Mesh obj;
    ok = mobile3d::loadObj(objPath, &obj, &error);
    if (!ok) std::fprintf(stderr, "obj load failed: %s\n", error.c_str());
    assert(ok);
    assert(obj.triangleCount() == 4);  // 2 from quad fan + 2 from second quad
    for (uint32_t i = 0; i < obj.vertexCount(); ++i) {
        const float* n = &obj.vertices[i * 6] + 3;
        assert(std::fabs(n[2] - 1.0f) < 1e-6f);
    }

    // OBJ without normals: computed smooth normals must be unit length.
    const std::string objPath2 = dir + "/tri.obj";
    {
        std::ofstream out(objPath2);
        out << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    }
    mobile3d::Mesh tri;
    ok = mobile3d::loadObj(objPath2, &tri, &error);
    assert(ok);
    assert(tri.triangleCount() == 1);
    const float* n = &tri.vertices[3];
    assert(std::fabs(std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) - 1.0f) < 1e-5f);

    std::printf("test_io: PASS (glb roundtrip + obj quad/tri)\n");
    return 0;
}
