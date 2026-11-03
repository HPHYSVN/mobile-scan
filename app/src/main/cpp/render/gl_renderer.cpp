#include "gl_renderer.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <algorithm>
#include <cmath>

#include "../core/project.h"
#include "../io/gltf_exporter.h"
#include "../io/gltf_loader.h"
#include "../io/obj_loader.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Mobile3D", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Mobile3D", __VA_ARGS__)

namespace mobile3d {
namespace {

const char* kVertexShader = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat3 uNormalMat;
out vec3 vNormal;
void main() {
    vNormal = uNormalMat * aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
uniform vec3 uColor;
out vec4 fragColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.5, 0.8, 0.6));
    float diff = max(dot(n, lightDir), 0.0);
    fragColor = vec4(uColor * (0.25 + 0.75 * diff), 1.0);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOGE("shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Column-major helpers (no math library dependency yet).
void perspective(float fovY, float aspect, float nearZ, float farZ, float* m) {
    const float f = 1.0f / std::tan(fovY * 0.5f);
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (farZ + nearZ) / (nearZ - farZ);
    m[11] = -1.0f;
    m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
}

void multiply(const float* a, const float* b, float* out) {
    float r[16];
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = s;
        }
    for (int i = 0; i < 16; ++i) out[i] = r[i];
}

}  // namespace

bool GlRenderer::init() {
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    glInfo_ = std::string(version ? version : "?") + " | " + (renderer ? renderer : "?");
    LOGI("GL: %s", glInfo_.c_str());

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) return false;
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        LOGE("program link failed");
        return false;
    }

    // A mesh may already have been loaded before the surface existed.
    if (mesh_.vertexCount() == 0) mesh_ = makeCube();
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, mesh_.vertices.size() * sizeof(float), mesh_.vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh_.indices.size() * sizeof(uint32_t), mesh_.indices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glEnable(GL_DEPTH_TEST);
    start_ = std::chrono::steady_clock::now();
    last_ = start_;
    LOGI("renderer init ok: verts=%u tris=%u", vertexCount(), triangleCount());
    return true;
}

double GlRenderer::loadMesh(const std::string& path) {
    const auto begin = std::chrono::steady_clock::now();
    Mesh loaded;
    std::string error;
    if (!mobile3d::loadGltf(path, &loaded, &error)) {
        LOGE("loadGltf failed for %s: %s", path.c_str(), error.c_str());
        return -1.0;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();

    // Frame the mesh: center on bounding-box center, scale radius to 1.
    float minV[3] = {1e30f, 1e30f, 1e30f}, maxV[3] = {-1e30f, -1e30f, -1e30f};
    for (uint32_t i = 0; i < loaded.vertexCount(); ++i) {
        const float* p = &loaded.vertices[i * 6];
        for (int k = 0; k < 3; ++k) {
            minV[k] = std::min(minV[k], p[k]);
            maxV[k] = std::max(maxV[k], p[k]);
        }
    }
    float radius = 0.0f;
    for (int k = 0; k < 3; ++k) {
        center_[k] = (minV[k] + maxV[k]) * 0.5f;
        radius = std::max(radius, (maxV[k] - minV[k]) * 0.5f);
    }
    scale_ = radius > 0.0f ? 1.0f / radius : 1.0f;

    mesh_ = std::move(loaded);

    // Prepare deform spike: adjacency build (timed) + region of ~5k vertices.
    {
        const auto adjBegin = std::chrono::steady_clock::now();
        deform_.buildAdjacency(mesh_);
        adjacencyMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - adjBegin).count();
        deform_.snapshotBase(mesh_);
        float threshold = 0.99f;
        while (threshold > 0.0f && deform_.selectByNormal(mesh_, 0.3f, 0.7f, 0.65f, threshold) < 4000) threshold -= 0.02f;
        deformFrames_ = 0;
        deformTotalMs_ = normalTotalMs_ = uploadTotalMs_ = 0.0;
        LOGI("deform prep: adjacency %.1f ms, region %u verts (threshold %.2f)",
             adjacencyMs_, deformVertexCount(), threshold);
    }

    if (vao_) {  // re-upload on GL thread (loadMesh is called from GL thread)
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, mesh_.vertices.size() * sizeof(float), mesh_.vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh_.indices.size() * sizeof(uint32_t), mesh_.indices.data(), GL_DYNAMIC_DRAW);
    }
    frameCount_ = 0;
    totalFrameMs_ = 0.0;
    LOGI("loaded %s in %.1f ms: verts=%u tris=%u", path.c_str(), ms, vertexCount(), triangleCount());
    return ms;
}

