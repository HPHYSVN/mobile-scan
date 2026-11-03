package com.mobile3d.validation

import android.content.Context
import android.opengl.GLSurfaceView
import android.util.Log
import android.view.MotionEvent
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import kotlin.math.abs

class ViewportView(context: Context) : GLSurfaceView(context) {

    private val handle: Long = NativeEngine.nativeCreate()

    @Volatile private var yaw = 0.6f
    @Volatile private var pitch = 0.4f
    @Volatile private var distance = 4.2f

    private var lastX = 0f
    private var lastY = 0f
    private var lastPinch = 0f

    init {
        setEGLContextClientVersion(3)
        setRenderer(object : Renderer {
            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                NativeEngine.nativeInitSurface(handle)
                Log.i(TAG, "GL info: ${NativeEngine.nativeGetGlInfo(handle)}")
            }

            override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                NativeEngine.nativeResize(handle, width, height)
            }

            override fun onDrawFrame(gl: GL10?) {
                NativeEngine.nativeSetOrbit(handle, yaw, pitch, distance)
                NativeEngine.nativeDrawFrame(handle)
            }
        })
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    /** Must be called so the load runs on the GL thread. Callback receives load ms or -1. */
    fun loadMesh(path: String, onLoaded: (Double) -> Unit) {
        queueEvent {
            val ms = NativeEngine.nativeLoadMesh(handle, path)
            Log.i(TAG, "loadMesh($path) -> $ms ms")
            post { onLoaded(ms) }
        }
    }

    data class NodeInfo(
        val id: Long, val name: String, val meshRef: String, val visible: Boolean,
        val transform: FloatArray,
        val color: FloatArray = floatArrayOf(0.25f, 0.55f, 0.9f),
    )

    private fun <T> onGl(block: () -> T, callback: (T) -> Unit) {
        queueEvent {
            val result = block()
            post { callback(result) }
        }
    }

    fun newProject(dir: String, callback: () -> Unit = {}) =
        onGl({ NativeEngine.nativeNewProject(handle, dir) }) { callback() }

    fun addPrimitive(kind: String, callback: (Long) -> Unit) =
        onGl({ NativeEngine.nativeAddPrimitive(handle, kind) }, callback)

    fun addNode(name: String, meshRef: String, callback: (Long) -> Unit) =
        onGl({ NativeEngine.nativeAddNode(handle, name, meshRef) }, callback)

    fun setNodeTransform(id: Long, values: FloatArray) {
        queueEvent { NativeEngine.nativeSetNodeTransform(handle, id, values) }
    }

    fun setNodeColor(id: Long, r: Float, g: Float, b: Float) {
        queueEvent { NativeEngine.nativeSetNodeColor(handle, id, r, g, b) }
    }

    fun removeNode(id: Long, callback: (Boolean) -> Unit) =
        onGl({ NativeEngine.nativeRemoveNode(handle, id) }, callback)

    fun exportNode(id: Long, path: String, callback: (String) -> Unit) =
        onGl({ NativeEngine.nativeExportNode(handle, id, path) }, callback)

    fun saveProject(callback: (String) -> Unit) =
        onGl({ NativeEngine.nativeSaveProject(handle) }, callback)

    fun openProject(dir: String, callback: (String) -> Unit) =
        onGl({ NativeEngine.nativeOpenProject(handle, dir) }, callback)

    fun fetchNodes(callback: (List<NodeInfo>) -> Unit) =
        onGl({ parseNodes(NativeEngine.nativeGetNodes(handle)) }, callback)

    private fun parseNodes(raw: String): List<NodeInfo> =
        raw.lineSequence().filter { it.isNotBlank() }.map { line ->
            val p = line.split(";")
            NodeInfo(
                id = p[0].toLong(), name = p[1], meshRef = p[2], visible = p[3] == "1",
                transform = FloatArray(9) { p[4 + it].toFloat() },
                color = if (p.size >= 16) FloatArray(3) { p[13 + it].toFloat() } else floatArrayOf(0.25f, 0.55f, 0.9f),
            )
        }.toList()

    fun frameStats(): DoubleArray = NativeEngine.nativeGetFrameStats(handle)

    fun setDeformEnabled(enabled: Boolean) {
        queueEvent { NativeEngine.nativeSetDeformEnabled(handle, enabled) }
    }

    fun release() {
        queueEvent { NativeEngine.nativeDestroy(handle) }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = event.x
                lastY = event.y
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (event.pointerCount == 2) lastPinch = pinchDistance(event)
            }
            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 1) {
                    yaw += (event.x - lastX) * 0.01f
                    pitch = (pitch + (event.y - lastY) * 0.01f).coerceIn(-1.4f, 1.4f)
                    lastX = event.x
                    lastY = event.y
                } else if (event.pointerCount == 2) {
                    val d = pinchDistance(event)
                    if (lastPinch > 0f && d > 0f) {
                        distance = (distance * lastPinch / d).coerceIn(1.2f, 12f)
                    }
                    lastPinch = d
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> lastPinch = 0f
        }
        return true
    }

    private fun pinchDistance(event: MotionEvent): Float {
        val dx = event.getX(0) - event.getX(1)
        val dy = event.getY(0) - event.getY(1)
        return kotlin.math.sqrt(dx * dx + dy * dy).takeIf { abs(it) > 0f } ?: 0f
    }

    companion object {
        private const val TAG = "Mobile3D"
    }
}
