#include "mesh_ops.h"

#ifdef __ANDROID__
#include <android/log.h>
#define MLOG(...) __android_log_print(ANDROID_LOG_INFO, "Mobile3D", __VA_ARGS__)
#else
#define MLOG(...) ((void)0)
#endif

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>

#include "../third_party/meshoptimizer/meshoptimizer.h"

namespace mobile3d {

void recomputeSmoothNormals(Mesh* mesh) {
    for (uint32_t i = 0; i < mesh->vertexCount(); ++i) {
        float* n = &mesh->vertices[i * 6] + 3;
        n[0] = n[1] = n[2] = 0.0f;
    }
    for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        float* a = &mesh->vertices[mesh->indices[i] * 6];
        float* b = &mesh->vertices[mesh->indices[i + 1] * 6];
        float* c = &mesh->vertices[mesh->indices[i + 2] * 6];
        const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
        const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
        const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        for (int k = 0; k < 3; ++k) {
            float* v = &mesh->vertices[mesh->indices[i + k] * 6];
            v[3] += nx; v[4] += ny; v[5] += nz;
        }
    }
    for (uint32_t i = 0; i < mesh->vertexCount(); ++i) {
        float* n = &mesh->vertices[i * 6] + 3;
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }
    }
}

Mesh subdivideMesh(const Mesh& mesh, int levels) {
    Mesh current = mesh;
    levels = std::max(0, std::min(levels, 3));
    for (int level = 0; level < levels; ++level) {
        Mesh next;
        next.vertices = current.vertices;
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpoints;
        auto midpoint = [&next, &midpoints](const Mesh& src, uint32_t a, uint32_t b) -> uint32_t {
            const auto key = std::minmax(a, b);
            const auto it = midpoints.find(key);
            if (it != midpoints.end()) return it->second;
            const float* pa = &src.vertices[a * 6];
            const float* pb = &src.vertices[b * 6];
            for (int k = 0; k < 3; ++k) {
                next.vertices.push_back((pa[k] + pb[k]) * 0.5f);
            }
            for (int k = 0; k < 3; ++k) next.vertices.push_back(0.0f);
            const uint32_t index = next.vertexCount() - 1;
            midpoints[key] = index;
            return index;
        };
        for (size_t i = 0; i + 2 < current.indices.size(); i += 3) {
            const uint32_t a = current.indices[i], b = current.indices[i + 1], c = current.indices[i + 2];
            const uint32_t ab = midpoint(current, a, b);
            const uint32_t bc = midpoint(current, b, c);
            const uint32_t ca = midpoint(current, c, a);
            next.indices.insert(next.indices.end(), {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca});
        }
        recomputeSmoothNormals(&next);
        current = std::move(next);
    }
    return current;
}

bool simplifyMesh(const Mesh& mesh, float targetRatio, Mesh* out, std::string* error) {
    targetRatio = std::max(0.02f, std::min(targetRatio, 1.0f));
    const size_t targetIndices = static_cast<size_t>(mesh.indices.size() * targetRatio) / 3 * 3;
    std::vector<unsigned int> simplified(mesh.indices.size());
    float resultError = 0.0f;
    const size_t written = meshopt_simplify(
        simplified.data(), mesh.indices.data(), mesh.indices.size(),
        mesh.vertices.data(), mesh.vertexCount(), 6 * sizeof(float),
        targetIndices, 1.0f, 0, &resultError);
    if (written == 0) {
        if (error) *error = "meshoptimizer produced empty mesh";
        return false;
    }
    simplified.resize(written);

    // Compact away vertices no longer referenced.
    std::vector<uint32_t> remap(mesh.vertexCount(), UINT32_MAX);
    Mesh result;
    for (unsigned int idx : simplified) {
        if (remap[idx] == UINT32_MAX) {
            remap[idx] = result.vertexCount();
            const float* v = &mesh.vertices[idx * 6];
            result.vertices.insert(result.vertices.end(), v, v + 6);
        }
        result.indices.push_back(remap[idx]);
    }
    (void)resultError;
    *out = std::move(result);
    return true;
}

