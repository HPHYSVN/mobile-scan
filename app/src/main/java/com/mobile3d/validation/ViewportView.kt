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
