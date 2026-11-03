#include <jni.h>

#include <string>

#include "../render/gl_renderer.h"

using mobile3d::GlRenderer;

namespace {
GlRenderer* fromHandle(jlong handle) { return reinterpret_cast<GlRenderer*>(handle); }

std::string toStdString(JNIEnv* env, jstring value) {
    if (!value) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string out = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(value, chars);
    return out;
}
}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeCreate(JNIEnv*, jobject) {
    return reinterpret_cast<jlong>(new GlRenderer());
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeDestroy(JNIEnv*, jobject, jlong handle) {
    auto* renderer = fromHandle(handle);
    if (renderer) {
        renderer->shutdown();
        delete renderer;
    }
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeInitSurface(JNIEnv*, jobject, jlong handle) {
    if (auto* r = fromHandle(handle)) r->init();
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeResize(JNIEnv*, jobject, jlong handle, jint w, jint h) {
    if (auto* r = fromHandle(handle)) r->resize(w, h);
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeDrawFrame(JNIEnv*, jobject, jlong handle) {
    if (auto* r = fromHandle(handle)) r->drawFrame();
}

// Trivial round-trip used to measure raw JNI call overhead (D9).
JNIEXPORT jlong JNICALL
Java_com_mobile3d_validation_NativeEngine_nativePing(JNIEnv*, jobject, jlong handle) {
    return handle;
}

JNIEXPORT jdouble JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeLoadMesh(JNIEnv* env, jobject, jlong handle, jstring path) {
    auto* r = fromHandle(handle);
    if (!r || !path) return -1.0;
    const char* chars = env->GetStringUTFChars(path, nullptr);
    const double ms = r->loadMesh(chars);
    env->ReleaseStringUTFChars(path, chars);
    return ms;
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetOrbit(JNIEnv*, jobject, jlong handle, jfloat yaw, jfloat pitch, jfloat distance) {
    if (auto* r = fromHandle(handle)) r->setOrbit(yaw, pitch, distance);
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetDeformEnabled(JNIEnv*, jobject, jlong handle, jboolean enabled) {
    if (auto* r = fromHandle(handle)) r->setDeformEnabled(enabled == JNI_TRUE);
}

JNIEXPORT jdoubleArray JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeGetFrameStats(JNIEnv* env, jobject, jlong handle) {
    jdoubleArray out = env->NewDoubleArray(9);
    double values[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (auto* r = fromHandle(handle)) {
        values[0] = static_cast<double>(r->frameCount());
        values[1] = r->avgFrameMs();
        values[2] = r->lastFrameMs();
        values[3] = static_cast<double>(r->vertexCount());
        values[4] = static_cast<double>(r->triangleCount());
        values[5] = r->avgDeformMs();
        values[6] = r->avgNormalMs();
        values[7] = r->avgUploadMs();
        values[8] = static_cast<double>(r->deformVertexCount());
    }
    env->SetDoubleArrayRegion(out, 0, 9, values);
    return out;
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeNewProject(JNIEnv* env, jobject, jlong handle, jstring dir) {
    if (auto* r = fromHandle(handle)) r->newProject(toStdString(env, dir));
}

JNIEXPORT jlong JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeAddPrimitive(JNIEnv* env, jobject, jlong handle, jstring kind) {
    auto* r = fromHandle(handle);
    return r ? static_cast<jlong>(r->addPrimitive(toStdString(env, kind))) : 0;
}

JNIEXPORT jlong JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeAddNode(JNIEnv* env, jobject, jlong handle, jstring name, jstring meshRef) {
    auto* r = fromHandle(handle);
    return r ? static_cast<jlong>(r->addNode(toStdString(env, name), toStdString(env, meshRef))) : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetNodeTransform(JNIEnv* env, jobject, jlong handle, jlong id, jfloatArray values) {
    auto* r = fromHandle(handle);
    if (!r || !values || env->GetArrayLength(values) < 9) return JNI_FALSE;
    float floats[9];
    env->GetFloatArrayRegion(values, 0, 9, floats);
    return r->setNodeTransform(static_cast<uint32_t>(id), floats) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeRemoveNode(JNIEnv*, jobject, jlong handle, jlong id) {
    auto* r = fromHandle(handle);
    return (r && r->removeNode(static_cast<uint32_t>(id))) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetPan(JNIEnv*, jobject, jlong handle, jfloat panX, jfloat panY) {
    if (auto* r = fromHandle(handle)) r->setPan(panX, panY);
}

JNIEXPORT jlong JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeDuplicateNode(JNIEnv*, jobject, jlong handle, jlong id) {
    auto* r = fromHandle(handle);
    return r ? static_cast<jlong>(r->duplicateNode(static_cast<uint32_t>(id))) : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetNodeColor(JNIEnv*, jobject, jlong handle, jlong id, jfloat r, jfloat g, jfloat b) {
    auto* renderer = fromHandle(handle);
    return (renderer && renderer->setNodeColor(static_cast<uint32_t>(id), r, g, b)) ? JNI_TRUE : JNI_FALSE;
}

// Line format per node: id;name;meshRef;visible;px;py;pz;rx;ry;rz;sx;sy;sz;cr;cg;cb
// (Names must not contain ';' in this validation build.)
JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeGetNodes(JNIEnv* env, jobject, jlong handle) {
    std::string out;
    if (auto* r = fromHandle(handle)) {
        char buf[640];
        for (const auto& node : r->scene().nodes()) {
            const auto& t = node.transform;
            std::snprintf(buf, sizeof(buf), "%u;%s;%s;%d;%g;%g;%g;%g;%g;%g;%g;%g;%g;%g;%g;%g\n",
                          node.id, node.name.c_str(), node.meshRef.c_str(), node.visible ? 1 : 0,
                          t.position[0], t.position[1], t.position[2],
                          t.rotationEuler[0], t.rotationEuler[1], t.rotationEuler[2],
                          t.scale[0], t.scale[1], t.scale[2],
                          node.color[0], node.color[1], node.color[2]);
            out += buf;
        }
    }
    return env->NewStringUTF(out.c_str());
}

// Returns empty string on success, error message otherwise.
JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSaveProject(JNIEnv* env, jobject, jlong handle) {
    auto* r = fromHandle(handle);
    std::string error = r ? "" : "invalid handle";
    if (r && !r->saveProject(&error)) { /* error set */ }
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeOpenProject(JNIEnv* env, jobject, jlong handle, jstring dir) {
    auto* r = fromHandle(handle);
    std::string error = r ? "" : "invalid handle";
    if (r) r->openProject(toStdString(env, dir), &error);
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetBrush(JNIEnv*, jobject, jlong handle, jfloat radius, jfloat strength, jint mode) {
    if (auto* r = fromHandle(handle)) r->setBrush(radius, strength, mode);
}

JNIEXPORT jint JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeBrushStroke(JNIEnv*, jobject, jlong handle, jlong nodeId, jfloat ndcX, jfloat ndcY, jint phase) {
    auto* r = fromHandle(handle);
    return r ? r->brushStroke(static_cast<uint32_t>(nodeId), ndcX, ndcY, phase) : 0;
}

JNIEXPORT jlong JNICALL
Java_com_mobile3d_validation_NativeEngine_nativePickNode(JNIEnv*, jobject, jlong handle, jfloat ndcX, jfloat ndcY) {
    auto* r = fromHandle(handle);
    return r ? static_cast<jlong>(r->pickNode(ndcX, ndcY)) : -1;
}

JNIEXPORT jboolean JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeUndoSculpt(JNIEnv*, jobject, jlong handle) {
    auto* r = fromHandle(handle);
    return (r && r->undoSculpt()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeRedoSculpt(JNIEnv*, jobject, jlong handle) {
    auto* r = fromHandle(handle);
    return (r && r->redoSculpt()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeExportNodeEx(JNIEnv* env, jobject, jlong handle, jlong id, jstring path, jint format, jint resolutionPercent, jint subdivideLevels) {
    auto* r = fromHandle(handle);
    std::string error = r ? "" : "invalid handle";
    if (r) r->exportNodeEx(static_cast<uint32_t>(id), toStdString(env, path), format, resolutionPercent, subdivideLevels, &error);
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT jfloatArray JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeGetNodeScreenBox(JNIEnv* env, jobject, jlong handle, jlong id) {
    auto* r = fromHandle(handle);
    float box[4] = {0, 0, 0, 0};
    const bool ok = r && r->nodeScreenBox(static_cast<uint32_t>(id), box);
    jfloatArray out = env->NewFloatArray(ok ? 4 : 0);
    if (ok) env->SetFloatArrayRegion(out, 0, 4, box);
    return out;
}

JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeExportScene(JNIEnv* env, jobject, jlong handle, jstring path) {
    auto* r = fromHandle(handle);
    std::string error = r ? "" : "invalid handle";
    if (r) r->exportScene(toStdString(env, path), &error);
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetDisplayMode(JNIEnv*, jobject, jlong handle, jint mode) {
    if (auto* r = fromHandle(handle)) r->setDisplayMode(mode);
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSetShowGrid(JNIEnv*, jobject, jlong handle, jboolean show) {
    if (auto* r = fromHandle(handle)) r->setShowGrid(show == JNI_TRUE);
}

JNIEXPORT jint JNICALL
Java_com_mobile3d_validation_NativeEngine_nativePickElement(JNIEnv*, jobject, jlong handle, jlong nodeId, jint mode, jfloat ndcX, jfloat ndcY) {
    auto* r = fromHandle(handle);
    return r ? r->pickElement(static_cast<uint32_t>(nodeId), mode, ndcX, ndcY) : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeMoveElement(JNIEnv*, jobject, jlong handle, jfloat dxPx, jfloat dyPx, jfloat worldPerPixel) {
    auto* r = fromHandle(handle);
    return (r && r->moveElement(dxPx, dyPx, worldPerPixel)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeEndElementEdit(JNIEnv*, jobject, jlong handle) {
    if (auto* r = fromHandle(handle)) r->endElementEdit();
}

JNIEXPORT jboolean JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeCaptureThumbnail(JNIEnv* env, jobject, jlong handle, jstring path) {
    auto* r = fromHandle(handle);
    return (r && r->captureThumbnail(toStdString(env, path), 480)) ? JNI_TRUE : JNI_FALSE;
}

// Save + thumbnail in ONE GL-thread event so autosave-on-pause cannot race
// surface teardown (thumbnail queued separately never ran while paused).
JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeSaveProjectWithThumbnail(JNIEnv* env, jobject, jlong handle, jstring thumbPath) {
    auto* r = fromHandle(handle);
    std::string error = r ? "" : "invalid handle";
    if (r && r->saveProject(&error)) {
        if (!r->captureThumbnail(toStdString(env, thumbPath), 480)) {
            error = "thumbnail failed";
        }
    }
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeGetGlInfo(JNIEnv* env, jobject, jlong handle) {
    auto* r = fromHandle(handle);
    return env->NewStringUTF(r ? r->glInfo().c_str() : "unavailable");
}

}  // extern "C"
