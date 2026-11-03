#include "gl_renderer.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <algorithm>
#include <cmath>

#include <filesystem>
#include <unordered_set>

#include "../core/project.h"
#include "../core/raycast.h"
#include "../io/gltf_exporter.h"
#include "../io/mesh_ops.h"
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
    model[12] += panX_;
    model[13] += panY_;
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
            panX_ * cy - panY_ * sy * sx, panY_ * cx, -panX_ * sy - panY_ * cy * sx - distance_, 1.0f,
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
    // Instance suffix "#<nodeId>" gives every node its own CPU/GPU mesh so
    // sculpting one node never deforms another that shares the source mesh.
    const std::string base = ref.substr(0, ref.find('#'));
    if (base.rfind("primitive:", 0) == 0) {
        const std::string kind = base.substr(10);
        if (kind == "cube") *out = makeCube();
        else if (kind == "sphere") *out = makeSphere();
        else if (kind == "cylinder") *out = makeCylinder();
        else if (kind == "plane") *out = makePlane();
        else {
            if (error) *error = "unknown primitive: " + base;
            return false;
        }
        return true;
    }
    const std::string path = projectDir_ + "/" + base;
    const bool isObj = path.size() > 4 && path.compare(path.size() - 4, 4, ".obj") == 0;
    return isObj ? loadObj(path, out, error) : loadGltf(path, out, error);
}

