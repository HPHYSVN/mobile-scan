#include "scene.h"

#include <algorithm>
#include <cmath>

namespace mobile3d {

void Transform::toMatrix(float out[16]) const {
    const float cx = std::cos(rotationEuler[0]), sx = std::sin(rotationEuler[0]);
    const float cy = std::cos(rotationEuler[1]), sy = std::sin(rotationEuler[1]);
    const float cz = std::cos(rotationEuler[2]), sz = std::sin(rotationEuler[2]);

    // R = Rx * Ry * Rz (column-major entries)
    const float r00 = cy * cz, r01 = cy * sz, r02 = -sy;
    const float r10 = cx * sz - sx * sy * cz, r11 = cx * cz + sx * sy * sz, r12 = sx * cy;
    const float r20 = sx * sz + cx * sy * cz, r21 = sx * cz - cx * sy * sz, r22 = cx * cy;

    out[0] = r00 * scale[0]; out[1] = r10 * scale[0]; out[2] = r20 * scale[0]; out[3] = 0.0f;
    out[4] = r01 * scale[1]; out[5] = r11 * scale[1]; out[6] = r21 * scale[1]; out[7] = 0.0f;
    out[8] = r02 * scale[2]; out[9] = r12 * scale[2]; out[10] = r22 * scale[2]; out[11] = 0.0f;
    out[12] = position[0]; out[13] = position[1]; out[14] = position[2]; out[15] = 1.0f;
}

bool Transform::equals(const Transform& other) const {
    for (int i = 0; i < 3; ++i) {
        if (position[i] != other.position[i] || rotationEuler[i] != other.rotationEuler[i] ||
            scale[i] != other.scale[i]) {
            return false;
        }
    }
    return true;
}

uint32_t Scene::addNode(const std::string& name, const std::string& meshRef) {
    Node node;
    node.id = nextId_++;
    node.name = name;
    node.meshRef = meshRef;
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

bool Scene::removeNode(uint32_t id) {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(), [id](const Node& n) { return n.id == id; });
    if (it == nodes_.end()) return false;
    nodes_.erase(it);
    return true;
}

Node* Scene::find(uint32_t id) {
    for (auto& node : nodes_) if (node.id == id) return &node;
    return nullptr;
}

const Node* Scene::find(uint32_t id) const {
    for (const auto& node : nodes_) if (node.id == id) return &node;
    return nullptr;
}

void Scene::clear() {
    nodes_.clear();
    nextId_ = 1;
}

}  // namespace mobile3d
