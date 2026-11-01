#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/mesh.h"
#include "io/gltf_loader.h"

// Usage: bench_import <file.glb> [runs]
// Prints one line per run plus median. Host numbers only - never report as device results.
int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: bench_import <file.glb> [runs]\n");
        return 2;
    }
    const int runs = argc > 2 ? std::stoi(argv[2]) : 3;
    std::vector<double> times;
    for (int i = 0; i < runs; ++i) {
        mobile3d::Mesh mesh;
        std::string error;
        const auto begin = std::chrono::steady_clock::now();
        if (!mobile3d::loadGltf(argv[1], &mesh, &error)) {
            std::fprintf(stderr, "load failed: %s\n", error.c_str());
            return 1;
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        times.push_back(ms);
        std::printf("run %d: %.2f ms (verts=%u tris=%u)\n", i + 1, ms, mesh.vertexCount(), mesh.triangleCount());
    }
    std::sort(times.begin(), times.end());
    std::printf("median: %.2f ms\n", times[times.size() / 2]);
    return 0;
}