bool GlRenderer::exportNodeEx(uint32_t id, const std::string& path, int format,
                              int resolutionPercent, int subdivideLevels, std::string* error) {
    const Node* node = scene_.find(id);
    if (!node) {
        if (error) *error = "node not found";
        return false;
    }
    Mesh cpu;
    if (!cpuMeshForRef(node->meshRef, &cpu, error)) return false;
    Mesh processed = subdivideMesh(cpu, subdivideLevels);
    if (resolutionPercent < 100) {
        Mesh simplified;
        if (!simplifyMesh(processed, resolutionPercent / 100.0f, &simplified, error)) return false;
        processed = std::move(simplified);
    }
    LOGI("export node %u: format=%d tris=%u (from %u)", id, format, processed.triangleCount(), cpu.triangleCount());
    if (format == 0) return exportGlb(processed, path, error);
    return exportObj(processed, path, format == 2, error);
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

    cpuMeshes_[ref] = cpu;  // retained for picking/sculpting
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
    for (int k = 0; k < 3; ++k) gpu.center[k] = (minV[k] + maxV[k]) * 0.5f;
    float radius = 0.0f;
    for (uint32_t i = 0; i < cpu.vertexCount(); ++i) {
        const float* p = &cpu.vertices[i * 6];
        const float dx = p[0] - gpu.center[0], dy = p[1] - gpu.center[1], dz = p[2] - gpu.center[2];
        radius = std::max(radius, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    gpu.scale = radius > 0.0f ? 1.0f / radius : 1.0f;
    gpu.radius = radius;

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
    const uint32_t id = scene_.addNode(kind, "primitive:" + kind);
    if (Node* node = scene_.find(id)) node->meshRef += "#" + std::to_string(id);
    return id;
}

uint32_t GlRenderer::addNode(const std::string& name, const std::string& meshRef) {
    const uint32_t id = scene_.addNode(name, meshRef);
    if (Node* node = scene_.find(id)) node->meshRef += "#" + std::to_string(id);
    return id;
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

uint32_t GlRenderer::duplicateNode(uint32_t id) {
    const Node* node = scene_.find(id);
    if (!node) return 0;
    Node copy = *node;
    const uint32_t newId = scene_.addNode(node->name + " copy", node->meshRef);
    Node* created = scene_.find(newId);
    if (created) {
        created->transform = copy.transform;
        created->transform.position[0] += 0.35f;
        created->visible = copy.visible;
        created->color[0] = copy.color[0]; created->color[1] = copy.color[1]; created->color[2] = copy.color[2];
    }
    return newId;
}

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
    {
        std::error_code ec;
        std::filesystem::create_directories(projectDir_ + "/meshes", ec);
        if (ec) {
            if (error) *error = "cannot create meshes dir: " + ec.message();
            return false;
        }
    }
    // Persist sculpted geometry: write the node's current CPU mesh to its
    // own file and repoint the node at it (scene.json only stores refs).
    std::vector<uint32_t> nodeIds;
    for (const auto& node : scene_.nodes()) nodeIds.push_back(node.id);
    for (uint32_t nodeId : nodeIds) {
        Node* node = scene_.find(nodeId);
        if (!node || sculptDirty_.find(node->meshRef) == sculptDirty_.end()) continue;
        const auto it = cpuMeshes_.find(node->meshRef);
        if (it == cpuMeshes_.end()) continue;
        const std::string fileName = "node_" + std::to_string(node->id) + ".glb";
        std::string exportError;
        if (!exportGlb(it->second, projectDir_ + "/meshes/" + fileName, &exportError)) {
            if (error) *error = "sculpt persist failed: " + exportError;
            return false;
        }
        LOGI("persisted sculpted mesh for node %u -> meshes/%s", node->id, fileName.c_str());
        node->meshRef = "meshes/" + fileName;
    }
    sculptDirty_.clear();
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

namespace {

bool invert4(const float m[16], float out[16]) {
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0f) return false;
    const float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i) out[i] = inv[i] * invDet;
    return true;
}

void transformPoint4(const float m[16], float x, float y, float z, float out[3]) {
    const float w = m[3] * x + m[7] * y + m[11] * z + m[15];
    out[0] = (m[0] * x + m[4] * y + m[8] * z + m[12]) / w;
    out[1] = (m[1] * x + m[5] * y + m[9] * z + m[13]) / w;
    out[2] = (m[2] * x + m[6] * y + m[10] * z + m[14]) / w;
}

}  // namespace

void GlRenderer::buildViewProj(float out[16]) {
    const float aspect = height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    float proj[16];
    perspective(0.9f, aspect, 0.1f, 100.0f, proj);
    const float cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float cx = std::cos(pitch_), sx = std::sin(pitch_);
    float view[16] = {
        cy, sy * sx, sy * cx, 0.0f,
        0.0f, cx, -sx, 0.0f,
        -sy, cy * sx, cy * cx, 0.0f,
        panX_ * cy - panY_ * sy * sx, panY_ * cx, -panX_ * sy - panY_ * cy * sx - distance_, 1.0f,
    };
    multiply(proj, view, out);
}

void GlRenderer::nodeWorld(const Node& node, const GpuMesh& gpu, float out[16]) {
    float nodeMat[16];
    node.transform.toMatrix(nodeMat);
    float fit[16] = {};
    fit[0] = gpu.scale; fit[5] = gpu.scale; fit[10] = gpu.scale; fit[15] = 1.0f;
    fit[12] = -gpu.center[0] * gpu.scale;
    fit[13] = -gpu.center[1] * gpu.scale;
    fit[14] = -gpu.center[2] * gpu.scale;
    multiply(nodeMat, fit, out);
}

namespace {

void makeRay(const float viewProj[16], float ndcX, float ndcY, float origin[3], float dir[3]) {
    float inv[16];
    invert4(viewProj, inv);
    float nearP[3], farP[3];
    transformPoint4(inv, ndcX, ndcY, -1.0f, nearP);
    transformPoint4(inv, ndcX, ndcY, 1.0f, farP);
    for (int k = 0; k < 3; ++k) { origin[k] = nearP[k]; dir[k] = farP[k] - nearP[k]; }
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len > 0.0f) { dir[0] /= len; dir[1] /= len; dir[2] /= len; }
}

}  // namespace

bool GlRenderer::nodeScreenBox(uint32_t id, float out[4]) {
    const Node* node = scene_.find(id);
    if (!node) return false;
    GpuMesh* gpu = ensureGpuMesh(node->meshRef);
    if (!gpu || gpu->indexCount == 0) return false;
    float viewProj[16], world[16], mvp[16];
    buildViewProj(viewProj);
    nodeWorld(*node, *gpu, world);
    multiply(viewProj, world, mvp);
    float centerWorld[3];
    transformPoint4(world, gpu->center[0], gpu->center[1], gpu->center[2], centerWorld);
    const float worldScale = std::sqrt(world[0] * world[0] + world[1] * world[1] + world[2] * world[2]);
    const float radiusWorld = gpu->radius * worldScale;
    float centerNdc[3], topNdc[3];
    transformPoint4(mvp, gpu->center[0], gpu->center[1], gpu->center[2], centerNdc);
    // Approximate screen radius by projecting a point one radius up in world Y.
    const float topWorld[3] = {centerWorld[0], centerWorld[1] + radiusWorld, centerWorld[2]};
    transformPoint4(viewProj, topWorld[0], topWorld[1], topWorld[2], topNdc);
    const float ry = std::fabs(topNdc[1] - centerNdc[1]);
    // Same pixel radius on both axes: NDC x-range scales with height/width.
    const float rxNdc = (width_ > 0 && height_ > 0)
                            ? ry * static_cast<float>(height_) / static_cast<float>(width_)
                            : ry;
    out[0] = centerNdc[0] - rxNdc;
    out[1] = centerNdc[1] - ry;
    out[2] = centerNdc[0] + rxNdc;
    out[3] = centerNdc[1] + ry;
    return true;
}