void GlRenderer::resize(int width, int height) {
    width_ = width;
    height_ = height;
    glViewport(0, 0, width, height);
}

void GlRenderer::drawFrame() {
    const auto now = std::chrono::steady_clock::now();
    lastFrameMs_ = std::chrono::duration<double, std::milli>(now - last_).count();
    last_ = now;
    totalFrameMs_ += lastFrameMs_;
    ++frameCount_;

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!program_ || height_ == 0) return;

    if (deformEnabled_ && deformVertexCount() > 0) {
        // Spike only: work happens on the GL thread to isolate stage costs.
        // Production threading (worker -> dirty ranges -> GL) is an M2 decision.
        const float phase = std::sin(std::chrono::duration<float>(now - start_).count() * 2.0f) * 0.06f;
        auto stage = std::chrono::steady_clock::now();
        deform_.apply(&mesh_, phase);
        const auto afterDeform = std::chrono::steady_clock::now();
        deform_.recomputeNormals(&mesh_);
        const auto afterNormals = std::chrono::steady_clock::now();

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        const auto& selected = deform_.selected();
        size_t i = 0;
        while (i < selected.size()) {
            size_t j = i;
            while (j + 1 < selected.size() && selected[j + 1] == selected[j] + 1) ++j;
            const uint32_t first = selected[i];
            const size_t count = j - i + 1;
            glBufferSubData(GL_ARRAY_BUFFER,
                            static_cast<GLintptr>(first * 6 * sizeof(float)),
                            static_cast<GLsizeiptr>(count * 6 * sizeof(float)),
                            &mesh_.vertices[first * 6]);
            i = j + 1;
        }
        const auto afterUpload = std::chrono::steady_clock::now();
        deformTotalMs_ += std::chrono::duration<double, std::milli>(afterDeform - stage).count();
        normalTotalMs_ += std::chrono::duration<double, std::milli>(afterNormals - afterDeform).count();
        uploadTotalMs_ += std::chrono::duration<double, std::milli>(afterUpload - afterNormals).count();
        ++deformFrames_;
    }

    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);

    float proj[16];
    perspective(0.9f, aspect, 0.1f, 100.0f, proj);

    const float cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float cx = std::cos(pitch_), sx = std::sin(pitch_);
    // model = T(0,0,-dist) * RotY * RotX * Scale * T(-center), column-major.
    // The Scale*T(-center) part folds into the rotation rows below.
    float rot[16] = {
        cy, sy * sx, sy * cx, 0.0f,
        0.0f, cx, -sx, 0.0f,
        -sy, cy * sx, cy * cx, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    float model[16];
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 4; ++r) model[c * 4 + r] = rot[c * 4 + r] * scale_;
    // translation = rot * (-center) * scale
    for (int r = 0; r < 3; ++r) {
        float s = 0.0f;
        for (int k = 0; k < 3; ++k) s += rot[k * 4 + r] * (-center_[k]);
        model[12 + r] = s * scale_;
    }
    model[15] = 1.0f;
    model[14] -= distance_;
    float mvp[16];
    multiply(proj, model, mvp);

    const float normalMat[9] = {model[0], model[1], model[2],
                                model[4], model[5], model[6],
                                model[8], model[9], model[10]};

    glUseProgram(program_);
    if (!scene_.nodes().empty()) {
        // Scene path: view = T(0,0,-dist) * RotY(yaw) * RotX(pitch)
        float view[16] = {
            cy, sy * sx, sy * cx, 0.0f,
            0.0f, cx, -sx, 0.0f,
            -sy, cy * sx, cy * cx, 0.0f,
            0.0f, 0.0f, -distance_, 1.0f,
        };
        float viewProj[16];
        multiply(proj, view, viewProj);
        for (const auto& node : scene_.nodes()) {
            if (!node.visible) continue;
            GpuMesh* gpu = ensureGpuMesh(node.meshRef);
            if (gpu) drawMesh(*gpu, viewProj, &node);
        }
        return;
    }

    glUniformMatrix4fv(glGetUniformLocation(program_, "uMvp"), 1, GL_FALSE, mvp);
    glUniformMatrix3fv(glGetUniformLocation(program_, "uNormalMat"), 1, GL_FALSE, normalMat);
    glUniform3f(glGetUniformLocation(program_, "uColor"), 0.25f, 0.55f, 0.9f);
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh_.indices.size()), GL_UNSIGNED_INT, nullptr);
}

