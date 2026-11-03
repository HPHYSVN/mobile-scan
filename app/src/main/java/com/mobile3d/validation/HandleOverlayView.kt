package com.mobile3d.validation

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.sqrt

/**
 * Edit-mode handles: draws the selected node's screen box (NDC from native)
 * with four corner handles. Dragging a corner scales, dragging inside moves.
 * Only intercepts touches in EDIT mode when a box is present; otherwise
 * touches fall through to the viewport.
 */
class HandleOverlayView(context: Context) : View(context) {

    // Box in view pixels: left, top, right, bottom.
    private var box: FloatArray? = null
    private var draggingCorner = false
    private var draggingInside = false
    private var lastX = 0f
    private var lastY = 0f
    private var lastCornerDist = 0f

    var enabledHandles: Boolean = false
    var onMove: ((dxPx: Float, dyPx: Float) -> Unit)? = null
    var onCornerScale: ((factor: Float) -> Unit)? = null

    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF6EC1FF.toInt()
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF6EC1FF.toInt()
        style = Paint.Style.FILL
    }

    fun setBoxNdc(ndc: FloatArray?) {
        box = if (ndc == null || ndc.size < 4 || width == 0 || height == 0) null else floatArrayOf(
            (ndc[0] + 1f) / 2f * width,
            (1f - ndc[3]) / 2f * height,
            (ndc[2] + 1f) / 2f * width,
            (1f - ndc[1]) / 2f * height,
        )
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (!enabledHandles) return
        val b = box ?: return
        canvas.drawRect(b[0], b[1], b[2], b[3], stroke)
        val r = 14f
        for ((cx, cy) in corners(b)) canvas.drawCircle(cx, cy, r, fill)
    }

    private fun corners(b: FloatArray): List<Pair<Float, Float>> = listOf(
        b[0] to b[1], b[2] to b[1], b[0] to b[3], b[2] to b[3],
    )

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!enabledHandles) return false
        val b = box ?: return false
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                val hitRadius = 48f
                draggingCorner = corners(b).any { (cx, cy) -> abs(event.x - cx) < hitRadius && abs(event.y - cy) < hitRadius }
                draggingInside = !draggingCorner &&
                    event.x in b[0]..b[2] && event.y in b[1]..b[3]
                if (!draggingCorner && !draggingInside) return false
                lastX = event.x
                lastY = event.y
                lastCornerDist = distToCenter(b, event.x, event.y)
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (draggingCorner) {
                    val d = distToCenter(b, event.x, event.y)
                    if (lastCornerDist > 1f && d > 1f) onCornerScale?.invoke(d / lastCornerDist)
                    lastCornerDist = d
                } else if (draggingInside) {
                    onMove?.invoke(event.x - lastX, event.y - lastY)
                }
                lastX = event.x
                lastY = event.y
                return draggingCorner || draggingInside
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                draggingCorner = false
                draggingInside = false
            }
        }
        return draggingCorner || draggingInside
    }

    private fun distToCenter(b: FloatArray, x: Float, y: Float): Float {
        val cx = (b[0] + b[2]) / 2
        val cy = (b[1] + b[3]) / 2
        return sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy))
    }
}