int GlRenderer::pickNode(float ndcX, float ndcY) {
    float viewProj[16];
    buildViewProj(viewProj);
    float origin[3], dir[3];
    makeRay(viewProj, ndcX, ndcY, origin, dir);

    int bestId = -1;
    float bestDist = 1e30f;
    for (const auto& node : scene_.nodes()) {
        if (!node.visible) continue;
        GpuMesh* gpu = ensureGpuMesh(node.meshRef);
        if (!gpu || gpu->indexCount == 0) continue;
        const auto cpuIt = cpuMeshes_.find(node.meshRef);
        if (cpuIt == cpuMeshes_.end()) continue;
        float world[16], invWorld[16], mvp[16];
        nodeWorld(node, *gpu, world);
        multiply(viewProj, world, mvp);
        if (!invert4(world, invWorld)) continue;
        float localOrigin[3], localPoint[3];
        transformPoint4(invWorld, origin[0], origin[1], origin[2], localOrigin);
        const float worldDirPoint[3] = {origin[0] + dir[0], origin[1] + dir[1], origin[2] + dir[2]};
        transformPoint4(invWorld, worldDirPoint[0], worldDirPoint[1], worldDirPoint[2], localPoint);
        float localDir[3] = {localPoint[0] - localOrigin[0], localPoint[1] - localOrigin[1], localPoint[2] - localOrigin[2]};
        const float len = std::sqrt(localDir[0] * localDir[0] + localDir[1] * localDir[1] + localDir[2] * localDir[2]);
        if (len == 0.0f) continue;
        localDir[0] /= len; localDir[1] /= len; localDir[2] /= len;
        const RayHit hit = raycastMesh(cpuIt->second, localOrigin, localDir);
        if (!hit.hit) continue;
        float worldHit[3];
        transformPoint4(world, hit.point[0], hit.point[1], hit.point[2], worldHit);
        const float dx = worldHit[0] - origin[0], dy = worldHit[1] - origin[1], dz = worldHit[2] - origin[2];
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < bestDist) { bestDist = dist; bestId = static_cast<int>(node.id); }
    }
    return bestId;
}

