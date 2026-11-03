#include "undo.h"

#include <cstring>

namespace mobile3d {
namespace {

void writeVertices(Mesh* mesh, const std::vector<uint32_t>& indices, const std::vector<float>& data) {
    if (data.size() != indices.size() * 6) return;
    for (size_t i = 0; i < indices.size(); ++i) {
        float* dst = &mesh->vertices[indices[i] * 6];
        std::memcpy(dst, &data[i * 6], 6 * sizeof(float));
    }
}

std::vector<float> readVertices(const Mesh& mesh, const std::vector<uint32_t>& indices) {
    std::vector<float> data(indices.size() * 6);
    for (size_t i = 0; i < indices.size(); ++i) {
        std::memcpy(&data[i * 6], &mesh.vertices[indices[i] * 6], 6 * sizeof(float));
    }
    return data;
}

}  // namespace

void VertexSnapshotCommand::captureBefore(const Mesh& mesh, const std::vector<uint32_t>& indices) {
    indices_ = indices;
    before_ = readVertices(mesh, indices_);
    after_.clear();
}

void VertexSnapshotCommand::setData(std::vector<uint32_t> indices, std::vector<float> before, std::vector<float> after) {
    indices_ = std::move(indices);
    before_ = std::move(before);
    after_ = std::move(after);
}

void VertexSnapshotCommand::captureAfter(const Mesh& mesh) {
    after_ = readVertices(mesh, indices_);
}

void VertexSnapshotCommand::undo(Mesh* mesh) const { writeVertices(mesh, indices_, before_); }
void VertexSnapshotCommand::redo(Mesh* mesh) const { writeVertices(mesh, indices_, after_); }

size_t VertexSnapshotCommand::bytes() const {
    return indices_.size() * sizeof(uint32_t) + (before_.size() + after_.size()) * sizeof(float);
}

void UndoStack::push(VertexSnapshotCommand command) {
    while (commands_.size() > cursor_) commands_.pop_back();  // drop redo tail
    commands_.push_back(std::move(command));
    ++cursor_;
    while (commands_.size() > maxCommands_) {
        commands_.pop_front();
        if (cursor_ > 0) --cursor_;
    }
}

bool UndoStack::undo(Mesh* mesh) {
    if (!canUndo()) return false;
    --cursor_;
    commands_[cursor_].undo(mesh);
    return true;
}

bool UndoStack::redo(Mesh* mesh) {
    if (!canRedo()) return false;
    commands_[cursor_].redo(mesh);
    ++cursor_;
    return true;
}

size_t UndoStack::totalBytes() const {
    size_t total = 0;
    for (const auto& command : commands_) total += command.bytes();
    return total;
}

}  // namespace mobile3d
