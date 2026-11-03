cmake_minimum_required(VERSION 3.22)
project(mobile3d_host_tests LANGUAGES CXX)

# Host tests for platform-independent core code only. No Android/GL/JNI here,
# so failures reproduce on the dev machine (Risk R6).
add_executable(test_mesh
    test_mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/mesh.cpp
)
target_include_directories(test_mesh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)
target_compile_features(test_mesh PRIVATE cxx_std_17)

add_executable(test_loader
    test_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/io/gltf_loader.cpp
)
target_include_directories(test_loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)

add_executable(bench_import
    bench_import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/io/gltf_loader.cpp
)
target_include_directories(bench_import PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)

add_executable(test_deform
    test_deform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/deform.cpp
)
target_include_directories(test_deform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)

add_executable(bench_deform
    bench_deform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/deform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/io/gltf_loader.cpp
)
target_include_directories(bench_deform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)

set(MOBILE3D_CORE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/deform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/undo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/raycast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/io/gltf_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/io/gltf_exporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/io/obj_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/io/mesh_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/third_party/meshoptimizer/simplifier.cpp
)

add_executable(test_io test_io.cpp ${MOBILE3D_CORE})
target_include_directories(test_io PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)

add_executable(test_undo test_undo.cpp ${MOBILE3D_CORE})
target_include_directories(test_undo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)

add_executable(test_scene_project
    test_scene_project.cpp
    ${MOBILE3D_CORE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/core/project.cpp
)
target_include_directories(test_scene_project PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)
target_compile_features(test_scene_project PRIVATE cxx_std_17)

enable_testing()
add_test(NAME mesh COMMAND test_mesh)
add_test(NAME loader COMMAND test_loader ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sphere_6k.glb)
add_test(NAME deform COMMAND test_deform)
add_test(NAME io COMMAND test_io ${CMAKE_BINARY_DIR})
add_test(NAME undo COMMAND test_undo)
add_test(NAME scene_project COMMAND test_scene_project ${CMAKE_BINARY_DIR})

add_executable(test_meshops test_meshops.cpp ${MOBILE3D_CORE})
target_include_directories(test_meshops PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)
add_test(NAME meshops COMMAND test_meshops ${CMAKE_BINARY_DIR})

add_executable(test_raycast test_raycast.cpp ${MOBILE3D_CORE})
target_include_directories(test_raycast PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
)
add_test(NAME raycast COMMAND test_raycast)
