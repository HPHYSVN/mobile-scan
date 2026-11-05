package com.mobile3d.validation

import android.content.Context
import android.opengl.GLSurfaceView
import android.util.Log
import android.view.MotionEvent
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import kotlin.math.abs

/** Applies system-window + display-cutout insets so overlay bars never sit
 * under the status bar, punch-hole camera, or gesture/navigation bar. */
fun android.view.View.applySafeAreaInsets(
    onInsets: (top: Int, bottom: Int, left: Int, right: Int) -> Unit,
) {
    setOnApplyWindowInsetsListener { _, insets ->
        val cutout = insets.displayCutout
        val top = maxOf(insets.systemWindowInsetTop, cutout?.safeInsetTop ?: 0)
        val bottom = maxOf(insets.systemWindowInsetBottom, cutout?.safeInsetBottom ?: 0)
        val left = maxOf(insets.systemWindowInsetLeft, cutout?.safeInsetLeft ?: 0)
        val right = maxOf(insets.systemWindowInsetRight, cutout?.safeInsetRight ?: 0)
        onInsets(top, bottom, left, right)
        insets
    }
    requestApplyInsets()
}

class ViewportView(context: Context) : GLSurfaceView(context) {

    private val handle: Long = NativeEngine.nativeCreate()

    @Volatile private var yaw = 0.6f
    @Volatile private var pitch = 0.4f
    @Volatile private var distance = 4.2f

    private var lastX = 0f
    private var lastY = 0f
    private var lastPinch = 0f
    private var lastCentroidX = 0f
    private var lastCentroidY = 0f
    private var threeFingerStartY = 0f
    private var downTime = 0L
    private var downX = 0f
    private var downY = 0f
    private var threeFingerDownTime = 0L
    private var threeFingerMoved = false
    private var lastTapTime = 0L
    private var lastTapX = 0f
    private var lastTapY = 0f

    enum class Mode { VIEW, EDIT, SCULPT }
    @Volatile var mode: Mode = Mode.VIEW

    @Volatile private var panX = 0f
    @Volatile private var panY = 0f

    // Gesture callbacks wired by the activity (world units precomputed there).
    var onEditDrag: ((dxPx: Float, dyPx: Float) -> Unit)? = null
    var onEditMoveY: ((dyPx: Float) -> Unit)? = null
    var onDuplicateRequest: (() -> Unit)? = null
    var onBrush: ((ndcX: Float, ndcY: Float, phase: Int) -> Unit)? = null  // phase 0=start 1=move 2=end
    var onTapPick: ((ndcX: Float, ndcY: Float) -> Unit)? = null
    var onElementTap: ((ndcX: Float, ndcY: Float) -> Unit)? = null
    var onElementDrag: ((dxPx: Float, dyPx: Float) -> Unit)? = null
    var onElementEnd: (() -> Unit)? = null
    var onThreeFingerTap: (() -> Unit)? = null

    fun resetCamera() {
        yaw = 0.6f; pitch = 0.4f; distance = 4.2f; panX = 0f; panY = 0f
    }

    fun worldPerPixel(): Float =
        if (height > 0) (2.0 * distance * kotlin.math.tan(0.45) / height).toFloat() else 0.01f

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
                NativeEngine.nativeSetPan(handle, panX, panY)
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

    fun setBrush(radius: Float, strength: Float, mode: Int) {
        queueEvent { NativeEngine.nativeSetBrush(handle, radius, strength, mode) }
    }

    fun brushStroke(nodeId: Long, ndcX: Float, ndcY: Float, phase: Int, callback: (Int) -> Unit = {}) =
        onGl({ NativeEngine.nativeBrushStroke(handle, nodeId, ndcX, ndcY, phase) }, callback)

    fun pickNode(ndcX: Float, ndcY: Float, callback: (Long) -> Unit) =
        onGl({ NativeEngine.nativePickNode(handle, ndcX, ndcY) }, callback)

    fun setDisplayMode(mode: Int) {
        queueEvent { NativeEngine.nativeSetDisplayMode(handle, mode) }
    }

    fun setShowGrid(show: Boolean) {
        queueEvent { NativeEngine.nativeSetShowGrid(handle, show) }
    }

