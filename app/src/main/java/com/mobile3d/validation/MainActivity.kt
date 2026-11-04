package com.mobile3d.validation

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import java.io.File

/**
 * Editor screen (redesigned after mobile 3D modelling apps):
 * - top-left hamburger = project/options menu (one button, expands)
 * - top-center "?" = guide
 * - top-right = undo / redo
 * - bottom bar = 4 category buttons (Create / Edit / Sculpt / Export);
 *   tapping one expands its panel above the bar, tapping again collapses.
 * The viewport fills the whole screen behind these overlays.
 */
class MainActivity : Activity() {

    private lateinit var viewport: ViewportView
    private lateinit var handleOverlay: HandleOverlayView
    private lateinit var selectedChip: TextView
    private lateinit var statsLabel: TextView
    private lateinit var brushButton: Button
    private lateinit var displayButton: Button
    private lateinit var gridButton: Button
    private lateinit var exportFormatButton: Button
    private lateinit var exportSubdivButton: Button
    private lateinit var toolLabel: TextView

    private lateinit var seekX: SeekBar
    private lateinit var seekRotY: SeekBar
    private lateinit var seekScale: SeekBar

    private var nodes: List<ViewportView.NodeInfo> = emptyList()
    private var selectedId: Long = -1
    private var syncingSliders = false

    private data class TransformEdit(val nodeId: Long, val before: FloatArray, val after: FloatArray)
    private val undoStack = ArrayDeque<TransformEdit>()
    private val redoStack = ArrayDeque<TransformEdit>()
    private var transformBefore: FloatArray? = null

    private var brushMode = 0
    private var brushRadius = 0.35f
    private var brushStrength = 0.04f
    private val brushModes = listOf("Draw", "Dig", "Smooth", "Inflate")

    private var displayMode = 2  // 0 solid, 1 wireframe, 2 solid+edges
    private var showGrid = true
    private var exportFormat = 0  // 0 GLB, 1 OBJ tri, 2 OBJ quad
    private var exportResolution = 100
    private var exportSubdiv = 0

    private var openCategory: String? = null
    private val panels = mutableMapOf<String, LinearLayout>()
    private val categoryButtons = mutableMapOf<String, Button>()

    private val handler = Handler(Looper.getMainLooper())
    private val statsTick = object : Runnable {
        override fun run() {
            val s = viewport.frameStats()
            statsLabel.text = "frames=${s[0].toLong()} avgMs=%.2f nodes=${nodes.size}".format(s[1])
            val showHandles = viewport.mode == ViewportView.Mode.EDIT && viewport.elementMode == 0 && selectedId > 0
            handleOverlay.enabledHandles = showHandles
            if (showHandles) {
                viewport.getNodeScreenBox(selectedId) { ndc -> handleOverlay.setBoxNdc(ndc) }
            } else {
                handleOverlay.setBoxNdc(null)
            }
            handler.postDelayed(this, 1000)
        }
    }

    val projectDir: File by lazy {
        intent.getStringExtra("projectDir")?.let { File(it) }
            ?: File(filesDir, "projects/default")
    }
    private val projectName: String by lazy {
        intent.getStringExtra("projectName") ?: projectDir.name
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        viewport = ViewportView(this)
        handleOverlay = HandleOverlayView(this)
        wireGestures()

        statsLabel = label("", 11f)
        toolLabel = label("Tool: Object", 12f)
        selectedChip = TextView(this).apply {
            setTextColor(0xFFFFFFFF.toInt())
            setBackgroundColor(0xB314181D.toInt())
            setPadding(18, 8, 18, 8)
            textSize = 12f
            text = "Chưa chọn vật"
        }

        seekX = transformSlider(40, 20)
        seekRotY = transformSlider(360, 0)
        seekScale = transformSlider(28, 8)

        brushButton = Button(this).apply {
            text = "Brush: Draw"
            setOnClickListener { cycleBrushMode() }
        }
        displayButton = Button(this).apply {
            text = "Display: Edges"
            setOnClickListener {
                displayMode = (displayMode + 1) % 3
                text = "Display: " + listOf("Solid", "Wireframe", "Edges")[displayMode]
                viewport.setDisplayMode(displayMode)
            }
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

        val viewportFrame = FrameLayout(this).apply {
            addView(viewport)
            addView(handleOverlay)
        }

        val root = FrameLayout(this).apply { addView(viewportFrame) }

        // --- top bar: hamburger | ? | undo redo ---
        val topBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(8, 8, 8, 8)
            addView(topButton("☰") { showMenu() })
            addView(spacer())
            addView(topButton("?") { showHelp(firstRun = false) })
            addView(spacer())
            addView(topButton("↩") { undoTransform() })
            addView(topButton("↪") { redoTransform() })
        }
        root.addView(topBar, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT))

