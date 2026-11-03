#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

#include <array>
#include <map>
#include <set>
#include <unordered_map>

#include "../core/deform.h"
#include "../core/mesh.h"
#include "../core/scene.h"
#include "../core/undo.h"

namespace mobile3d {

// GLES 3.0 renderer. Owns all GL objects; must only be called from the GL
// thread. No abstraction over GL yet (see TD-03/TD-11).
// Draws the Scene when it has nodes; otherwise draws the legacy single mesh
// loaded via loadMesh() (M0 fixture/deform spike).
class GlRenderer {
public:
    bool init();  // compile shaders, upload mesh; false on failure (see log)
    // Loads a glTF mesh as the legacy single mesh. Returns load ms or -1.
    double loadMesh(const std::string& path);
    void setOrbit(float yaw, float pitch, float distance) {
        yaw_ = yaw; pitch_ = pitch; distance_ = distance;
    }
    void setPan(float panX, float panY) { panX_ = panX; panY_ = panY; }
    float distance() const { return distance_; }
    void setDeformEnabled(bool enabled) { deformEnabled_ = enabled; }
    bool deformEnabled() const { return deformEnabled_; }
    double avgDeformMs() const { return deformFrames_ ? deformTotalMs_ / deformFrames_ : 0.0; }
    double avgNormalMs() const { return deformFrames_ ? normalTotalMs_ / deformFrames_ : 0.0; }
    double avgUploadMs() const { return deformFrames_ ? uploadTotalMs_ / deformFrames_ : 0.0; }
    uint32_t deformVertexCount() const { return static_cast<uint32_t>(deform_.selected().size()); }
    double adjacencyMs() const { return adjacencyMs_; }
    void resize(int width, int height);
    void drawFrame();
    void shutdown();

    // --- Scene / project (M1) ---
    void newProject(const std::string& dir);
    uint32_t addPrimitive(const std::string& kind);  // cube/sphere/cylinder/plane
    uint32_t addNode(const std::string& name, const std::string& meshRef);
    bool setNodeTransform(uint32_t id, const float values[9]);  // pos, euler, scale
    bool setNodeColor(uint32_t id, float r, float g, float b);
    uint32_t duplicateNode(uint32_t id);

    // --- Sculpt (M2 slice) ---
    // Brush modes: 0=Draw, 1=Dig, 2=Smooth, 3=Inflate.
    void setBrush(float radiusWorld, float strength, int mode) {
        brushRadius_ = radiusWorld; brushStrength_ = strength; brushMode_ = mode;
    }
    // phase: 0=start stroke + dab, 1=dab, 2=end stroke. Returns affected verts.
    int brushStroke(uint32_t nodeId, float ndcX, float ndcY, int phase);
    int pickNode(float ndcX, float ndcY);  // node id or -1
    // Screen-space bounding box (NDC) of a node for edit handles.
    bool nodeScreenBox(uint32_t id, float out[4]);
    bool undoSculpt();
    bool redoSculpt();
    bool removeNode(uint32_t id);
    bool saveProject(std::string* error);
    bool openProject(const std::string& dir, std::string* error);
    // Exports one node's mesh (without node transform) to a .glb file.
    // format: 0=GLB triangles, 1=OBJ triangles, 2=OBJ quads.
    // resolutionPercent scales triangle count via simplification (100 = keep),
    // subdivideLevels (0..2) runs before simplification.
    // Exports all visible nodes into one GLB with node transforms baked in.
    bool exportScene(const std::string& path, std::string* error);
    bool exportNodeEx(uint32_t id, const std::string& path, int format,
                      int resolutionPercent, int subdivideLevels, std::string* error);
    const Scene& scene() const { return scene_; }
    const std::string& projectDir() const { return projectDir_; }

    const std::string& glInfo() const { return glInfo_; }
    uint64_t frameCount() const { return frameCount_; }
    double avgFrameMs() const { return frameCount_ ? totalFrameMs_ / frameCount_ : 0.0; }
    double lastFrameMs() const { return lastFrameMs_; }
    uint32_t vertexCount() const { return mesh_.vertexCount(); }
    uint32_t triangleCount() const { return mesh_.triangleCount(); }

private:
    struct GpuMesh {
        unsigned int vao = 0, vbo = 0, ebo = 0;
        uint32_t indexCount = 0;
        float center[3] = {0.0f, 0.0f, 0.0f};
        float scale = 1.0f;
        float radius = 0.5f;  // local bounding-sphere radius
    };
    // Creates (once per ref) GPU buffers for a scene meshRef; nullptr on failure.
    GpuMesh* ensureGpuMesh(const std::string& ref);
    bool cpuMeshForRef(const std::string& ref, Mesh* out, std::string* error);
    void drawMesh(const GpuMesh& gpu, const float* viewProj, const Node* node);

    Mesh mesh_;  // legacy single mesh
    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    int width_ = 0;
    int height_ = 0;
    float yaw_ = 0.6f;
    float pitch_ = 0.4f;
    float distance_ = 4.2f;
    float panX_ = 0.0f;
    float panY_ = 0.0f;
    float center_[3] = {0.0f, 0.0f, 0.0f};
    float scale_ = 1.0f;
    DeformRegion deform_;
    bool deformEnabled_ = false;
    uint64_t deformFrames_ = 0;
    double deformTotalMs_ = 0.0;
    double normalTotalMs_ = 0.0;
    double uploadTotalMs_ = 0.0;
    double adjacencyMs_ = 0.0;

    Scene scene_;
    std::string projectDir_;
    std::map<std::string, GpuMesh> gpuMeshes_;
    std::map<std::string, Mesh> cpuMeshes_;
    std::set<std::string> sculptDirty_;  // refs whose CPU mesh differs from its source
    std::map<std::string, DeformRegion> adjacency_;

    float brushRadius_ = 0.35f;
    float brushStrength_ = 0.02f;
    int brushMode_ = 0;
    bool strokeActive_ = false;
    uint32_t strokeNode_ = 0;
    std::map<uint32_t, std::array<float, 6>> strokeBefore_;
    struct SculptEntry { std::string ref; VertexSnapshotCommand command; };
    std::vector<SculptEntry> sculptUndo_;
    std::vector<SculptEntry> sculptRedo_;

    void buildViewProj(float out[16]);
    void nodeWorld(const Node& node, const GpuMesh& gpu, float out[16]);

    std::string glInfo_;
    std::chrono::steady_clock::time_point start_{};
    std::chrono::steady_clock::time_point last_{};
    uint64_t frameCount_ = 0;
    double totalFrameMs_ = 0.0;
    double lastFrameMs_ = 0.0;
};

}  // namespace mobile3d