    @Volatile var elementMode: Int = 0  // 0=object, 1=vertex, 2=edge, 3=face (EDIT mode)

    fun pickElement(nodeId: Long, mode: Int, ndcX: Float, ndcY: Float, callback: (Int) -> Unit) =
        onGl({ NativeEngine.nativePickElement(handle, nodeId, mode, ndcX, ndcY) }, callback)

    fun moveElement(dxPx: Float, dyPx: Float) {
        queueEvent { NativeEngine.nativeMoveElement(handle, dxPx, dyPx, worldPerPixel()) }
    }

    fun endElementEdit() {
        queueEvent { NativeEngine.nativeEndElementEdit(handle) }
    }

    fun getNodeScreenBox(id: Long, callback: (FloatArray) -> Unit) =
        onGl({ NativeEngine.nativeGetNodeScreenBox(handle, id) }, callback)

    fun undoSculpt(callback: (Boolean) -> Unit) =
        onGl({ NativeEngine.nativeUndoSculpt(handle) }, callback)

    fun redoSculpt(callback: (Boolean) -> Unit) =
        onGl({ NativeEngine.nativeRedoSculpt(handle) }, callback)

    fun duplicateNode(id: Long, callback: (Long) -> Unit) =
        onGl({ NativeEngine.nativeDuplicateNode(handle, id) }, callback)

    fun removeNode(id: Long, callback: (Boolean) -> Unit) =
        onGl({ NativeEngine.nativeRemoveNode(handle, id) }, callback)

    fun exportScene(path: String, callback: (String) -> Unit) =
        onGl({ NativeEngine.nativeExportScene(handle, path) }, callback)

    fun exportNodeEx(id: Long, path: String, format: Int, resolutionPercent: Int, subdivideLevels: Int, callback: (String) -> Unit) =
        onGl({ NativeEngine.nativeExportNodeEx(handle, id, path, format, resolutionPercent, subdivideLevels) }, callback)

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

    fun saveProjectWithThumbnail(thumbPath: String, callback: (String) -> Unit) =
        onGl({ NativeEngine.nativeSaveProjectWithThumbnail(handle, thumbPath) }, callback)