bool GlRenderer::cpuMeshForRef(const std::string& ref, Mesh* out, std::string* error) {
    if (ref.rfind("primitive:", 0) == 0) {
        const std::string kind = ref.substr(10);
        if (kind == "cube") *out = makeCube();
        else if (kind == "sphere") *out = makeSphere();
        else if (kind == "cylinder") *out = makeCylinder();
        else if (kind == "plane") *out = makePlane();
        else {
            if (error) *error = "unknown primitive: " + ref;
            return false;
        }
        return true;
    }
    const std::string path = projectDir_ + "/" + ref;
    const bool isObj = path.size() > 4 && path.compare(path.size() - 4, 4, ".obj") == 0;
    return isObj ? loadObj(path, out, error) : loadGltf(path, out, error);
}

bool GlRenderer::exportNode(uint32_t id, const std::string& path, std::string* error) {
    const Node* node = scene_.find(id);
    if (!node) {
        if (error) *error = "node not found";
        return false;
    }
    Mesh cpu;
    if (!cpuMeshForRef(node->meshRef, &cpu, error)) return false;
    return exportGlb(cpu, path, error);
}

GlRenderer::GpuMesh* GlRenderer::ensureGpuMesh(const std::string& ref) {
    const auto it = gpuMeshes_.find(ref);
    if (it != gpuMeshes_.end()) return &it->second;

    Mesh cpu;
    std::string error;
    if (!cpuMeshForRef(ref, &cpu, &error)) {
        LOGE("scene mesh load failed %s: %s", ref.c_str(), error.c_str());
        gpuMeshes_[ref] = GpuMesh{};
        return &gpuMeshes_[ref];
    }

    GpuMesh gpu;
    gpu.indexCount = static_cast<uint32_t>(cpu.indices.size());
    float minV[3] = {1e30f, 1e30f, 1e30f}, maxV[3] = {-1e30f, -1e30f, -1e30f};
    for (uint32_t i = 0; i < cpu.vertexCount(); ++i) {
        const float* p = &cpu.vertices[i * 6];
        for (int k = 0; k < 3; ++k) {
            minV[k] = std::min(minV[k], p[k]);
            maxV[k] = std::max(maxV[k], p[k]);
        }
    }
    float radius = 0.0f;
    for (int k = 0; k < 3; ++k) {
        gpu.center[k] = (minV[k] + maxV[k]) * 0.5f;
        radius = std::max(radius, (maxV[k] - minV[k]) * 0.5f);
    }
    gpu.scale = radius > 0.0f ? 1.0f / radius : 1.0f;

    glGenVertexArrays(1, &gpu.vao);
    glGenBuffers(1, &gpu.vbo);
    glGenBuffers(1, &gpu.ebo);
    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, cpu.vertices.size() * sizeof(float), cpu.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, cpu.indices.size() * sizeof(uint32_t), cpu.indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    auto [inserted, okInsert] = gpuMeshes_.emplace(ref, gpu);
    (void)okInsert;
    LOGI("scene mesh ready %s: tris=%u", ref.c_str(), gpu.indexCount / 3);
    return &inserted->second;
}

