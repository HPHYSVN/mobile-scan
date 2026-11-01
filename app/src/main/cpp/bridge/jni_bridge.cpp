#include <jni.h>

#include "../render/gl_renderer.h"

using mobile3d::GlRenderer;

namespace {
GlRenderer* fromHandle(jlong handle) { return reinterpret_cast<GlRenderer*>(handle); }
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

JNIEXPORT jstring JNICALL
Java_com_mobile3d_validation_NativeEngine_nativeGetGlInfo(JNIEnv* env, jobject, jlong handle) {
    auto* r = fromHandle(handle);
    return env->NewStringUTF(r ? r->glInfo().c_str() : "unavailable");
}

}  // extern "C"
