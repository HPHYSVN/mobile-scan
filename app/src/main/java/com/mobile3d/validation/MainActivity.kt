package com.mobile3d.validation

import android.app.Activity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import java.io.File

/**
 * M1 editor skeleton: viewport + primitive add + select + numeric transform
 * (sliders) + project save/open. Deliberately uses framework views - Compose
 * is a separate, later decision. Not product UI.
 */
class MainActivity : Activity() {

    private lateinit var viewport: ViewportView
    private lateinit var statsView: TextView
    private lateinit var nodeLabel: TextView
    private lateinit var seekX: SeekBar
    private lateinit var seekRotY: SeekBar
    private lateinit var seekScale: SeekBar

    private lateinit var modeLabel: TextView
    private var nodes: List<ViewportView.NodeInfo> = emptyList()

    // Transform undo history (UI-level, capped). Native core/undo covers
    // vertex snapshots; transform edits are small value objects kept here.
    private data class TransformEdit(val nodeId: Long, val before: FloatArray, val after: FloatArray)
    private val undoStack = ArrayDeque<TransformEdit>()
    private val redoStack = ArrayDeque<TransformEdit>()
    private var transformBefore: FloatArray? = null
    private var selectedId: Long = -1
    private var syncingSliders = false

    private val handler = Handler(Looper.getMainLooper())
    private val statsTick = object : Runnable {
        override fun run() {
            val s = viewport.frameStats()
            statsView.text = "frames=${s[0].toLong()} avgMs=%.2f nodes=${nodes.size}".format(s[1])
            handler.postDelayed(this, 1000)
        }
    }

    private val projectDir: File get() = File(filesDir, "current_project")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        viewport = ViewportView(this)
        statsView = TextView(this).apply { setTextColor(0xFFFFFFFF.toInt()); setPadding(16, 6, 16, 6); textSize = 12f }
        nodeLabel = TextView(this).apply { setTextColor(0xFFFFFFFF.toInt()); setPadding(16, 6, 16, 6); textSize = 13f; text = "no node" }