        // --- bottom: selected chip, expandable panel, 4 category buttons ---
        val bottom = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(selectedChip)
        }
        panels["create"] = categoryPanel {
            addView(rowOf(
                button("Cube") { addPrimitive("cube") },
                button("Sphere") { addPrimitive("sphere") },
                button("Cylinder") { addPrimitive("cylinder") },
                button("Plane") { addPrimitive("plane") },
            ))
            addView(rowOf(
                button("Import File") { pickFile() },
                button("Import Fixture") { importFixture() },
                button("Scan") { startScan() },
            ))
        }
        panels["edit"] = categoryPanel {
            addView(rowOf(
                button("Object") { setTool(0) },
                button("Vertex") { setTool(1) },
                button("Edge") { setTool(2) },
                button("Face") { setTool(3) },
            ))
            addView(toolLabel)
            addView(rowOf(label("PosX", 12f), seekX))
            addView(rowOf(label("RotY", 12f), seekRotY))
            addView(rowOf(label("Scale", 12f), seekScale))
            addView(rowOf(
                button("Prev") { cycleSelection(-1) },
                button("Next") { cycleSelection(1) },
                button("Duplicate") { duplicateSelected() },
                button("Delete") { deleteSelected() },
            ))
            gridButton = Button(this@MainActivity).apply {
                text = "Grid: On"
                setOnClickListener { toggleGrid(gridButton) }
            }
            addView(rowOf(displayButton, gridButton))
            addView(rowOf(
                *palette.map { (name, rgb) ->
                    Button(this@MainActivity).apply {
                        text = name
                        setBackgroundColor(rgb)
                        setOnClickListener { applyColor(rgb) }
                    }
                }.toTypedArray()
            ))
            addView(statsLabel)
        }
        panels["sculpt"] = categoryPanel {
            addView(rowOf(
                brushButton,
                button("Sculpt Undo") { viewport.undoSculpt { } },
                button("Sculpt Redo") { viewport.redoSculpt { } },
            ))
            addView(rowOf(label("Brush R", 12f), SeekBar(this@MainActivity).apply {
                max = 90; progress = 25
                setOnSeekBarChangeListener(simpleSeek { applyBrushSettings(radius = (it + 10) / 100f) })
            }))
            addView(rowOf(label("Brush S", 12f), SeekBar(this@MainActivity).apply {
                max = 99; progress = 19
                setOnSeekBarChangeListener(simpleSeek { applyBrushSettings(strength = (it + 1) / 500f) })
            }))
            addView(label("Chạm để chọn mặt, kéo 1 ngón để vẽ. 2 ngón: camera.", 11f))
        }
        panels["export"] = categoryPanel {
            addView(rowOf(exportFormatButton, exportSubdivButton, button("Export") { exportSelected() }))
            addView(rowOf(label("Export %", 12f), SeekBar(this@MainActivity).apply {
                max = 90; progress = 90
                setOnSeekBarChangeListener(simpleSeek { exportResolution = it + 10 })
            }))
            addView(rowOf(
                button("Export Scene (.glb)") { exportScene() },
            ))
        }
        panels.values.forEach {
            it.visibility = LinearLayout.GONE
            bottom.addView(it)
        }

        val bottomBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(0xE614181D.toInt())
            for ((key, title) in listOf("create" to "Create", "edit" to "Edit", "sculpt" to "Sculpt", "export" to "Export")) {
                val b = Button(this@MainActivity).apply {
                    text = title
                    setOnClickListener { toggleCategory(key) }
                }
                categoryButtons[key] = b
                addView(b, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            }
        }
        bottom.addView(bottomBar)
        root.addView(bottom, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT,
            android.view.Gravity.BOTTOM,
        ))

        setContentView(root)

        if (File(projectDir, "manifest.json").exists()) openProject() else newProject(withDefaultCube = true)
        benchmarkJni()
        applyBrushSettings()
        viewport.setDisplayMode(displayMode)

        val prefs = getSharedPreferences("mobile3d", MODE_PRIVATE)
        if (!prefs.getBoolean("help_seen", false)) {
            showHelp(firstRun = true)
            prefs.edit().putBoolean("help_seen", true).apply()
        }
    }

    // ---------- UI helpers ----------
    private fun label(text: String, size: Float) = TextView(this).apply {
        this.text = text
        setTextColor(0xFFD8E6F2.toInt())
        setPadding(14, 6, 14, 6)
        textSize = size
    }

    private fun spacer() = TextView(this).apply {
        layoutParams = LinearLayout.LayoutParams(0, 1, 1f)
    }

    private fun topButton(text: String, action: () -> Unit) = Button(this).apply {
        this.text = text
        textSize = 16f
        setBackgroundColor(0xC614181D.toInt())
        setTextColor(0xFFFFFFFF.toInt())
        setOnClickListener { action() }
    }

    private fun button(labelText: String, action: () -> Unit) = Button(this).apply {
        text = labelText
        setOnClickListener { action() }
    }

    private fun rowOf(vararg views: android.view.View) = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL
        views.forEach { addView(it, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)) }
    }

    private fun categoryPanel(build: LinearLayout.() -> Unit) = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(0xF214181D.toInt())
        setPadding(8, 8, 8, 8)
        build()
    }

    private fun toggleCategory(key: String) {
        val opening = openCategory != key
        openCategory = if (opening) key else null
        panels.forEach { (k, panel) -> panel.visibility = if (k == openCategory) LinearLayout.VISIBLE else LinearLayout.GONE }
        categoryButtons.forEach { (k, b) ->
            b.setBackgroundColor(if (k == openCategory) 0xFF2E7D32.toInt() else 0x00000000)
        }
        viewport.mode = when (openCategory) {
            "edit" -> ViewportView.Mode.EDIT
            "sculpt" -> ViewportView.Mode.SCULPT
            else -> ViewportView.Mode.VIEW
        }
    }

    private fun toggleGrid(button: Button) {
        showGrid = !showGrid
        button.text = if (showGrid) "Grid: On" else "Grid: Off"
        viewport.setShowGrid(showGrid)
    }

    private fun setTool(elementMode: Int) {
        viewport.elementMode = elementMode
        viewport.mode = ViewportView.Mode.EDIT
        toolLabel.text = "Tool: " + listOf(
            "Object — kéo để di chuyển vật, kéo góc xanh để scale",
            "Vertex — chạm chọn đỉnh, kéo để dời đỉnh",
            "Edge — chạm chọn cạnh, kéo để dời cạnh",
            "Face — chạm chọn mặt, kéo để dời mặt",
        )[elementMode]
    }

    private fun showMenu() {
        val items = arrayOf(
            "Lưu dự án (Save)", "Mở lại dự án (Open)", "Dự án mới (New)",
            "Danh sách dự án", "Đặt lại camera", "Chụp thumbnail ngay",
        )
        AlertDialog.Builder(this)
            .setTitle("$projectName — Tuỳ chọn")
            .setItems(items) { _, which ->
                when (which) {
                    0 -> saveProject(manual = true)
                    1 -> openProject()
                    2 -> confirmNewProject()
                    3 -> finish()
                    4 -> viewport.resetCamera()
                    5 -> captureThumbnail()
                }
            }
            .setNegativeButton("Đóng", null)
            .show()
    }

    private fun confirmNewProject() {
        AlertDialog.Builder(this)
            .setTitle("Dự án mới?")
            .setMessage("Cảnh hiện tại sẽ bị xoá khỏi màn hình (bản đã lưu vẫn còn cho tới khi bạn Lưu đè).")
            .setPositiveButton("Tạo mới") { _, _ -> newProject(withDefaultCube = true) }
            .setNegativeButton("Huỷ", null)
            .show()
    }

    // ---------- gestures ----------
    private fun wireGestures() {
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
        viewport.onEditDrag = { dx, dy ->
            val wpp = viewport.worldPerPixel()
            mutateSelected { tr -> tr[0] += dx * wpp; tr[1] -= dy * wpp }
        }
        viewport.onEditMoveY = { dyPx ->
            val wpp = viewport.worldPerPixel()
            if (viewport.elementMode > 0) viewport.moveElement(0f, -dyPx)
            else mutateSelected { tr -> tr[1] += dyPx * wpp }
        }
        viewport.onDuplicateRequest = { duplicateSelected() }
        viewport.onTapPick = { x, y ->
            viewport.pickNode(x, y) { id ->
                if (id > 0) {
                    selectedId = id
                    nodes.firstOrNull { it.id == id }?.let { syncSliders(it) }
                    updateSelectedChip()
                }
            }
        }
        viewport.onElementTap = { x, y ->
            if (selectedId >= 0) {
                viewport.pickElement(selectedId, viewport.elementMode, x, y) { count ->
                    toolLabel.text = if (count > 0) "Đã chọn $count đỉnh của phần tử — kéo để dời"
                    else "Không trúng mô hình — chạm lại gần bề mặt"
                }
            }
        }
        viewport.onElementDrag = { dx, dy -> viewport.moveElement(dx, dy) }
        viewport.onElementEnd = { viewport.endElementEdit() }
        viewport.onBrush = { x, y, phase ->
            if (selectedId >= 0) {
                viewport.brushStroke(selectedId, x, y, phase) { affected ->
                    if (phase == 0) Log.i(TAG, "brush start affected=$affected")
                }
            }
        }
    }

    // ---------- transform / selection ----------
    private fun transformSlider(max: Int, progress: Int) = SeekBar(this).apply {
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

    private fun applyTransformFromSliders() {
        val node = nodes.firstOrNull { it.id == selectedId } ?: return
        val t = node.transform.copyOf()
        t[0] = (seekX.progress - 20) / 10f
        t[4] = Math.toRadians(seekRotY.progress.toDouble()).toFloat()
        val scale = (seekScale.progress + 2) / 10f
        t[6] = scale; t[7] = scale; t[8] = scale
        viewport.setNodeTransform(selectedId, t)
        System.arraycopy(t, 0, node.transform, 0, 9)
        updateSelectedChip()
    }

    private fun syncSliders(node: ViewportView.NodeInfo) {
        syncingSliders = true
        val t = node.transform
        seekX.progress = (t[0] * 10).toInt() + 20
        seekRotY.progress = Math.toDegrees(t[4].toDouble()).toInt().mod(360)
        seekScale.progress = (t[6] * 10).toInt() - 2
        syncingSliders = false
    }

    private fun mutateSelected(change: (FloatArray) -> Unit) {
        val node = nodes.firstOrNull { it.id == selectedId } ?: return
        val tr = node.transform.copyOf()
        change(tr)
        viewport.setNodeTransform(selectedId, tr)
        System.arraycopy(tr, 0, node.transform, 0, 9)
        syncSliders(node)
        updateSelectedChip()
    }

    private fun updateSelectedChip() {
        val node = nodes.firstOrNull { it.id == selectedId }
        selectedChip.text = if (node == null) "Chưa chọn vật — chạm vào vật để chọn"
        else "${node.name} #${node.id} • x=%.1f rotY=%.0f scale=%.1f".format(
            node.transform[0], Math.toDegrees(node.transform[4].toDouble()), node.transform[6])
    }

    private fun refreshNodes(selectLast: Boolean = false) {
        viewport.fetchNodes { list ->
            nodes = list
            if (selectLast) selectedId = nodes.lastOrNull()?.id ?: -1
            if (nodes.none { it.id == selectedId }) selectedId = nodes.lastOrNull()?.id ?: -1
            nodes.firstOrNull { it.id == selectedId }?.let { syncSliders(it) }
            updateSelectedChip()
        }
    }

    private fun cycleSelection(direction: Int) {
        if (nodes.isEmpty()) return
        val index = nodes.indexOfFirst { it.id == selectedId }
        val next = nodes[(index + direction + nodes.size) % nodes.size]
        selectedId = next.id
        syncSliders(next)
        updateSelectedChip()
    }

    private fun undoTransform() {
        val edit = undoStack.removeLastOrNull() ?: return
        redoStack.addLast(edit)
        viewport.setNodeTransform(edit.nodeId, edit.before)
        nodes.firstOrNull { it.id == edit.nodeId }?.let {
            System.arraycopy(edit.before, 0, it.transform, 0, 9)
            if (edit.nodeId == selectedId) syncSliders(it)
        }
        updateSelectedChip()
    }

    private fun redoTransform() {
        val edit = redoStack.removeLastOrNull() ?: return
        undoStack.addLast(edit)
        viewport.setNodeTransform(edit.nodeId, edit.after)
        nodes.firstOrNull { it.id == edit.nodeId }?.let {
            System.arraycopy(edit.after, 0, it.transform, 0, 9)
            if (edit.nodeId == selectedId) syncSliders(it)
        }
        updateSelectedChip()
    }

    // ---------- scene operations ----------
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

    private fun duplicateSelected() {
        if (selectedId < 0) return
        viewport.duplicateNode(selectedId) { newId ->
            if (newId > 0) {
                selectedId = newId
                refreshNodes()
            }
        }
    }

    private fun deleteSelected() {
        if (selectedId < 0) return
        viewport.removeNode(selectedId) {
            selectedId = -1
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

    private fun checkPendingScanResult() {
        val marker = File(projectDir, "pending_scan_result.txt")
        if (!marker.exists()) return
        val meshRef = marker.readText().trim()
        if (meshRef.isEmpty()) {
            marker.delete()
            return
        }
        viewport.fetchNodes { list ->
            nodes = list
            if (list.none { it.meshRef.contains("scan_result") }) {
                viewport.addNode("Scan result", meshRef) { id ->
                    selectedId = id
                    refreshNodes()
                }
            }
            marker.delete()
        }
    }

    private fun startScan() {
        @Suppress("DEPRECATION")
        startActivityForResult(
            Intent(this, ScanActivity::class.java).apply {
                putExtra("projectDir", projectDir.absolutePath)
            },
            REQUEST_SCAN,
        )
    }

    private fun pickFile() {
        @Suppress("DEPRECATION")
        startActivityForResult(
            Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
            },
            REQUEST_PICK_FILE,
        )
    }

    @Deprecated("Framework picker")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_PICK_FILE && resultCode == RESULT_OK) {
            data?.data?.let { importPickedFile(it) }
        }
        if (requestCode == REQUEST_SCAN && resultCode == RESULT_OK) {
            val mesh = data?.getStringExtra("resultMesh") ?: return
            viewport.addNode("Scan result", mesh) { id ->
                selectedId = id
                refreshNodes()
                toggleCategory("edit")
            }
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
                runOnUiThread { selectedChip.text = "Import lỗi: ${e.message}" }
            }
        }.start()
    }

    // ---------- brush ----------
    private fun cycleBrushMode() {
        brushMode = (brushMode + 1) % brushModes.size
        brushButton.text = "Brush: ${brushModes[brushMode]}"
        applyBrushSettings()
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

    // ---------- material ----------
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
        selectedChip.text = "Đã đổi màu vật #${selectedId}"
    }

    // ---------- project ----------
    private fun saveProject(manual: Boolean) {
        viewport.saveProjectWithThumbnail(File(projectDir, "thumbnail.png").absolutePath) { error ->
            Log.i(TAG, "saveProject: ${if (error.isEmpty()) "ok" else error}")
            if (error.isEmpty() || error == "thumbnail failed") {
                writeProjectInfo()
                refreshNodes()
            }
            if (manual) {
                selectedChip.text = when {
                    error.isEmpty() -> "Đã lưu dự án"
                    error == "thumbnail failed" -> "Đã lưu dự án (thumbnail lỗi)"
                    else -> "Lưu lỗi: $error"
                }
            }
        }
    }

    private fun openProject() {
        viewport.openProject(projectDir.absolutePath) { error ->
            Log.i(TAG, "openProject: ${if (error.isEmpty()) "ok" else error}")
            if (error.isEmpty()) refreshNodes() else newProject(withDefaultCube = true)
        }
    }

    private fun writeProjectInfo() {
        try {
            val info = org.json.JSONObject()
                .put("name", projectName)
                .put("modified", System.currentTimeMillis())
                .put("nodes", nodes.size)
            File(projectDir, "project_info.json").writeText(info.toString())
        } catch (e: Exception) {
            Log.e(TAG, "write project info failed", e)
        }
    }

    private fun captureThumbnail() {
        // Native glReadPixels on the GL thread: deterministic even while the
        // activity is pausing, unlike PixelCopy which raced surface teardown.
        viewport.captureThumbnail(File(projectDir, "thumbnail.png").absolutePath) { ok ->
            Log.i(TAG, "thumbnail capture: $ok")
        }
    }


    // ---------- export ----------
    private fun exportSelected() {
        if (selectedId < 0) return
        val dir = File(filesDir, "exports").apply { mkdirs() }
        val ext = if (exportFormat == 0) "glb" else "obj"
        val out = File(dir, "node_${selectedId}_f${exportFormat}_r${exportResolution}_s$exportSubdiv.$ext")
        viewport.exportNodeEx(selectedId, out.absolutePath, exportFormat, exportResolution, exportSubdiv) { error ->
            Log.i(TAG, "exportNodeEx: ${if (error.isEmpty()) out.absolutePath else error}")
            selectedChip.text = if (error.isEmpty()) "Đã xuất: ${out.name}" else "Xuất lỗi: $error"
        }
    }

    private fun exportScene() {
        val dir = File(filesDir, "exports").apply { mkdirs() }
        val out = File(dir, "scene.glb")
        viewport.exportScene(out.absolutePath) { error ->
            Log.i(TAG, "exportScene: ${if (error.isEmpty()) out.absolutePath else error}")
            selectedChip.text = if (error.isEmpty()) "Đã xuất scene.glb" else "Xuất scene lỗi: $error"
        }
    }

    // ---------- help ----------
    private fun showHelp(firstRun: Boolean) {
        val intro = if (firstRun) "Lần đầu sử dụng — đọc 30 giây. " else ""
        val text = intro + """
            HƯỚNG DẪN NHANH

            THANH TRÊN: ☰ = menu dự án/tuỳ chọn • ? = hướng dẫn này • ↩/↪ = Undo/Redo transform.
            4 NÚT DƯỚI: Create (thêm vật) • Edit (sửa) • Sculpt (điêu khắc) • Export (xuất file).
            Bấm 1 nút để bung chức năng của nó, bấm lại để thu gọn.

            THAO TÁC CHẠM (mọi chế độ):
            • Chạm vào vật = chọn vật. Chạm khoảng trống = bỏ chọn.
            • Kéo 1 ngón ở khoảng trống = xoay camera.
            • 2 ngón = camera: pinch zoom, kéo để pan.
            • 3 ngón chạm nhanh = đặt lại camera. 3 ngón vuốt dọc (Edit) = nâng/hạ vật.

            EDIT — 4 công cụ:
            • Object: kéo để di chuyển vật; kéo góc ô vuông xanh để scale; hoặc dùng PosX/RotY/Scale.
            • Vertex / Edge / Face: chạm lên bề mặt để chọn đỉnh/cạnh/mặt (điểm cam là phần đã chọn),
              sau đó kéo để dời phần đó. Sculpt Undo cũng hoàn tác được sửa phần tử.
            • Display: Solid / Wireframe / Edges (hiện lưới tam giác). Grid: lưới mặt phẳng toạ độ gốc 0,0,0
              (trục X đỏ, Y xanh lá, Z xanh dương).

            SCULPT: chạm chọn mặt, kéo 1 ngón để vẽ brush Draw/Dig/Smooth/Inflate.
            CREATE: thêm khối, Import File (.glb/.obj), Import Fixture, Capture chụp ảnh vào source/.
            EXPORT: GLB / OBJ Tri / OBJ Quad (polygon), Export % là độ phân giải, Subdiv chia nhỏ bề mặt,
            Export Scene xuất cả cảnh thành 1 GLB.

            Dự án tự lưu khi thoát editor. Màn hình chính là danh sách dự án (nút ☰ → Danh sách dự án để quay lại).
        """.trimIndent()
        AlertDialog.Builder(this)
            .setTitle("Hướng dẫn sử dụng")
            .setMessage(text)
            .setPositiveButton("Đã hiểu") { dialog, _ -> dialog.dismiss() }
            .show()
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
        checkPendingScanResult()
        viewport.onResume()
        handler.postDelayed(statsTick, 1000)
    }

    override fun onPause() {
        handler.removeCallbacks(statsTick)
        saveProject(manual = false)  // autosave + thumbnail + info
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
        private const val REQUEST_SCAN = 4103
    }
}