bool exportObj(const Mesh& mesh, const std::string& path, bool mergeQuads, std::string* error) {
    std::ofstream outFile(path);
    if (!outFile) {
        if (error) *error = "cannot write " + path;
        return false;
    }
    outFile << "# exported by mobile3d validation build\n";
    for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
        const float* v = &mesh.vertices[i * 6];
        outFile << "v " << v[0] << ' ' << v[1] << ' ' << v[2] << "\n";
    }
    for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
        const float* n = &mesh.vertices[i * 6] + 3;
        outFile << "vn " << n[0] << ' ' << n[1] << ' ' << n[2] << "\n";
    }

    const uint32_t faceCount = mesh.triangleCount();
    std::vector<char> used(faceCount, 0);

    if (mergeQuads) {
        // Map undirected edge -> faces, then greedily merge coplanar pairs.
        std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> edgeFaces;
        // Manual pair (std::minmax returns a reference pair whose behaviour
        // differed under libc++ on Android: edge keys collapsed, see debug log).
        auto edgeKey = [](uint32_t a, uint32_t b) {
            return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        };
        for (uint32_t f = 0; f < faceCount; ++f) {
            for (int k = 0; k < 3; ++k) {
                edgeFaces[edgeKey(mesh.indices[f * 3 + k], mesh.indices[f * 3 + (k + 1) % 3])].push_back(f);
            }
        }
        auto faceNormal = [&mesh](uint32_t f, float n[3]) {
            const float* a = &mesh.vertices[mesh.indices[f * 3] * 6];
            const float* b = &mesh.vertices[mesh.indices[f * 3 + 1] * 6];
            const float* c = &mesh.vertices[mesh.indices[f * 3 + 2] * 6];
            const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
            const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
            n[0] = uy * vz - uz * vy; n[1] = uz * vx - ux * vz; n[2] = ux * vy - uy * vx;
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }
        };

        for (uint32_t f = 0; f < faceCount; ++f) {
            if (used[f]) continue;
            bool merged = false;
            float nf[3];
            faceNormal(f, nf);
            for (int k = 0; k < 3 && !merged; ++k) {
                const uint32_t a = mesh.indices[f * 3 + k];
                const uint32_t b = mesh.indices[f * 3 + (k + 1) % 3];
                const uint32_t c = mesh.indices[f * 3 + (k + 2) % 3];
                for (uint32_t g : edgeFaces[edgeKey(a, b)]) {
                    if (g == f || used[g]) continue;
                    float ng[3];
                    faceNormal(g, ng);
                    // Quad meshes in DCC tools routinely contain slightly
                    // non-planar quads; 0.98 (~11 deg) merges curved grids
                    // without folding visibly different faces together.
                    const float dot = nf[0] * ng[0] + nf[1] * ng[1] + nf[2] * ng[2];
                    if (dot < 0.98f) continue;
                    // Fourth vertex of g (not on the shared edge).
                    uint32_t d = UINT32_MAX;
                    for (int m = 0; m < 3; ++m) {
                        const uint32_t v = mesh.indices[g * 3 + m];
                        if (v != a && v != b) d = v;
                    }
                    if (d == UINT32_MAX || d == c) continue;
                    // Quad loop: c -> a -> d -> b (fan-triangulates back cleanly).
                    outFile << "f " << c + 1 << "//" << c + 1 << ' ' << a + 1 << "//" << a + 1 << ' '
                            << d + 1 << "//" << d + 1 << ' ' << b + 1 << "//" << b + 1 << "\n";
                    used[f] = 1;
                    used[g] = 1;
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                // Emitted below with the remaining triangles.
            }
        }
    }

    {
        uint32_t mergedCount = 0;
        for (char u : used) mergedCount += u ? 1 : 0;
        MLOG("exportObj mergeQuads=%d faces=%u usedVerts=%u mergedFaces=%u",
             mergeQuads ? 1 : 0, faceCount, mesh.vertexCount(), mergedCount);
    }

    for (uint32_t f = 0; f < faceCount; ++f) {
        if (used[f]) continue;
        outFile << "f";
        for (int k = 0; k < 3; ++k) {
            const uint32_t idx = mesh.indices[f * 3 + k] + 1;
            outFile << ' ' << idx << "//" << idx;
        }
        outFile << "\n";
    }
    return static_cast<bool>(outFile);
}

}  // namespace mobile3d
