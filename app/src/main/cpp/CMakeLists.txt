cmake_minimum_required(VERSION 3.22.1)
project(mobile3d LANGUAGES CXX)

add_library(mobile3d SHARED
    core/mesh.cpp
    core/deform.cpp
    core/undo.cpp
    core/raycast.cpp
    core/scene.cpp
    core/project.cpp
    io/gltf_loader.cpp
    io/gltf_exporter.cpp
    io/obj_loader.cpp
    io/mesh_ops.cpp
    third_party/meshoptimizer/simplifier.cpp
    render/gl_renderer.cpp
    bridge/jni_bridge.cpp
)

target_compile_features(mobile3d PRIVATE cxx_std_17)
# Required for devices with 16 KB page size (Android 15+).
target_link_options(mobile3d PRIVATE "-Wl,-z,max-page-size=16384")

find_library(log-lib log)
find_library(gles-lib GLESv3)
target_link_libraries(mobile3d PRIVATE ${log-lib} ${gles-lib} android)
