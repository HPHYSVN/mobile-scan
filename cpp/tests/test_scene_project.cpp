#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/mesh.h"
#include "core/project.h"
#include "core/scene.h"

namespace fs = std::filesystem;

static bool nearly(float a, float b) { return std::fabs(a - b) < 1e-6f; }

int main(int argc, char** argv) {
    assert(argc > 1);
    const std::string dir = std::string(argv[1]) + "/sample_project";

    // --- primitives -------------------------------------------------------
    const auto plane = mobile3d::makePlane();
    assert(plane.vertexCount() == 4 && plane.triangleCount() == 2);
    const auto cylinder = mobile3d::makeCylinder(24);
    assert(cylinder.triangleCount() == 24 * 2 + 24 * 2);  // sides + caps
    const auto sphere = mobile3d::makeSphere(32, 24);
    assert(sphere.triangleCount() == 32 * 24 * 2);
    for (uint32_t i = 0; i < sphere.vertexCount(); ++i) {
        const float* n = &sphere.vertices[i * 6] + 3;
        assert(nearly(std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]), 1.0f));
    }

    // --- transform --------------------------------------------------------
    mobile3d::Transform t;
    t.position[0] = 1; t.position[1] = 2; t.position[2] = 3;
    t.scale[0] = t.scale[1] = t.scale[2] = 2;
    float m[16];
    t.toMatrix(m);
    assert(nearly(m[0], 2) && nearly(m[5], 2) && nearly(m[10], 2));
    assert(nearly(m[12], 1) && nearly(m[13], 2) && nearly(m[14], 3) && nearly(m[15], 1));

    mobile3d::Transform rot;
    rot.rotationEuler[2] = 3.14159265358979f;  // 180 deg about Z flips X
    rot.toMatrix(m);
    assert(nearly(m[0], -1) && nearly(m[5], -1) && nearly(m[10], 1));

    // --- scene + project round-trip --------------------------------------
    mobile3d::Scene scene;
    const uint32_t a = scene.addNode("Cube A", "primitive:cube");
    const uint32_t b = scene.addNode("Scan", "meshes/scan.glb");
    assert(a != b);
    auto* nodeB = scene.find(b);
    nodeB->color[0] = 0.1f; nodeB->color[1] = 0.2f; nodeB->color[2] = 0.3f;
    nodeB->transform.position[0] = -1.5f;
    nodeB->transform.rotationEuler[1] = 0.5f;
    nodeB->visible = false;
    assert(scene.removeNode(a));
    assert(!scene.removeNode(9999));

    std::string error;
    bool ok = mobile3d::saveProject(dir, scene, &error);
    if (!ok) std::fprintf(stderr, "save failed: %s\n", error.c_str());
    assert(ok);
    assert(fs::exists(fs::path(dir) / "manifest.json"));
    assert(fs::exists(fs::path(dir) / "scene.json"));
    assert(fs::exists(fs::path(dir) / "meshes"));

    mobile3d::Scene loaded;
    ok = mobile3d::loadProject(dir, &loaded, &error);
    if (!ok) std::fprintf(stderr, "load failed: %s\n", error.c_str());
    assert(ok);
    assert(loaded.nodes().size() == 1);
    const auto* ln = loaded.find(b);
    assert(ln != nullptr);
    assert(ln->name == "Scan" && ln->meshRef == "meshes/scan.glb" && !ln->visible);
    assert(ln->transform.equals(nodeB->transform));
    assert(nearly(ln->color[0], 0.1f) && nearly(ln->color[1], 0.2f) && nearly(ln->color[2], 0.3f));
    // New nodes must not collide with loaded ids.
    const uint32_t c = loaded.addNode("New", "primitive:sphere");
    assert(c > b);

    // --- error paths ------------------------------------------------------
    mobile3d::Scene empty;
    assert(!mobile3d::loadProject(std::string(argv[1]) + "/does_not_exist", &empty, &error));
    assert(!error.empty());

    // Newer schema is rejected, not silently mangled.
    const std::string futureDir = std::string(argv[1]) + "/future_project";
    fs::create_directories(futureDir);
    { std::ofstream(futureDir + "/manifest.json") << "{\"schemaVersion\": 99}"; }
    { std::ofstream(futureDir + "/scene.json") << "{\"nodes\": []}"; }
    assert(!mobile3d::loadProject(futureDir, &empty, &error));
    assert(error.find("newer") != std::string::npos);

    std::printf("test_scene_project: PASS\n");
    return 0;
}
