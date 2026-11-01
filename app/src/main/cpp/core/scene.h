#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mobile3d {

// Local transform: position, Euler rotation (radians, applied Z*Y*X in
// toMatrix order below via column-major composition), scale.
struct Transform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotationEuler[3] = {0.0f, 0.0f, 0.0f};
    float scale[3] = {1.0f, 1.0f, 1.0f};

    // Writes a column-major 4x4: T * Rx * Ry * Rz * S.
    void toMatrix(float out[16]) const;
    bool equals(const Transform& other) const;
};

struct Node {
    uint32_t id = 0;
    std::string name;
    // Reference into the project's meshes/ directory (file name), or a
    // primitive kind prefixed with "primitive:" (e.g. "primitive:cube").
    std::string meshRef;
    Transform transform;
    bool visible = true;
};

// Flat scene graph for M1 (parenting is a later, deliberate decision).
class Scene {
public:
    uint32_t addNode(const std::string& name, const std::string& meshRef);
    bool removeNode(uint32_t id);
    Node* find(uint32_t id);
    const Node* find(uint32_t id) const;
    void clear();

    const std::vector<Node>& nodes() const { return nodes_; }
    uint32_t nextId() const { return nextId_; }
    void setNextId(uint32_t next) { nextId_ = next; }
    void restore(std::vector<Node> nodes, uint32_t nextId) {
        nodes_ = std::move(nodes);
        nextId_ = nextId;
    }

private:
    std::vector<Node> nodes_;
    uint32_t nextId_ = 1;
};

}  // namespace mobile3d
