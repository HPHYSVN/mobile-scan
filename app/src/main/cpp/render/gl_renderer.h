#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

#include "../core/deform.h"
#include "../core/mesh.h"
#include "../core/scene.h"

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
    bool removeNode(uint32_t id);
    bool saveProject(std::string* error);
    bool openProject(const std::string& dir, std::string* error);
    // Exports one node's mesh (without node transform) to a .glb file.
    bool exportNode(uint32_t id, const std::string& path, std::string* error);
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

    std::string glInfo_;
    std::chrono::steady_clock::time_point start_{};
    std::chrono::steady_clock::time_point last_{};
    uint64_t frameCount_ = 0;
    double totalFrameMs_ = 0.0;
    double lastFrameMs_ = 0.0;
};

}  // namespace mobile3d
