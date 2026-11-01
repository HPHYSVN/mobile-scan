#include "obj_loader.h"

#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace mobile3d {
namespace {

int resolveIndex(int idx, size_t count) {
    if (idx > 0) return idx - 1;
    if (idx < 0) return static_cast<int>(count) + idx;
    return -1;
}

}  // namespace

bool loadObj(const std::string& path, Mesh* out, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        if (error) *error = "cannot open " + path;
        return false;
    }

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;
    // Key: (position index, normal index or -1) -> output vertex.
    std::map<std::pair<int, int>, uint32_t> remap;
    Mesh mesh;

    auto emitVertex = [&](int pi, int ni) -> uint32_t {
        const auto key = std::make_pair(pi, ni);
        const auto it = remap.find(key);
        if (it != remap.end()) return it->second;
        const auto& p = positions[pi];
        mesh.vertices.insert(mesh.vertices.end(), {p[0], p[1], p[2]});
        if (ni >= 0) {
            const auto& n = normals[ni];
            mesh.vertices.insert(mesh.vertices.end(), {n[0], n[1], n[2]});
        } else {
            mesh.vertices.insert(mesh.vertices.end(), {0.0f, 0.0f, 0.0f});
        }
        const uint32_t index = mesh.vertexCount() - 1;
        remap[key] = index;
        return index;
    };

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            std::array<float, 3> p{};
            ss >> p[0] >> p[1] >> p[2];
            positions.push_back(p);
        } else if (tag == "vn") {
            std::array<float, 3> n{};
            ss >> n[0] >> n[1] >> n[2];
            normals.push_back(n);
        } else if (tag == "f") {
            std::vector<uint32_t> face;
            std::string token;
            while (ss >> token) {
                // token forms: v, v/vt, v//vn, v/vt/vn
                int vi = 0, ni = -1;
                const size_t slash1 = token.find('/');
                vi = std::stoi(token.substr(0, slash1));
                if (slash1 != std::string::npos) {
                    const size_t slash2 = token.find('/', slash1 + 1);
                    if (slash2 != std::string::npos && slash2 + 1 < token.size()) {
                        ni = resolveIndex(std::stoi(token.substr(slash2 + 1)), normals.size());
                    }
                }
                const int pi = resolveIndex(vi, positions.size());
                if (pi < 0 || pi >= static_cast<int>(positions.size())) continue;
                face.push_back(emitVertex(pi, ni));
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                mesh.indices.push_back(face[0]);
                mesh.indices.push_back(face[i]);
                mesh.indices.push_back(face[i + 1]);
            }
        }
    }

    if (mesh.vertexCount() == 0 || mesh.indices.empty()) {
        if (error) *error = "no geometry in " + path;
        return false;
    }

    // If any normal is zero (source had no vn for it), recompute smooth normals for all.
    bool needsNormals = false;
    for (uint32_t i = 0; i < mesh.vertexCount() && !needsNormals; ++i) {
        const float* n = &mesh.vertices[i * 6] + 3;
        needsNormals = (n[0] == 0.0f && n[1] == 0.0f && n[2] == 0.0f);
    }
    if (needsNormals) {
        for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
            float* n = &mesh.vertices[i * 6] + 3;
            n[0] = n[1] = n[2] = 0.0f;
        }
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            float* a = &mesh.vertices[mesh.indices[i] * 6];
            float* b = &mesh.vertices[mesh.indices[i + 1] * 6];
            float* c = &mesh.vertices[mesh.indices[i + 2] * 6];
            const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
            const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
            const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            for (int k = 0; k < 3; ++k) {
                float* v = &mesh.vertices[mesh.indices[i + k] * 6];
                v[3] += nx; v[4] += ny; v[5] += nz;
            }
        }
        for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
            float* n = &mesh.vertices[i * 6] + 3;
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }
        }
    }

    *out = std::move(mesh);
    return true;
}

}  // namespace mobile3d
