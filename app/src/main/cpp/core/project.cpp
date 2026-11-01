#include "project.h"

#include <filesystem>
#include <fstream>
#include <sstream>

// nlohmann/json is already vendored with tinygltf (MIT). Reusing it here
// avoids a hand-written JSON parser; see THIRD_PARTY_LICENSES.md.
#include "../third_party/tinygltf/json.hpp"

namespace mobile3d {
namespace {

using nlohmann::json;
namespace fs = std::filesystem;

bool writeFile(const fs::path& path, const std::string& content, std::string* error) {
    std::ofstream out(path);
    if (!out) {
        if (error) *error = "cannot write " + path.string();
        return false;
    }
    out << content;
    return static_cast<bool>(out);
}

bool readFile(const fs::path& path, std::string* content, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        if (error) *error = "cannot read " + path.string();
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    *content = ss.str();
    return true;
}

json vec3(const float v[3]) { return json::array({v[0], v[1], v[2]}); }

void readVec3(const json& value, float out[3], const float fallback[3]) {
    if (value.is_array() && value.size() == 3) {
        for (int i = 0; i < 3; ++i) out[i] = value[i].get<float>();
    } else {
        for (int i = 0; i < 3; ++i) out[i] = fallback[i];
    }
}

}  // namespace

bool saveProject(const std::string& dir, const Scene& scene, std::string* error) {
    std::error_code ec;
    fs::create_directories(fs::path(dir) / "meshes", ec);
    if (ec) {
        if (error) *error = "cannot create project dir: " + ec.message();
        return false;
    }

    json manifest;
    manifest["schemaVersion"] = kProjectSchemaVersion;
    manifest["createdWith"] = "mobile3d-validation 0.1.0";
    if (!writeFile(fs::path(dir) / "manifest.json", manifest.dump(2), error)) return false;

    json sceneJson;
    sceneJson["nodes"] = json::array();
    for (const auto& node : scene.nodes()) {
        json n;
        n["id"] = node.id;
        n["name"] = node.name;
        n["mesh"] = node.meshRef;
        n["visible"] = node.visible;
        n["transform"]["position"] = vec3(node.transform.position);
        n["transform"]["rotationEuler"] = vec3(node.transform.rotationEuler);
        n["transform"]["scale"] = vec3(node.transform.scale);
        sceneJson["nodes"].push_back(n);
    }
    return writeFile(fs::path(dir) / "scene.json", sceneJson.dump(2), error);
}

bool loadProject(const std::string& dir, Scene* out, std::string* error) {
    std::string manifestText;
    if (!readFile(fs::path(dir) / "manifest.json", &manifestText, error)) return false;

    json manifest;
    try {
        manifest = json::parse(manifestText);
    } catch (const std::exception& e) {
        if (error) *error = std::string("manifest parse error: ") + e.what();
        return false;
    }

    const int version = manifest.value("schemaVersion", -1);
    if (version < 0) {
        if (error) *error = "manifest missing schemaVersion (unsupported legacy format)";
        return false;
    }
    if (version > kProjectSchemaVersion) {
        if (error) *error = "project schemaVersion " + std::to_string(version) +
                            " is newer than supported " + std::to_string(kProjectSchemaVersion);
        return false;
    }
    // Migration steps for older versions belong here, one function per
    // version bump, exercised by fixture projects in cpp/tests.

    std::string sceneText;
    if (!readFile(fs::path(dir) / "scene.json", &sceneText, error)) return false;
    json sceneJson;
    try {
        sceneJson = json::parse(sceneText);
    } catch (const std::exception& e) {
        if (error) *error = std::string("scene parse error: ") + e.what();
        return false;
    }

    std::vector<Node> nodes;
    uint32_t maxId = 0;
    for (const auto& n : sceneJson.value("nodes", json::array())) {
        Node node;
        node.id = n.value("id", 0u);
        node.name = n.value("name", std::string());
        node.meshRef = n.value("mesh", std::string());
        node.visible = n.value("visible", true);
        const auto& t = n.value("transform", json::object());
        const float zero[3] = {0, 0, 0};
        const float one[3] = {1, 1, 1};
        readVec3(t.value("position", json()), node.transform.position, zero);
        readVec3(t.value("rotationEuler", json()), node.transform.rotationEuler, zero);
        readVec3(t.value("scale", json()), node.transform.scale, one);
        maxId = std::max(maxId, node.id);
        nodes.push_back(std::move(node));
    }

    out->restore(std::move(nodes), maxId + 1);
    return true;
}

}  // namespace mobile3d
