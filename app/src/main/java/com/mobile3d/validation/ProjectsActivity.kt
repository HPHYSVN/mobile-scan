package com.mobile3d.validation

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.graphics.BitmapFactory
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Home screen: project list with thumbnail cards + add-project button. */
class ProjectsActivity : Activity() {

    private lateinit var listContainer: LinearLayout

    data class ProjectEntry(
        val dir: File,
        val name: String,
        val modified: Long,
        val nodeCount: Int,
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val title = TextView(this).apply {
            text = "Dự án 3D của tôi"
            textSize = 20f
            setTextColor(0xFFFFFFFF.toInt())
            setPadding(24, 64, 24, 8)
        }
        val addButton = Button(this).apply {
            text = "+  Dự án mới"
            textSize = 16f
            setOnClickListener { createProject() }
        }
        listContainer = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF10151B.toInt())
            addView(title)
            addView(addButton, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                setMargins(24, 8, 24, 16)
            })
            addView(ScrollView(this@ProjectsActivity).apply { addView(listContainer) },
                LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        }
        root.applySafeAreaInsets { insetTop, insetBottom, insetLeft, insetRight ->
            root.setPadding(insetLeft, insetTop, insetRight, insetBottom)
        }
        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        refreshList()
    }

    private fun projectsRoot(): File = File(filesDir, "projects").apply { mkdirs() }

    private fun loadEntries(): List<ProjectEntry> {
        val entries = mutableListOf<ProjectEntry>()
        val dirs = mutableListOf<File>()
        projectsRoot().listFiles { f -> f.isDirectory }?.let { dirs += it }
        // Legacy single-project location from before the project list existed.
        File(filesDir, "current_project").takeIf { it.isDirectory }?.let { dirs += it }

        for (dir in dirs) {
            var name = dir.name
            var nodes = -1
            var modified = dir.lastModified()
            try {
                val infoFile = File(dir, "project_info.json")
                if (infoFile.exists()) {
                    val info = JSONObject(infoFile.readText())
                    name = info.optString("name", name)
                    nodes = info.optInt("nodes", -1)
                    modified = info.optLong("modified", modified)
                }
            } catch (_: Exception) {
                // Corrupt info must not hide the project itself.
            }
            entries += ProjectEntry(dir, name, modified, nodes)
        }
        return entries.sortedByDescending { it.modified }
    }

    private fun refreshList() {
        listContainer.removeAllViews()
        val entries = loadEntries()
        if (entries.isEmpty()) {
            listContainer.addView(TextView(this).apply {
                text = "Chưa có dự án nào.\nBấm “+ Dự án mới” để bắt đầu tạo mô hình 3D."
                setTextColor(0xFFB9C9D8.toInt())
                textSize = 15f
                setPadding(24, 32, 24, 32)
            })
            return
        }
        val dateFormat = SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.getDefault())
        for (entry in entries) {
            listContainer.addView(projectCard(entry, dateFormat))
        }
    }

    private fun projectCard(entry: ProjectEntry, dateFormat: SimpleDateFormat): LinearLayout {
        val thumbFile = File(entry.dir, "thumbnail.png").takeIf { it.exists() } ?: File(entry.dir, "thumbnail.jpg")
        val thumbnail = ImageView(this).apply {
            if (thumbFile.exists()) {
                setImageBitmap(BitmapFactory.decodeFile(thumbFile.absolutePath))
            } else {
                setBackgroundColor(0xFF223140.toInt())
            }
            scaleType = ImageView.ScaleType.CENTER_CROP
        }
        val meta = TextView(this).apply {
            val nodesText = if (entry.nodeCount >= 0) "${entry.nodeCount} vật thể • " else ""
            text = "${entry.name}\n$nodesText${dateFormat.format(Date(entry.modified))}"
            setTextColor(0xFFFFFFFF.toInt())
            textSize = 15f
            setPadding(18, 10, 10, 10)
        }
        val deleteButton = Button(this).apply {
            text = "Xoá"
            setOnClickListener { confirmDelete(entry) }
        }
        return LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setBackgroundColor(0xFF1B242F.toInt())
            setPadding(10, 10, 10, 10)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT,
            ).apply { setMargins(24, 10, 24, 10) }
            addView(thumbnail, LinearLayout.LayoutParams(300, 200))
            addView(meta, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(deleteButton)
            setOnClickListener { openProject(entry) }
            setOnLongClickListener {
                renameProject(entry)
                true
            }
        }
    }

    private fun openProject(entry: ProjectEntry) {
        startActivity(Intent(this, MainActivity::class.java).apply {
            putExtra("projectDir", entry.dir.absolutePath)
            putExtra("projectName", entry.name)
        })
    }

    private fun createProject() {
        val stamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
        val dir = File(projectsRoot(), "project_$stamp")
        dir.mkdirs()
        val name = "Dự án ${SimpleDateFormat("dd/MM HH:mm", Locale.getDefault()).format(Date())}"
        startActivity(Intent(this, MainActivity::class.java).apply {
            putExtra("projectDir", dir.absolutePath)
            putExtra("projectName", name)
        })
    }

    private fun renameProject(entry: ProjectEntry) {
        val input = EditText(this).apply { setText(entry.name) }
        AlertDialog.Builder(this)
            .setTitle("Đổi tên dự án")
            .setView(input)
            .setPositiveButton("Lưu") { _, _ ->
                val newName = input.text.toString().trim()
                if (newName.isNotEmpty()) {
                    try {
                        val infoFile = File(entry.dir, "project_info.json")
                        val info = if (infoFile.exists()) JSONObject(infoFile.readText()) else JSONObject()
                        info.put("name", newName)
                        info.put("modified", System.currentTimeMillis())
                        infoFile.writeText(info.toString())
                    } catch (_: Exception) {
                    }
                    refreshList()
                }
            }
            .setNegativeButton("Huỷ", null)
            .show()
    }

    private fun confirmDelete(entry: ProjectEntry) {
        AlertDialog.Builder(this)
            .setTitle("Xoá dự án?")
            .setMessage("“${entry.name}” và toàn bộ dữ liệu trong thư mục dự án sẽ bị xoá. Không hoàn tác được.")
            .setPositiveButton("Xoá") { _, _ ->
                entry.dir.deleteRecursively()
                refreshList()
            }
            .setNegativeButton("Huỷ", null)
            .show()
    }
}