void GlRenderer::drawMesh(const GpuMesh& gpu, const float* viewProj, const Node* node) {
    if (gpu.indexCount == 0) return;
    float nodeMat[16];
    node->transform.toMatrix(nodeMat);
    // fit = Scale(gpu.scale) * T(-center)
    float fit[16] = {};
    fit[0] = gpu.scale; fit[5] = gpu.scale; fit[10] = gpu.scale; fit[15] = 1.0f;
    fit[12] = -gpu.center[0] * gpu.scale;
    fit[13] = -gpu.center[1] * gpu.scale;
    fit[14] = -gpu.center[2] * gpu.scale;
    float world[16], tmp[16];
    multiply(nodeMat, fit, world);
    multiply(viewProj, world, tmp);

    const float normalMat[9] = {world[0], world[1], world[2],
                                world[4], world[5], world[6],
                                world[8], world[9], world[10]};
    glUniformMatrix4fv(glGetUniformLocation(program_, "uMvp"), 1, GL_FALSE, tmp);
    glUniformMatrix3fv(glGetUniformLocation(program_, "uNormalMat"), 1, GL_FALSE, normalMat);
    glUniform3f(glGetUniformLocation(program_, "uColor"), node->color[0], node->color[1], node->color[2]);
    glBindVertexArray(gpu.vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(gpu.indexCount), GL_UNSIGNED_INT, nullptr);
}

void GlRenderer::newProject(const std::string& dir) {
    projectDir_ = dir;
    scene_.clear();
    for (auto& [ref, gpu] : gpuMeshes_) {
        (void)ref;
        if (gpu.ebo) glDeleteBuffers(1, &gpu.ebo);
        if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
        if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
    }
    gpuMeshes_.clear();
    deformEnabled_ = false;
    LOGI("new project: %s", dir.c_str());
}

uint32_t GlRenderer::addPrimitive(const std::string& kind) {
    return scene_.addNode(kind, "primitive:" + kind);
}

uint32_t GlRenderer::addNode(const std::string& name, const std::string& meshRef) {
    return scene_.addNode(name, meshRef);
}

bool GlRenderer::setNodeTransform(uint32_t id, const float values[9]) {
    Node* node = scene_.find(id);
    if (!node) return false;
    for (int i = 0; i < 3; ++i) {
        node->transform.position[i] = values[i];
        node->transform.rotationEuler[i] = values[3 + i];
        node->transform.scale[i] = values[6 + i];
    }
    return true;
}

bool GlRenderer::removeNode(uint32_t id) { return scene_.removeNode(id); }

bool GlRenderer::setNodeColor(uint32_t id, float r, float g, float b) {
    Node* node = scene_.find(id);
    if (!node) return false;
    node->color[0] = r; node->color[1] = g; node->color[2] = b;
    return true;
}

bool GlRenderer::saveProject(std::string* error) {
    if (projectDir_.empty()) {
        if (error) *error = "no project dir set";
        return false;
    }
    const bool ok = mobile3d::saveProject(projectDir_, scene_, error);
    LOGI("saveProject %s -> %s", projectDir_.c_str(), ok ? "ok" : (error ? error->c_str() : "?"));
    return ok;
}

bool GlRenderer::openProject(const std::string& dir, std::string* error) {
    Scene loaded;
    if (!mobile3d::loadProject(dir, &loaded, error)) return false;
    newProject(dir);
    scene_.restore(loaded.nodes(), loaded.nextId());
    LOGI("openProject %s: %zu nodes", dir.c_str(), scene_.nodes().size());
    return true;
}

void GlRenderer::shutdown() {
    for (auto& [ref, gpu] : gpuMeshes_) {
        (void)ref;
        if (gpu.ebo) glDeleteBuffers(1, &gpu.ebo);
        if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
        if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
    }
    gpuMeshes_.clear();
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    ebo_ = vbo_ = vao_ = program_ = 0;
}

}  // namespace mobile3d
