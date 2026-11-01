// Implementation lives in gltf_loader.cpp (TINYGLTF_IMPLEMENTATION there).
// Keep macros consistent across translation units.
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "../third_party/tinygltf/tiny_gltf.h"

#include "gltf_exporter.h"

#include <algorithm>
#include <cstring>

namespace mobile3d {

bool exportGlb(const Mesh& mesh, const std::string& path, std::string* error) {
    if (mesh.vertexCount() == 0 || mesh.indices.empty()) {
        if (error) *error = "empty mesh";
        return false;
    }

    // Deinterleave into separate position/normal arrays.
    std::vector<float> positions(mesh.vertexCount() * 3);
    std::vector<float> normals(mesh.vertexCount() * 3);
    float minV[3] = {1e30f, 1e30f, 1e30f}, maxV[3] = {-1e30f, -1e30f, -1e30f};
    for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
        const float* v = &mesh.vertices[i * 6];
        for (int k = 0; k < 3; ++k) {
            positions[i * 3 + k] = v[k];
            normals[i * 3 + k] = v[k + 3];
            minV[k] = std::min(minV[k], v[k]);
            maxV[k] = std::max(maxV[k], v[k]);
        }
    }

    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "mobile3d validation exporter";

    tinygltf::Buffer buffer;
    const size_t posBytes = positions.size() * sizeof(float);
    const size_t nrmBytes = normals.size() * sizeof(float);
    const size_t idxBytes = mesh.indices.size() * sizeof(uint32_t);
    buffer.data.resize(posBytes + nrmBytes + idxBytes);
    std::memcpy(buffer.data.data(), positions.data(), posBytes);
    std::memcpy(buffer.data.data() + posBytes, normals.data(), nrmBytes);
    std::memcpy(buffer.data.data() + posBytes + nrmBytes, mesh.indices.data(), idxBytes);
    model.buffers.push_back(buffer);

    auto addView = [&model](size_t offset, size_t length, int target) {
        tinygltf::BufferView view;
        view.buffer = 0;
        view.byteOffset = offset;
        view.byteLength = length;
        view.target = target;
        model.bufferViews.push_back(view);
        return static_cast<int>(model.bufferViews.size()) - 1;
    };
    const int posView = addView(0, posBytes, TINYGLTF_TARGET_ARRAY_BUFFER);
    const int nrmView = addView(posBytes, nrmBytes, TINYGLTF_TARGET_ARRAY_BUFFER);
    const int idxView = addView(posBytes + nrmBytes, idxBytes, TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

    tinygltf::Accessor posAcc;
    posAcc.bufferView = posView;
    posAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    posAcc.count = mesh.vertexCount();
    posAcc.type = TINYGLTF_TYPE_VEC3;
    posAcc.minValues = {minV[0], minV[1], minV[2]};
    posAcc.maxValues = {maxV[0], maxV[1], maxV[2]};
    model.accessors.push_back(posAcc);

    tinygltf::Accessor nrmAcc;
    nrmAcc.bufferView = nrmView;
    nrmAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    nrmAcc.count = mesh.vertexCount();
    nrmAcc.type = TINYGLTF_TYPE_VEC3;
    model.accessors.push_back(nrmAcc);

    tinygltf::Accessor idxAcc;
    idxAcc.bufferView = idxView;
    idxAcc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    idxAcc.count = mesh.indices.size();
    idxAcc.type = TINYGLTF_TYPE_SCALAR;
    model.accessors.push_back(idxAcc);

    tinygltf::Primitive prim;
    prim.attributes["POSITION"] = 0;
    prim.attributes["NORMAL"] = 1;
    prim.indices = 2;
    prim.mode = TINYGLTF_MODE_TRIANGLES;

    tinygltf::Mesh glMesh;
    glMesh.primitives.push_back(prim);
    model.meshes.push_back(glMesh);

    tinygltf::Node node;
    node.mesh = 0;
    model.nodes.push_back(node);

    tinygltf::Scene scene;
    scene.nodes.push_back(0);
    model.scenes.push_back(scene);
    model.defaultScene = 0;

    tinygltf::TinyGLTF writer;
    if (!writer.WriteGltfSceneToFile(&model, path, false, false, true, true)) {
        if (error) *error = "tinygltf failed to write " + path;
        return false;
    }
    return true;
}

}  // namespace mobile3d