    fun captureThumbnail(path: String, callback: (Boolean) -> Unit = {}) =
        onGl({ NativeEngine.nativeCaptureThumbnail(handle, path) }, callback)

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
                lastX = event.x; lastY = event.y
                downTime = System.currentTimeMillis(); downX = event.x; downY = event.y
                if (mode == Mode.SCULPT) {
                    onBrush?.invoke(ndcX(event.x), ndcY(event.y), 0)
                }
                // Double-tap in EDIT duplicates the selected node.
                val now = System.currentTimeMillis()
                if (mode == Mode.EDIT && now - lastTapTime < 350 &&
                    abs(event.x - lastTapX) < 60 && abs(event.y - lastTapY) < 60
                ) {
                    onDuplicateRequest?.invoke()
                    lastTapTime = 0
                } else if (mode == Mode.EDIT) {
                    lastTapTime = now; lastTapX = event.x; lastTapY = event.y
                }
                if (mode == Mode.VIEW) {
                    val now2 = System.currentTimeMillis()
                    if (now2 - lastTapTime < 350) onTapPick?.invoke(ndcX(event.x), ndcY(event.y))
                    lastTapTime = now2
                }
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (event.pointerCount == 2) {
                    lastPinch = pinchDistance(event)
                    lastCentroidX = (event.getX(0) + event.getX(1)) / 2
                    lastCentroidY = (event.getY(0) + event.getY(1)) / 2
                }
                if (event.pointerCount == 3) {
                    threeFingerStartY = (event.getY(0) + event.getY(1) + event.getY(2)) / 3
                    if (threeFingerDownTime == 0L) {
                        threeFingerDownTime = System.currentTimeMillis()
                        threeFingerMoved = false
                    }
                }
                if (mode == Mode.SCULPT && event.pointerCount == 2) {
                    onBrush?.invoke(0f, 0f, 2)  // end stroke when switching to orbit
                }
            }
            MotionEvent.ACTION_MOVE -> {
                when (event.pointerCount) {
                    1 -> when (mode) {
                        Mode.VIEW -> {
                            yaw += (event.x - lastX) * 0.01f
                            pitch = (pitch + (event.y - lastY) * 0.01f).coerceIn(-1.4f, 1.4f)
                        }
                        Mode.EDIT -> {
                            if (elementMode > 0) onElementDrag?.invoke(event.x - lastX, event.y - lastY)
                            else onEditDrag?.invoke(event.x - lastX, event.y - lastY)
                        }
                        Mode.SCULPT -> onBrush?.invoke(ndcX(event.x), ndcY(event.y), 1)
                    }
                    2 -> {
                        val d = pinchDistance(event)
                        val cx = (event.getX(0) + event.getX(1)) / 2
                        val cy = (event.getY(0) + event.getY(1)) / 2
                        // Two fingers are always camera: pinch zoom + drag pan
                        // (object scale lives in the corner handle / Scale slider).
                        if (lastPinch > 0f && d > 0f) {
                            distance = (distance * lastPinch / d).coerceIn(1.2f, 15f)
                        }
                        val wpp = worldPerPixel()
                        panX -= (cx - lastCentroidX) * wpp
                        panY += (cy - lastCentroidY) * wpp
                        lastPinch = d
                        lastCentroidX = cx; lastCentroidY = cy
                    }
                    3 -> {
                        val cy = (event.getY(0) + event.getY(1) + event.getY(2)) / 3
                        if (abs(cy - threeFingerStartY) > 8) threeFingerMoved = true
                        if (mode == Mode.EDIT) {
                            // Three-finger vertical drag: raise/lower the object.
                            onEditMoveY?.invoke(-(cy - threeFingerStartY))
                        } else {
                            // Elsewhere: three-finger vertical drag pans the camera vertically.
                            panY += (cy - threeFingerStartY) * worldPerPixel()
                        }
                        threeFingerStartY = cy
                    }
                }
                lastX = event.x; lastY = event.y
            }
            MotionEvent.ACTION_UP -> {
                // Three-finger quick tap resets the camera.
                if (threeFingerDownTime > 0L && !threeFingerMoved &&
                    System.currentTimeMillis() - threeFingerDownTime < 350
                ) {
                    resetCamera()
                    onThreeFingerTap?.invoke()
                }
                threeFingerDownTime = 0L
                // Tap = select (short press, almost no movement).
                val isTap = System.currentTimeMillis() - downTime < 350 &&
                    abs(event.x - downX) < 24 && abs(event.y - downY) < 24
                if (isTap) {
                    when {
                        mode == Mode.EDIT && elementMode > 0 ->
                            onElementTap?.invoke(ndcX(event.x), ndcY(event.y))
                        mode != Mode.SCULPT ->
                            onTapPick?.invoke(ndcX(event.x), ndcY(event.y))
                        else -> Unit
                    }
                }
                if (mode == Mode.SCULPT) onBrush?.invoke(ndcX(event.x), ndcY(event.y), 2)
                if (mode == Mode.EDIT && elementMode > 0) onElementEnd?.invoke()
                lastPinch = 0f
            }
            MotionEvent.ACTION_POINTER_UP -> {
                lastPinch = 0f
                if (mode == Mode.SCULPT && event.pointerCount == 2) {
                    // Lifting one of two fingers ends nothing; brush resumes on next 1-finger move.
                }
            }
            MotionEvent.ACTION_CANCEL -> {
                if (mode == Mode.SCULPT) onBrush?.invoke(0f, 0f, 2)
                lastPinch = 0f
            }
        }
        return true
    }

    private fun ndcX(x: Float) = if (width > 0) 2f * x / width - 1f else 0f
    private fun ndcY(y: Float) = if (height > 0) 1f - 2f * y / height else 0f

    private fun pinchDistance(event: MotionEvent): Float {
        val dx = event.getX(0) - event.getX(1)
        val dy = event.getY(0) - event.getY(1)
        return kotlin.math.sqrt(dx * dx + dy * dy).takeIf { abs(it) > 0f } ?: 0f
    }

    companion object {
        private const val TAG = "Mobile3D"
    }
}
