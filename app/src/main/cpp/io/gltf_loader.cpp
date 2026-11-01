#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "../third_party/tinygltf/tiny_gltf.h"

#include "gltf_loader.h"

#include <cmath>
#include <cstring>

namespace mobile3d {
namespace {

template <typename T>
const T* accessorData(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const size_t offset = view.byteOffset + accessor.byteOffset;
    return reinterpret_cast<const T*>(buffer.data.data() + offset);
}

size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const int componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const int components = tinygltf::GetNumComponentsInType(accessor.type);
    return view.byteStride ? view.byteStride : static_cast<size_t>(componentSize * components);
}

}  // namespace

bool loadGltf(const std::string& path, Mesh* out, std::string* error) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    const bool isGlb = path.size() > 4 && path.compare(path.size() - 4, 4, ".glb") == 0;
    const bool ok = isGlb ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
                          : loader.LoadASCIIFromFile(&model, &err, &warn, path);
    if (!ok) {
        if (error) *error = err;
        return false;
    }
    if (model.meshes.empty() || model.meshes[0].primitives.empty()) {
        if (error) *error = "no mesh primitive";
        return false;
    }
    const auto& prim = model.meshes[0].primitives[0];
    const auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) {
        if (error) *error = "missing POSITION";
        return false;
    }

    const auto& posAccessor = model.accessors[posIt->second];
    const float* positions = accessorData<float>(model, posAccessor);
    const size_t posStride = accessorStride(model, posAccessor);
    const size_t vertexCount = posAccessor.count;

    const float* normals = nullptr;
    size_t nrmStride = 0;
    const auto nrmIt = prim.attributes.find("NORMAL");
    if (nrmIt != prim.attributes.end()) {
        const auto& nrmAccessor = model.accessors[nrmIt->second];
        normals = accessorData<float>(model, nrmAccessor);
        nrmStride = accessorStride(model, nrmAccessor);
    }

    Mesh mesh;
    mesh.vertices.resize(vertexCount * 6);
    for (size_t i = 0; i < vertexCount; ++i) {
        const float* p = reinterpret_cast<const float*>(reinterpret_cast<const char*>(positions) + i * posStride);
        float* dst = &mesh.vertices[i * 6];
        dst[0] = p[0]; dst[1] = p[1]; dst[2] = p[2];
        if (normals) {
            const float* n = reinterpret_cast<const float*>(reinterpret_cast<const char*>(normals) + i * nrmStride);
            dst[3] = n[0]; dst[4] = n[1]; dst[5] = n[2];
        }
    }

    if (prim.indices >= 0) {
        const auto& idxAccessor = model.accessors[prim.indices];
        mesh.indices.resize(idxAccessor.count);
        if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const auto* data = accessorData<uint32_t>(model, idxAccessor);
            std::memcpy(mesh.indices.data(), data, idxAccessor.count * sizeof(uint32_t));
        } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const auto* data = accessorData<uint16_t>(model, idxAccessor);
            for (size_t i = 0; i < idxAccessor.count; ++i) mesh.indices[i] = data[i];
        } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            const auto* data = accessorData<uint8_t>(model, idxAccessor);
            for (size_t i = 0; i < idxAccessor.count; ++i) mesh.indices[i] = data[i];
        } else {
            if (error) *error = "unsupported index component type";
            return false;
        }
    } else {
        mesh.indices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) mesh.indices[i] = static_cast<uint32_t>(i);
    }

    if (!normals) {
        // Flat normals: accumulate face normals, then normalize.
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
        for (size_t i = 0; i < vertexCount; ++i) {
            float* n = &mesh.vertices[i * 6] + 3;
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }
        }
    }

    *out = std::move(mesh);
    return true;
}

}  // namespace mobile3d
