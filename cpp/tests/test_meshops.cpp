#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "core/mesh.h"
#include "io/mesh_ops.h"
#include "io/obj_loader.h"

// Usage: test_meshops <tmp_dir>
int main(int argc, char** argv) {
    assert(argc > 1);
    const std::string dir = argv[1];
    std::string error;

    // Subdivision: 12 tris -> 48 -> 192, normals stay unit length.
    const auto cube = mobile3d::makeCube();
    const auto sub1 = mobile3d::subdivideMesh(cube, 1);
    assert(sub1.triangleCount() == 48);
    const auto sub2 = mobile3d::subdivideMesh(cube, 2);
    assert(sub2.triangleCount() == 192);
    for (uint32_t i = 0; i < sub1.vertexCount(); ++i) {
        const float* n = &sub1.vertices[i * 6] + 3;
        assert(std::fabs(std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) - 1.0f) < 1e-4f);
    }

    // Simplification: sphere to ~25% must reduce and stay non-empty/valid.
    const auto sphere = mobile3d::makeSphere(48, 36);
    mobile3d::Mesh simplified;
    bool ok = mobile3d::simplifyMesh(sphere, 0.25f, &simplified, &error);
    if (!ok) std::fprintf(stderr, "simplify failed: %s\n", error.c_str());
    assert(ok);
    assert(simplified.triangleCount() < sphere.triangleCount());
    assert(simplified.triangleCount() > sphere.triangleCount() / 8);
    for (uint32_t idx : simplified.indices) assert(idx < simplified.vertexCount());
    std::printf("simplify: %u -> %u tris\n", sphere.triangleCount(), simplified.triangleCount());

    // OBJ quad export: the cube's 12 coplanar triangles must merge to 6 quads.
    const std::string quadPath = dir + "/cube_quad.obj";
    ok = mobile3d::exportObj(cube, quadPath, true, &error);
    assert(ok);
    int quadFaces = 0, triFaces = 0;
    {
        std::ifstream in(quadPath);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("f ", 0) == 0) {
                int slashes = 0;
                for (char ch : line) if (ch == ' ') ++slashes;
                if (slashes == 4) ++quadFaces; else if (slashes == 3) ++triFaces;
            }
        }
    }
    assert(quadFaces == 6 && triFaces == 0);

    // The quad OBJ must load back to the same triangle count via fan split.
    mobile3d::Mesh back;
    ok = mobile3d::loadObj(quadPath, &back, &error);
    assert(ok);
    assert(back.triangleCount() == 12);

    // Quad export on a curved grid must still merge most triangle pairs.
    const std::string sphereQuadPath = dir + "/sphere_quad.obj";
    ok = mobile3d::exportObj(sphere, sphereQuadPath, true, &error);
    assert(ok);
    int sphereFaces = 0, sphereQuads = 0;
    {
        std::ifstream in(sphereQuadPath);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("f ", 0) == 0) {
                ++sphereFaces;
                int spaces = 0;
                for (char ch : line) if (ch == ' ') ++spaces;
                if (spaces == 4) ++sphereQuads;  // "f a//a b//b c//c d//d"
            }
        }
    }
    std::printf("sphere quad export: %d faces, %d quads (of %u tris)\n",
                sphereFaces, sphereQuads, sphere.triangleCount());
    assert(sphereQuads > 200);

    // Triangle OBJ: 12 tri faces.
    const std::string triPath = dir + "/cube_tri.obj";
    ok = mobile3d::exportObj(cube, triPath, false, &error);
    assert(ok);
    mobile3d::Mesh backTri;
    ok = mobile3d::loadObj(triPath, &backTri, &error);
    assert(ok);
    assert(backTri.triangleCount() == 12);

    std::printf("test_meshops: PASS\n");
    return 0;
}
