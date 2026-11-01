#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "../core/deform.h"
#include "../core/mesh.h"

namespace mobile3d {

// GLES 3.0 renderer for validation. Owns all GL objects; must only be called
// from the GL thread. No abstraction over GL yet (see TD-03/TD-11).
class GlRenderer {
public:
    bool init();  // compile shaders, upload mesh; false on failure (see log)
    // Loads a glTF mesh and uploads it. Returns load time in ms, or -1 on failure.
    double loadMesh(const std::string& path);
    void setOrbit(float yaw, float pitch, float distance) {
        yaw_ = yaw; pitch_ = pitch; distance_ = distance;
    }
    void setDeformEnabled(bool enabled) { deformEnabled_ = enabled; }
    bool deformEnabled() const { return deformEnabled_; }
    double avgDeformMs() const { return deformFrames_ ? deformTotalMs_ / deformFrames_ : 0.0; }
    double avgNormalMs() const { return deformFrames_ ? normalTotalMs_ / deformFrames_ : 0.0; }
    double avgUploadMs() const { return deformFrames_ ? uploadTotalMs_ / deformFrames_ : 0.0; }
    uint32_t deformVertexCount() const { return deform_.selected().empty() ? 0 : static_cast<uint32_t>(deform_.selected().size()); }
    double adjacencyMs() const { return adjacencyMs_; }
    void resize(int width, int height);
    void drawFrame();
    void shutdown();

    const std::string& glInfo() const { return glInfo_; }
    uint64_t frameCount() const { return frameCount_; }
    double avgFrameMs() const { return frameCount_ ? totalFrameMs_ / frameCount_ : 0.0; }
    double lastFrameMs() const { return lastFrameMs_; }
    uint32_t vertexCount() const { return mesh_.vertexCount(); }
    uint32_t triangleCount() const { return mesh_.triangleCount(); }

private:
    Mesh mesh_;
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
    std::string glInfo_;
    std::chrono::steady_clock::time_point start_{};
    std::chrono::steady_clock::time_point last_{};
    uint64_t frameCount_ = 0;
    double totalFrameMs_ = 0.0;
    double lastFrameMs_ = 0.0;
};

}  // namespace mobile3d
