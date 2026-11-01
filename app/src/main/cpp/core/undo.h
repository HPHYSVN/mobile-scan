#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "mesh.h"

namespace mobile3d {

// Vertex-range snapshot command for M0 D10 spike.
// Stores interleaved vertex data (6 floats per vertex) for a sparse index
// set, before and after an edit. Undo writes `before`, redo writes `after`.
// Memory per command: indices.size() * (4 + 6*4*2) bytes, reported by bytes().
class VertexSnapshotCommand {
public:
    void captureBefore(const Mesh& mesh, const std::vector<uint32_t>& indices);
    void captureAfter(const Mesh& mesh);
    void undo(Mesh* mesh) const;
    void redo(Mesh* mesh) const;
    size_t bytes() const;

private:
    std::vector<uint32_t> indices_;
    std::vector<float> before_;
    std::vector<float> after_;
};

// Bounded stack: oldest commands are dropped beyond the cap so undo memory
// stays predictable (a product requirement, not just an implementation detail).
class UndoStack {
public:
    explicit UndoStack(size_t maxCommands = 100) : maxCommands_(maxCommands) {}

    void push(VertexSnapshotCommand command);
    bool undo(Mesh* mesh);
    bool redo(Mesh* mesh);
    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ < commands_.size(); }
    size_t size() const { return commands_.size(); }
    size_t totalBytes() const;

private:
    std::deque<VertexSnapshotCommand> commands_;
    size_t cursor_ = 0;  // commands_[0, cursor_) are currently applied
    size_t maxCommands_;
};

}  // namespace mobile3d
