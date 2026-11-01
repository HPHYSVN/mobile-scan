#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/deform.h"
#include "core/mesh.h"
#include "io/gltf_loader.h"

using Clock = std::chrono::steady_clock;

static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Usage: bench_deform <file.glb> [iterations]
// Measures adjacency build, CPU deform, and normal recomputation separately.
// GPU upload cannot be measured on host; see Android stats. Host != device.
int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: bench_deform <file.glb> [iterations]\n");
        return 2;
    }
    const int iterations = argc > 2 ? std::stoi(argv[2]) : 60;

    mobile3d::Mesh mesh;
    std::string error;
    if (!mobile3d::loadGltf(argv[1], &mesh, &error)) {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }

    mobile3d::DeformRegion deform;
    const auto adjBegin = Clock::now();
    deform.buildAdjacency(mesh);
    std::printf("verts=%u tris=%u adjacency=%.2f ms\n", mesh.vertexCount(), mesh.triangleCount(),
                ms(adjBegin, Clock::now()));

    float threshold = 0.99f;
    while (threshold > 0.0f && deform.selectByNormal(mesh, 0.3f, 0.7f, 0.65f, threshold) < 4000) threshold -= 0.02f;
    deform.snapshotBase(mesh);
    std::printf("region=%u verts (threshold %.2f), iterations=%d\n",
                static_cast<unsigned>(deform.selected().size()), threshold, iterations);

    std::vector<double> deformTimes, normalTimes;
    for (int i = 0; i < iterations; ++i) {
        const float amount = (i % 2 == 0) ? 0.06f : -0.06f;
        const auto t0 = Clock::now();
        deform.apply(&mesh, amount);
        const auto t1 = Clock::now();
        deform.recomputeNormals(&mesh);
        const auto t2 = Clock::now();
        deformTimes.push_back(ms(t0, t1));
        normalTimes.push_back(ms(t1, t2));
    }
    auto median = [](std::vector<double> v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    std::printf("deform median: %.3f ms | normals median: %.3f ms | combined: %.3f ms\n",
                median(deformTimes), median(normalTimes), median(deformTimes) + median(normalTimes));
    return 0;
}
