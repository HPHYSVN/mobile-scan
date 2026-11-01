#include <cassert>
#include <cmath>
#include <cstdio>

#include "core/deform.h"
#include "core/mesh.h"
#include "core/undo.h"

int main() {
    mobile3d::Mesh cube = mobile3d::makeCube();
    const mobile3d::Mesh original = cube;

    mobile3d::DeformRegion deform;
    deform.buildAdjacency(cube);
    deform.selectByNormal(cube, 0, 0, 1, 0.9f);
    deform.snapshotBase(cube);

    mobile3d::UndoStack stack(100);
    assert(!stack.canUndo());

    mobile3d::VertexSnapshotCommand command;
    command.captureBefore(cube, deform.selected());
    deform.apply(&cube, 0.25f);
    deform.recomputeNormals(&cube);
    command.captureAfter(cube);
    const size_t commandBytes = command.bytes();
    assert(commandBytes == 4 * (4 + 48));  // 4 verts: index + 2x6 floats each
    stack.push(std::move(command));
    assert(stack.totalBytes() == commandBytes);

    // Mesh differs after edit, undo restores bit-exact, redo reapplies.
    bool differs = false;
    for (size_t i = 0; i < cube.vertices.size(); ++i) differs |= cube.vertices[i] != original.vertices[i];
    assert(differs);

    assert(stack.undo(&cube));
    for (size_t i = 0; i < cube.vertices.size(); ++i) assert(cube.vertices[i] == original.vertices[i]);
    assert(!stack.canUndo());
    assert(stack.canRedo());

    assert(stack.redo(&cube));
    differs = false;
    for (size_t i = 0; i < cube.vertices.size(); ++i) differs |= cube.vertices[i] != original.vertices[i];
    assert(differs);

    // Cap: pushing beyond max drops oldest and keeps total bounded.
    mobile3d::UndoStack capped(3);
    mobile3d::Mesh m2 = mobile3d::makeCube();
    for (int i = 0; i < 5; ++i) {
        mobile3d::VertexSnapshotCommand c;
        c.captureBefore(m2, deform.selected());
        c.captureAfter(m2);
        capped.push(std::move(c));
    }
    assert(capped.size() == 3);

    std::printf("test_undo: PASS (command=%zu bytes)\n", commandBytes);
    return 0;
}