int GlRenderer::brushStroke(uint32_t nodeId, float ndcX, float ndcY, int phase) {
    if (phase == 2) {
        if (strokeActive_ && !strokeBefore_.empty()) {
            const Node* node = scene_.find(strokeNode_);
            if (node) {
                const auto it = cpuMeshes_.find(node->meshRef);
                if (it != cpuMeshes_.end()) {
                    std::vector<uint32_t> indices;
                    std::vector<float> before, after;
                    indices.reserve(strokeBefore_.size());
                    for (const auto& [idx, values] : strokeBefore_) {
                        indices.push_back(idx);
                        before.insert(before.end(), values.begin(), values.end());
                        const float* current = &it->second.vertices[idx * 6];
                        after.insert(after.end(), current, current + 6);
                    }
                    VertexSnapshotCommand command;
                    command.setData(indices, before, after);
                    sculptUndo_.push_back({node->meshRef, std::move(command)});
                    if (sculptUndo_.size() > 50) sculptUndo_.erase(sculptUndo_.begin());
                    sculptRedo_.clear();
                }
            }
        }
        strokeActive_ = false;
        strokeBefore_.clear();
        return 0;
    }

    Node* node = scene_.find(nodeId);
    if (!node || !node->visible) return 0;
    GpuMesh* gpu = ensureGpuMesh(node->meshRef);
    if (!gpu || gpu->indexCount == 0) return 0;
    auto cpuIt = cpuMeshes_.find(node->meshRef);
    if (cpuIt == cpuMeshes_.end()) return 0;
    Mesh& mesh = cpuIt->second;

    if (phase == 0) {
        strokeActive_ = true;
        strokeNode_ = nodeId;
        strokeBefore_.clear();
        if (adjacency_.find(node->meshRef) == adjacency_.end()) {
            DeformRegion region;
            region.buildAdjacency(mesh);
            adjacency_[node->meshRef] = std::move(region);
        }
    }
    if (!strokeActive_ || strokeNode_ != nodeId) return 0;

    float viewProj[16], world[16], invWorld[16];
    buildViewProj(viewProj);
    nodeWorld(*node, *gpu, world);
    if (!invert4(world, invWorld)) return 0;
    float origin[3], dir[3];
    makeRay(viewProj, ndcX, ndcY, origin, dir);
    float localOrigin[3], localTarget[3];
    transformPoint4(invWorld, origin[0], origin[1], origin[2], localOrigin);
    transformPoint4(invWorld, origin[0] + dir[0], origin[1] + dir[1], origin[2] + dir[2], localTarget);
    float localDir[3] = {localTarget[0] - localOrigin[0], localTarget[1] - localOrigin[1], localTarget[2] - localOrigin[2]};
    const float dirLen = std::sqrt(localDir[0] * localDir[0] + localDir[1] * localDir[1] + localDir[2] * localDir[2]);
    if (dirLen == 0.0f) return 0;
    localDir[0] /= dirLen; localDir[1] /= dirLen; localDir[2] /= dirLen;

    const RayHit hit = raycastMesh(mesh, localOrigin, localDir);
    if (!hit.hit) return 0;

    const float worldScale = std::sqrt(world[0] * world[0] + world[1] * world[1] + world[2] * world[2]);
    const float localRadius = worldScale > 0.0f ? brushRadius_ / worldScale : brushRadius_;
    const float radius2 = localRadius * localRadius;

    std::vector<uint32_t> affected;
    for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
        const float* p = &mesh.vertices[i * 6];
        const float dx = p[0] - hit.point[0], dy = p[1] - hit.point[1], dz = p[2] - hit.point[2];
        if (dx * dx + dy * dy + dz * dz <= radius2) affected.push_back(i);
    }
    if (affected.empty()) return 0;

    for (uint32_t idx : affected) {
        if (strokeBefore_.find(idx) == strokeBefore_.end()) {
            std::array<float, 6> values{};
            std::copy(&mesh.vertices[idx * 6], &mesh.vertices[idx * 6] + 6, values.begin());
            strokeBefore_[idx] = values;
        }
    }

    DeformRegion& adjacency = adjacency_[node->meshRef];
    if (brushMode_ == 2) {
        // Smooth: move each affected vertex towards the average of its neighbours.
        std::vector<std::array<float, 3>> targets;
        targets.reserve(affected.size());
        const auto& faces = adjacency.vertexFaces();
        for (uint32_t idx : affected) {
            std::unordered_set<uint32_t> neighbours;
            if (idx < faces.size()) {
                for (uint32_t f : faces[idx]) {
                    for (int k = 0; k < 3; ++k) {
                        const uint32_t other = mesh.indices[f * 3 + k];
                        if (other != idx) neighbours.insert(other);
                    }
                }
            }
            std::array<float, 3> avg = {0, 0, 0};
            for (uint32_t other : neighbours) {
                const float* p = &mesh.vertices[other * 6];
                avg[0] += p[0]; avg[1] += p[1]; avg[2] += p[2];
            }
            if (!neighbours.empty()) {
                const float inv = 1.0f / static_cast<float>(neighbours.size());
                avg = {avg[0] * inv, avg[1] * inv, avg[2] * inv};
            } else {
                const float* p = &mesh.vertices[idx * 6];
                avg = {p[0], p[1], p[2]};
            }
            targets.push_back(avg);
        }
        const float blend = std::min(1.0f, brushStrength_ * 10.0f);
        for (size_t i = 0; i < affected.size(); ++i) {
            float* p = &mesh.vertices[affected[i] * 6];
            for (int k = 0; k < 3; ++k) p[k] += (targets[i][k] - p[k]) * blend;
        }
    } else {
        const float sign = brushMode_ == 1 ? -1.0f : 1.0f;
        for (uint32_t idx : affected) {
            float* p = &mesh.vertices[idx * 6];
            const float dx = p[0] - hit.point[0], dy = p[1] - hit.point[1], dz = p[2] - hit.point[2];
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float falloff = localRadius > 0.0f ? std::max(0.0f, 1.0f - dist / localRadius) : 0.0f;
            const float amount = sign * brushStrength_ * falloff / std::max(worldScale, 1e-6f);
            if (brushMode_ == 3) {
                p[0] += p[3] * amount; p[1] += p[4] * amount; p[2] += p[5] * amount;
            } else {
                p[0] += hit.normal[0] * amount; p[1] += hit.normal[1] * amount; p[2] += hit.normal[2] * amount;
            }
        }
    }

    adjacency.setSelected(affected);
    adjacency.recomputeNormals(&mesh);
    sculptDirty_.insert(node->meshRef);

    // Dirty-range upload of the affected vertices (sorted, merged runs).
    std::sort(affected.begin(), affected.end());
    glBindBuffer(GL_ARRAY_BUFFER, gpu->vbo);
    size_t i = 0;
    while (i < affected.size()) {
        size_t j = i;
        while (j + 1 < affected.size() && affected[j + 1] == affected[j] + 1) ++j;
        const uint32_t first = affected[i];
        const size_t count = j - i + 1;
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(first * 6 * sizeof(float)),
                        static_cast<GLsizeiptr>(count * 6 * sizeof(float)), &mesh.vertices[first * 6]);
        i = j + 1;
    }
    return static_cast<int>(affected.size());
}

