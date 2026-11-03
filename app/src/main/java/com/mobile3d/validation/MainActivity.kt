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
    private lateinit var handleOverlay: HandleOverlayView
    private lateinit var statsView: TextView
    private lateinit var nodeLabel: TextView
    private lateinit var brushButton: Button
    private lateinit var exportFormatButton: Button
    private lateinit var exportSubdivButton: Button
    private var exportFormat = 0  // 0=GLB, 1=OBJ triangles, 2=OBJ quads
    private var exportResolution = 100
    private var exportSubdiv = 0
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
            val showHandles = viewport.mode == ViewportView.Mode.EDIT && selectedId > 0
            handleOverlay.enabledHandles = showHandles
            if (showHandles) {
                viewport.getNodeScreenBox(selectedId) { ndc -> handleOverlay.setBoxNdc(ndc) }
            } else {
                handleOverlay.setBoxNdc(null)
            }
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

        exportFormatButton = Button(this).apply {
            text = "Format: GLB"
            setOnClickListener {
                exportFormat = (exportFormat + 1) % 3
                text = "Format: " + listOf("GLB", "OBJ Tri", "OBJ Quad")[exportFormat]
            }
        }
        exportSubdivButton = Button(this).apply {
            text = "Subdiv: 0"
            setOnClickListener {
                exportSubdiv = (exportSubdiv + 1) % 3
                text = "Subdiv: $exportSubdiv"
            }
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
        viewport.onTapPick = { x, y ->
            viewport.pickNode(x, y) { id ->
                if (id > 0) {
                    selectedId = id
                    nodes.firstOrNull { it.id == id }?.let { syncSliders(it) }
                    updateNodeLabel()
                }
            }
        }
        viewport.onBrush = { x, y, phase ->
            if (selectedId >= 0) {
                viewport.brushStroke(selectedId, x, y, phase) { affected ->
                    if (phase == 0) Log.i(TAG, "brush start affected=$affected")
                }
            }
        }

        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF14181D.toInt())
            addView(statsView)
            addView(row(
                button("View") { setMode(ViewportView.Mode.VIEW) },
                button("Edit") { setMode(ViewportView.Mode.EDIT) },
                button("Sculpt") { setMode(ViewportView.Mode.SCULPT) },
                button("Reset Cam") { viewport.resetCamera() },
                button("Help") { showHelp(firstRun = false) },
            ))
            addView(modeLabel)
            addView(row(
                button("Cube") { addPrimitive("cube") },
                button("Sphere") { addPrimitive("sphere") },
                button("Cylinder") { addPrimitive("cylinder") },
                button("Plane") { addPrimitive("plane") },
            ))
            addView(row(
                button("Import File") { pickFile() },
                button("Export Scene") { exportScene() },
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
            brushButton = button(brushModeLabel()) { cycleBrushMode() }
            addView(row(
                brushButton,
                button("Sculpt Undo") { viewport.undoSculpt { refreshNodes() } },
                button("Sculpt Redo") { viewport.redoSculpt { refreshNodes() } },
            ))
            addView(row(TextView(this@MainActivity).apply { text = "Brush R"; setTextColor(0xFFFFFFFF.toInt()) },
                SeekBar(this@MainActivity).apply {
                    max = 90; progress = 25
                    setOnSeekBarChangeListener(simpleSeek { applyBrushSettings(radius = (it + 10) / 100f) })
                }))
            addView(row(TextView(this@MainActivity).apply { text = "Brush S"; setTextColor(0xFFFFFFFF.toInt()) },
                SeekBar(this@MainActivity).apply {
                    max = 99; progress = 19
                    setOnSeekBarChangeListener(simpleSeek { applyBrushSettings(strength = (it + 1) / 500f) })
                }))
            addView(row(
                button("Undo") { undoTransform() },
                button("Redo") { redoTransform() },
                exportFormatButton,
                exportSubdivButton,
            ))
            addView(row(TextView(this@MainActivity).apply { text = "Export %"; setTextColor(0xFFFFFFFF.toInt()) },
                SeekBar(this@MainActivity).apply {
                    max = 90; progress = 90
                    setOnSeekBarChangeListener(simpleSeek { exportResolution = it + 10 })
                },
                button("Export") { exportSelected() },
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

        handleOverlay = HandleOverlayView(this)
        handleOverlay.onMove = { dx, dy ->
            val wpp = viewport.worldPerPixel()
            mutateSelected { tr -> tr[0] += dx * wpp; tr[1] -= dy * wpp }
        }
        handleOverlay.onCornerScale = { factor ->
            mutateSelected { tr ->
                val s = (tr[6] * factor).coerceIn(0.05f, 20f)
                tr[6] = s; tr[7] = s; tr[8] = s
            }
        }
        val viewportFrame = android.widget.FrameLayout(this).apply {
            addView(viewport)
            addView(handleOverlay)
        }
        val panelScroll = android.widget.ScrollView(this).apply {
            addView(panel)
        }
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(viewportFrame, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
            // Panel is capped and scrollable so the viewport keeps most of the screen.
            addView(panelScroll, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 950))
        }
        setContentView(root)

        if (File(projectDir, "manifest.json").exists()) openProject() else newProject(withDefaultCube = true)
        benchmarkJni()
        applyBrushSettings()
        val prefs = getSharedPreferences("mobile3d", MODE_PRIVATE)
        if (!prefs.getBoolean("help_seen", false)) {
            showHelp(firstRun = true)
            prefs.edit().putBoolean("help_seen", true).apply()
        }
    }

    private fun showHelp(firstRun: Boolean) {
        val intro = if (firstRun) "Lan dau su dung - doc 30 giay. " else ""
        val text = intro + """
            MOBILE 3D STUDIO - HUONG DAN NHANH

            CHE DO (hang tren cung):
            - View: 1 ngon xoay, 2 ngon zoom/pan, 3 ngon vuot doc de zoom. Cham vao vat de chon.
            - Edit: keo 1 ngon de di chuyen, keo goc (o vuong xanh) de scale, 2 ngon pinch scale/xoay,
              3 ngon vuot doc de nang-ha, cham 2 lan de nhan ban.
            - Sculpt: 1 ngon ve brush, 2 ngon xoay camera. Brush Draw/Dig/Smooth/Inflate,
              Brush R/S la ban kinh/luc, Sculpt Undo/Redo de hoan tac.

            VAT: Cube/Sphere/Cylinder/Plane, Import fixture, Prev/Next, Duplicate, Delete.
            MAU: chon vat roi bam mau o hang mau.
            TRANSFORM: PosX/RotY/Scale, Undo/Redo cho transform.

            XUAT FILE (Export):
            - Format GLB (triangle, cho Unity/Unreal/Blender), OBJ Tri, OBJ Quad
              (polygon gop tu giac dong phang, tien sua tren Blender).
            - Export % = do phan giai (giam % = giam polygon). Subdiv 0-2 chia nho be mat.
            - File nam trong thu muc exports cua ung dung.

            DU AN: Save/Open/New, tu dong luu khi thoat app.
            Du lieu: current_project (manifest.json + scene.json + meshes/).
            Meo: Reset Cam neu lac huong. Edit truoc, Sculpt sau.
        """.trimIndent()
        android.app.AlertDialog.Builder(this)
            .setTitle("Huong dan su dung")
            .setMessage(text)
            .setPositiveButton("Da hieu") { dialog, _ -> dialog.dismiss() }
            .show()
    }

    private var brushMode = 0
    private var brushRadius = 0.35f
    private var brushStrength = 0.04f
    private val brushModes = listOf("Draw", "Dig", "Smooth", "Inflate")

    private fun brushModeLabel() = "Brush: ${brushModes[brushMode]}"

    private fun cycleBrushMode() {
        brushMode = (brushMode + 1) % brushModes.size
        brushButton.text = brushModeLabel()
        applyBrushSettings()
        nodeLabel.text = "brush mode: ${brushModes[brushMode]} (Sculpt, drag on model)"
    }

    private fun applyBrushSettings(radius: Float = brushRadius, strength: Float = brushStrength) {
        brushRadius = radius
        brushStrength = strength
        viewport.setBrush(brushRadius, brushStrength, brushMode)
    }

    private fun simpleSeek(onChange: (Int) -> Unit) = object : SeekBar.OnSeekBarChangeListener {
        override fun onProgressChanged(bar: SeekBar?, value: Int, fromUser: Boolean) {
            if (fromUser) onChange(value)
        }
        override fun onStartTrackingTouch(bar: SeekBar?) {}
        override fun onStopTrackingTouch(bar: SeekBar?) {}
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

    private fun newProject(withDefaultCube: Boolean) {
        viewport.newProject(projectDir.absolutePath) {
            if (withDefaultCube) addPrimitive("cube") else refreshNodes()
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

    private fun pickFile() {
        // Framework startActivityForResult: avoids an AndroidX Activity
        // dependency for this single picker (see DEPENDENCY_AUDIT).
        @Suppress("DEPRECATION")
        startActivityForResult(
            android.content.Intent(android.content.Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(android.content.Intent.CATEGORY_OPENABLE)
                type = "*/*"
            },
            REQUEST_PICK_FILE,
        )
    }

    @Deprecated("Framework picker")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: android.content.Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_PICK_FILE && resultCode == RESULT_OK) {
            data?.data?.let { importPickedFile(it) }
        }
    }

    private fun importPickedFile(uri: android.net.Uri) {
        Thread {
            try {
                val name = uri.lastPathSegment?.substringAfterLast('/') ?: "imported"
                val safe = name.replace(Regex("[^A-Za-z0-9._-]"), "_")
                val meshesDir = File(projectDir, "meshes").apply { mkdirs() }
                val out = File(meshesDir, safe)
                contentResolver.openInputStream(uri)?.use { input ->
                    out.outputStream().use { input.copyTo(it) }
                } ?: throw IllegalStateException("cannot open $uri")
                runOnUiThread {
                    viewport.addNode(safe.substringBeforeLast('.'), "meshes/$safe") { id ->
                        selectedId = id
                        refreshNodes()
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "import file failed", e)
                runOnUiThread { nodeLabel.text = "import failed: ${e.message}" }
            }
        }.start()
    }

    private fun exportScene() {
        val dir = File(filesDir, "exports").apply { mkdirs() }
        val out = File(dir, "scene.glb")
        viewport.exportScene(out.absolutePath) { error ->
            Log.i(TAG, "exportScene: ${if (error.isEmpty()) out.absolutePath else error}")
            nodeLabel.text = if (error.isEmpty()) "scene exported: ${out.name}" else "scene export failed: $error"
        }
    }

    private fun exportSelected() {
        if (selectedId < 0) return
        val dir = File(filesDir, "exports").apply { mkdirs() }
        val ext = if (exportFormat == 0) "glb" else "obj"
        val out = File(dir, "node_${selectedId}_f${exportFormat}_r${exportResolution}_s$exportSubdiv.$ext")
        viewport.exportNodeEx(selectedId, out.absolutePath, exportFormat, exportResolution, exportSubdiv) { error ->
            Log.i(TAG, "exportNodeEx: ${if (error.isEmpty()) out.absolutePath else error}")
            nodeLabel.text = if (error.isEmpty()) "exported: ${out.name}" else "export failed: $error"
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
            if (error.isEmpty()) refreshNodes()
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
        private const val REQUEST_PICK_FILE = 4101
    }
}
