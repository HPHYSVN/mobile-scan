package com.mobile3d.validation

/** JNI surface for libmobile3d.so. Handles are opaque native pointers. */
object NativeEngine {
    init {
        System.loadLibrary("mobile3d")
    }

    external fun nativeCreate(): Long

    /** Trivial round-trip for JNI overhead measurement. */
    external fun nativePing(handle: Long): Long
    external fun nativeDestroy(handle: Long)
    external fun nativeInitSurface(handle: Long)
    external fun nativeResize(handle: Long, width: Int, height: Int)
    external fun nativeDrawFrame(handle: Long)

    /** Loads a glTF file on the GL thread. Returns load ms, or -1 on failure. */
    external fun nativeLoadMesh(handle: Long, path: String): Double
    external fun nativeSetOrbit(handle: Long, yaw: Float, pitch: Float, distance: Float)

    external fun nativeSetDeformEnabled(handle: Long, enabled: Boolean)

    /** Returns [frameCount, avgFrameMs, lastFrameMs, vertexCount, triangleCount,
     *  avgDeformMs, avgNormalMs, avgUploadMs, deformVertexCount]. */
    external fun nativeGetFrameStats(handle: Long): DoubleArray
    external fun nativeGetGlInfo(handle: Long): String
}