bool GlRenderer::undoSculpt() {
    if (sculptUndo_.empty()) return false;
    SculptEntry entry = std::move(sculptUndo_.back());
    sculptUndo_.pop_back();
    const auto it = cpuMeshes_.find(entry.ref);
    if (it == cpuMeshes_.end()) return false;
    entry.command.undo(&it->second);
    sculptDirty_.insert(entry.ref);
    const auto gpuIt = gpuMeshes_.find(entry.ref);
    if (gpuIt != gpuMeshes_.end() && gpuIt->second.vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, gpuIt->second.vbo);
        glBufferData(GL_ARRAY_BUFFER, it->second.vertices.size() * sizeof(float), it->second.vertices.data(), GL_DYNAMIC_DRAW);
    }
    sculptRedo_.push_back(std::move(entry));
    return true;
}

bool GlRenderer::redoSculpt() {
    if (sculptRedo_.empty()) return false;
    SculptEntry entry = std::move(sculptRedo_.back());
    sculptRedo_.pop_back();
    const auto it = cpuMeshes_.find(entry.ref);
    if (it == cpuMeshes_.end()) return false;
    entry.command.redo(&it->second);
    sculptDirty_.insert(entry.ref);
    const auto gpuIt = gpuMeshes_.find(entry.ref);
    if (gpuIt != gpuMeshes_.end() && gpuIt->second.vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, gpuIt->second.vbo);
        glBufferData(GL_ARRAY_BUFFER, it->second.vertices.size() * sizeof(float), it->second.vertices.data(), GL_DYNAMIC_DRAW);
    }
    sculptUndo_.push_back(std::move(entry));
    return true;
}

bool GlRenderer::exportScene(const std::string& path, std::string* error) {
    Mesh combined;
    uint32_t exportedNodes = 0;
    for (const auto& node : scene_.nodes()) {
        if (!node.visible) continue;
        Mesh cpu;
        if (!cpuMeshForRef(node.meshRef, &cpu, error)) return false;
        float mat[16];
        node.transform.toMatrix(mat);
        const uint32_t base = combined.vertexCount();
        for (uint32_t i = 0; i < cpu.vertexCount(); ++i) {
            const float* v = &cpu.vertices[i * 6];
            float p[3], n[3];
            transformPoint4(mat, v[0], v[1], v[2], p);
            // Normal transform: use the upper 3x3 (correct for rotation +
            // uniform scale; non-uniform scale is approximated, then normalized).
            n[0] = mat[0] * v[3] + mat[4] * v[4] + mat[8] * v[5];
            n[1] = mat[1] * v[3] + mat[5] * v[4] + mat[9] * v[5];
            n[2] = mat[2] * v[3] + mat[6] * v[4] + mat[10] * v[5];
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 0.0f) { n[0] /= len; n[1] /= len; n[2] /= len; }
            combined.vertices.insert(combined.vertices.end(), {p[0], p[1], p[2], n[0], n[1], n[2]});
        }
        for (uint32_t idx : cpu.indices) combined.indices.push_back(base + idx);
        ++exportedNodes;
    }
    if (exportedNodes == 0) {
        if (error) *error = "no visible nodes to export";
        return false;
    }
    LOGI("export scene: %u nodes, %u tris", exportedNodes, combined.triangleCount());
    return exportGlb(combined, path, error);
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