        fun slider(max: Int, progress: Int) = SeekBar(this).apply {
            this.max = max
            this.progress = progress
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(bar: SeekBar?, value: Int, fromUser: Boolean) {
                    if (fromUser && !syncingSliders) applyTransformFromSliders()
                }
                override fun onStartTrackingTouch(bar: SeekBar?) {
                    transformBefore = nodes.firstOrNull { it.id == selectedId }?.transform?.copyOf()
                }
                override fun onStopTrackingTouch(bar: SeekBar?) {
                    val before = transformBefore ?: return
                    val after = nodes.firstOrNull { it.id == selectedId }?.transform?.copyOf() ?: return
                    if (!before.contentEquals(after)) {
                        undoStack.addLast(TransformEdit(selectedId, before, after))
                        if (undoStack.size > 50) undoStack.removeFirst()
                        redoStack.clear()
                    }
                    transformBefore = null
                }
            })
        }
        seekX = slider(40, 20)
        seekRotY = slider(360, 0)
        seekScale = slider(28, 8)

        fun button(label: String, action: () -> Unit) = Button(this).apply {
            text = label
            setOnClickListener { action() }
        }
        fun row(vararg views: android.view.View) = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            views.forEach { addView(it, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)) }
        }

        modeLabel = TextView(this).apply {
            setTextColor(0xFFBFE3FF.toInt()); setPadding(16, 6, 16, 6); textSize = 12f
            text = "VIEW: 1 ngón xoay • 2 ngón zoom/pan • chạm 2 lần chọn vật"
        }
        viewport.onEditDrag = { dx, dy ->
            val wpp = viewport.worldPerPixel()
            mutateSelected { tr -> tr[0] += dx * wpp; tr[1] -= dy * wpp }
        }
        viewport.onEditScale = { factor ->
            mutateSelected { tr ->
                val s = (tr[6] * factor).coerceIn(0.05f, 20f)
                tr[6] = s; tr[7] = s; tr[8] = s
            }
        }
        viewport.onEditRotate = { delta ->
            mutateSelected { tr -> tr[4] -= delta }
        }
        viewport.onEditMoveY = { dyPx ->
            val wpp = viewport.worldPerPixel()
            mutateSelected { tr -> tr[1] += dyPx * wpp }
        }
        viewport.onDuplicateRequest = { duplicateSelected() }
        viewport.onTapPick = { x, y -> Log.i(TAG, "tap pick ndc=$x,$y (raycast in Sculpt slice)") }

        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF14181D.toInt())
            addView(statsView)
            addView(row(
                button("View") { setMode(ViewportView.Mode.VIEW) },
                button("Edit") { setMode(ViewportView.Mode.EDIT) },
                button("Sculpt") { setMode(ViewportView.Mode.SCULPT) },
                button("Reset Cam") { viewport.resetCamera() },
            ))
            addView(modeLabel)
            addView(row(
                button("Cube") { addPrimitive("cube") },
                button("Sphere") { addPrimitive("sphere") },
                button("Cylinder") { addPrimitive("cylinder") },
                button("Plane") { addPrimitive("plane") },
            ))
            addView(row(
                button("Import fixture") { importFixture() },
                button("Prev") { cycleSelection(-1) },
                button("Next") { cycleSelection(1) },
                button("Duplicate") { duplicateSelected() },
                button("Delete") { deleteSelected() },
            ))
            addView(nodeLabel)
            addView(row(TextView(this@MainActivity).apply { text = "PosX"; setTextColor(0xFFFFFFFF.toInt()) }, seekX))
            addView(row(TextView(this@MainActivity).apply { text = "RotY"; setTextColor(0xFFFFFFFF.toInt()) }, seekRotY))
            addView(row(TextView(this@MainActivity).apply { text = "Scale"; setTextColor(0xFFFFFFFF.toInt()) }, seekScale))
            addView(row(
                button("Undo") { undoTransform() },
                button("Redo") { redoTransform() },
                button("Export GLB") { exportSelected() },
            ))
            addView(row(
                *palette.map { (name, rgb) ->
                    Button(this@MainActivity).apply {
                        text = name
                        setBackgroundColor(rgb)
                        setOnClickListener { applyColor(rgb) }
                    }
                }.toTypedArray()
            ))
            addView(row(
                button("Save") { saveProject(manual = true) },
                button("Open") { openProject() },
                button("New") { newProject(withDefaultCube = true) },
            ))
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(viewport, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
            addView(panel)
        }
        setContentView(root)

        if (File(projectDir, "manifest.json").exists()) openProject() else newProject(withDefaultCube = true)
        benchmarkJni()
    }

    private fun newProject(withDefaultCube: Boolean) {
        viewport.newProject(projectDir.absolutePath) {
            if (withDefaultCube) addPrimitive("cube") else refreshNodes()
        }
    }

    private fun setMode(mode: ViewportView.Mode) {
        viewport.mode = mode
        modeLabel.text = when (mode) {
            ViewportView.Mode.VIEW -> "VIEW: 1 ngón xoay • 2 ngón zoom/pan • 3 ngón vuốt dọc zoom"
            ViewportView.Mode.EDIT -> "EDIT: 1 ngón kéo di chuyển • chạm 2 lần nhân bản • 2 ngón pinch scale / xoay • 3 ngón vuốt dọc lên-xuống"
            ViewportView.Mode.SCULPT -> "SCULPT: 1 ngón vẽ brush • 2 ngón xoay/zoom camera"
        }
        Log.i(TAG, "mode=$mode")
    }

    private fun mutateSelected(change: (FloatArray) -> Unit) {
        val node = nodes.firstOrNull { it.id == selectedId } ?: return
        val tr = node.transform.copyOf()
        change(tr)
        viewport.setNodeTransform(selectedId, tr)
        System.arraycopy(tr, 0, node.transform, 0, 9)
        syncSliders(node)
        updateNodeLabel()
    }

    private fun duplicateSelected() {
        if (selectedId < 0) return
        viewport.duplicateNode(selectedId) { newId ->
            if (newId > 0) {
                selectedId = newId
                refreshNodes()
            }
        }
    }

    private fun addPrimitive(kind: String) {
        viewport.addPrimitive(kind) { id ->
            selectedId = id
            refreshNodes()
        }
    }

    private fun importFixture() {
        Thread {
            try {
                val meshesDir = File(projectDir, "meshes").apply { mkdirs() }
                val out = File(meshesDir, "scan.glb")
                if (!out.exists() || out.length() == 0L) {
                    assets.open("sphere_98k.glb").use { input -> out.outputStream().use { input.copyTo(it) } }
                }
                viewport.addNode("Scan fixture", "meshes/scan.glb") { id ->
                    selectedId = id
                    refreshNodes()
                }
            } catch (e: Exception) {
                Log.e(TAG, "import fixture failed", e)
            }
        }.start()
    }

    private fun deleteSelected() {
        if (selectedId < 0) return
        viewport.removeNode(selectedId) {
            selectedId = -1
            refreshNodes()
        }
    }

    private fun cycleSelection(direction: Int) {
        if (nodes.isEmpty()) return
        val index = nodes.indexOfFirst { it.id == selectedId }
        val next = nodes[(index + direction + nodes.size) % nodes.size]
        selectedId = next.id
        syncSliders(next)
        updateNodeLabel()
    }

    private fun applyTransformFromSliders() {
        val node = nodes.firstOrNull { it.id == selectedId } ?: return
        val t = node.transform.copyOf()
        t[0] = (seekX.progress - 20) / 10f          // position X -2..2
        t[4] = Math.toRadians(seekRotY.progress.toDouble()).toFloat()  // rotation Y
        val scale = (seekScale.progress + 2) / 10f  // 0.2..3.0
        t[6] = scale; t[7] = scale; t[8] = scale
        viewport.setNodeTransform(selectedId, t)
        node.transform.let { System.arraycopy(t, 0, it, 0, 9) }
        updateNodeLabel()
    }

    private fun syncSliders(node: ViewportView.NodeInfo) {
        syncingSliders = true
        val t = node.transform
        seekX.progress = (t[0] * 10).toInt() + 20
        seekRotY.progress = Math.toDegrees(t[4].toDouble()).toInt().mod(360)
        seekScale.progress = (t[6] * 10).toInt() - 2
        syncingSliders = false
    }

    private fun updateNodeLabel() {
        val node = nodes.firstOrNull { it.id == selectedId }
        nodeLabel.text = if (node == null) "no node selected (nodes=${nodes.size})"
        else "selected: ${node.name} #${node.id} (${node.meshRef}) x=%.1f rotY=%.0f scale=%.1f"
            .format(node.transform[0], Math.toDegrees(node.transform[4].toDouble()), node.transform[6])
    }

    private fun refreshNodes() {
        viewport.fetchNodes { list ->
            nodes = list
            if (nodes.none { it.id == selectedId }) selectedId = nodes.lastOrNull()?.id ?: -1
            nodes.firstOrNull { it.id == selectedId }?.let { syncSliders(it) }
            updateNodeLabel()
        }
    }

    private fun undoTransform() {
        val edit = undoStack.removeLastOrNull() ?: return
        redoStack.addLast(edit)
        viewport.setNodeTransform(edit.nodeId, edit.before)
        nodes.firstOrNull { it.id == edit.nodeId }?.let {
            System.arraycopy(edit.before, 0, it.transform, 0, 9)
            if (edit.nodeId == selectedId) syncSliders(it)
        }
        updateNodeLabel()
    }

    private fun redoTransform() {
        val edit = redoStack.removeLastOrNull() ?: return
        undoStack.addLast(edit)
        viewport.setNodeTransform(edit.nodeId, edit.after)
        nodes.firstOrNull { it.id == edit.nodeId }?.let {
            System.arraycopy(edit.after, 0, it.transform, 0, 9)
            if (edit.nodeId == selectedId) syncSliders(it)
        }
        updateNodeLabel()
    }

    private fun exportSelected() {
        if (selectedId < 0) return
        val dir = File(filesDir, "exports").apply { mkdirs() }
        val out = File(dir, "node_$selectedId.glb")
        viewport.exportNode(selectedId, out.absolutePath) { error ->
            Log.i(TAG, "exportNode: ${if (error.isEmpty()) out.absolutePath else error}")
            nodeLabel.text = if (error.isEmpty()) "exported: ${out.path}" else "export failed: $error"
        }
    }

    private val palette = listOf(
        "Blue" to 0xFF3F8CD2.toInt(), "Red" to 0xFFD23F3F.toInt(),
        "Green" to 0xFF3FD26A.toInt(), "Orange" to 0xFFD28A3F.toInt(),
        "Purple" to 0xFF8A3FD2.toInt(), "Gray" to 0xFF9A9A9A.toInt(),
        "White" to 0xFFF2F2F2.toInt(), "Black" to 0xFF222222.toInt(),
    )

    private fun applyColor(argb: Int) {
        if (selectedId < 0) return
        val r = android.graphics.Color.red(argb) / 255f
        val g = android.graphics.Color.green(argb) / 255f
        val b = android.graphics.Color.blue(argb) / 255f
        viewport.setNodeColor(selectedId, r, g, b)
        nodes.firstOrNull { it.id == selectedId }?.color?.let {
            it[0] = r; it[1] = g; it[2] = b
        }
        nodeLabel.text = "color updated"
    }

    private fun saveProject(manual: Boolean) {
        viewport.saveProject { error ->
            Log.i(TAG, "saveProject: ${if (error.isEmpty()) "ok" else error}")
            if (manual) nodeLabel.text = if (error.isEmpty()) "saved to ${projectDir.name}" else "save failed: $error"
        }
    }

    private fun openProject() {
        viewport.openProject(projectDir.absolutePath) { error ->
            Log.i(TAG, "openProject: ${if (error.isEmpty()) "ok" else error}")
            if (error.isEmpty()) refreshNodes() else newProject(withDefaultCube = true)
        }
    }

    // D9 kept: raw JNI round-trip overhead (comparative only, see PERFORMANCE.md).
    private fun benchmarkJni() {
        Thread {
            val probe = NativeEngine.nativeCreate()
            val calls = 10_000
            repeat(1_000) { NativeEngine.nativePing(probe) }
            val start = System.nanoTime()
            var sink = 0L
            repeat(calls) { sink += NativeEngine.nativePing(probe) }
            val elapsedNs = System.nanoTime() - start
            NativeEngine.nativeDestroy(probe)
            Log.i(TAG, "jni ping: $calls calls (%.2f us/call, sink=$sink)".format(elapsedNs / 1_000.0 / calls))
        }.start()
    }

    override fun onResume() {
        super.onResume()
        viewport.onResume()
        handler.postDelayed(statsTick, 1000)
    }

    override fun onPause() {
        handler.removeCallbacks(statsTick)
        saveProject(manual = false)  // autosave
        viewport.onPause()
        super.onPause()
    }

    override fun onDestroy() {
        viewport.release()
        super.onDestroy()
    }

    companion object {
        private const val TAG = "Mobile3D"
    }
}
