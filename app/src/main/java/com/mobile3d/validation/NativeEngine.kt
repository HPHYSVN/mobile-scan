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

    // --- Scene / project (M1). All must be called on the GL thread. ---
    external fun nativeNewProject(handle: Long, dir: String)
    external fun nativeAddPrimitive(handle: Long, kind: String): Long
    external fun nativeAddNode(handle: Long, name: String, meshRef: String): Long
    external fun nativeSetNodeTransform(handle: Long, id: Long, values: FloatArray): Boolean
    external fun nativeSetNodeColor(handle: Long, id: Long, r: Float, g: Float, b: Float): Boolean
    external fun nativeSetPan(handle: Long, panX: Float, panY: Float)
    external fun nativeDuplicateNode(handle: Long, id: Long): Long
    external fun nativeRemoveNode(handle: Long, id: Long): Boolean
    external fun nativeGetNodes(handle: Long): String
    external fun nativeSaveProject(handle: Long): String
    external fun nativeExportNode(handle: Long, id: Long, path: String): String
    external fun nativeOpenProject(handle: Long, dir: String): String

    /** Returns [frameCount, avgFrameMs, lastFrameMs, vertexCount, triangleCount,
     *  avgDeformMs, avgNormalMs, avgUploadMs, deformVertexCount]. */
    external fun nativeGetFrameStats(handle: Long): DoubleArray
    external fun nativeGetGlInfo(handle: Long): String
}
