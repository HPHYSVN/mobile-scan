package com.mobile3d.validation

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageCapture
import androidx.camera.core.ImageCaptureException
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.core.content.ContextCompat
import com.google.ar.core.ArCoreApk
import com.google.ar.core.Config
import com.google.ar.core.Session
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Scan screen (clean single-purpose layout):
 *  1. Camera preview + capture into the current project's source/ folder.
 *  2. Capability card: ARCore, Depth API, ToF/depth camera (Android has no
 *     Apple-style LiDAR; Depth API + ToF are the equivalents and are probed
 *     for real, not assumed).
 *  3. Reconstruction: uploads photos to the scan server (URL configurable),
 *     polls the job, downloads result.glb into meshes/ and returns it to the
 *     editor. Uploads are resumable: the server is idempotent per filename
 *     and already-present files are skipped.
 */
class ScanActivity : ComponentActivity() {

    private lateinit var statusLabel: TextView
    private lateinit var capabilityLabel: TextView
    private lateinit var stageLabel: TextView
    private lateinit var progressBar: ProgressBar
    private lateinit var serverUrlField: EditText
    private lateinit var reconstructButton: Button
    private lateinit var previewView: PreviewView
    private var imageCapture: ImageCapture? = null
    private var photoCount = 0
    @Volatile private var working = false

    private val projectDir: File by lazy {
        intent.getStringExtra("projectDir")?.let { File(it) }
            ?: File(filesDir, "projects/default")
    }
    private val sourceDir: File by lazy { File(projectDir, "source").apply { mkdirs() } }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        statusLabel = textView(13f)
        capabilityLabel = textView(12f)
        stageLabel = textView(12f)
        progressBar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            visibility = View.GONE
        }
        previewView = PreviewView(this)
        serverUrlField = EditText(this).apply {
            setText(getSharedPreferences("scan", MODE_PRIVATE).getString("server_url", "http://10.0.2.2:8000"))
            setTextColor(0xFFFFFFFF.toInt())
            hint = "http://<IP máy chủ>:8000"
            setHintTextColor(0xFF8AA0B4.toInt())
        }

        val captureButton = Button(this).apply {
            text = "Chụp ảnh"
            setOnClickListener { takePhoto() }
        }
        reconstructButton = Button(this).apply {
            text = "Tạo mô hình 3D từ ảnh"
            setOnClickListener { startReconstruction() }
        }
        val doneButton = Button(this).apply {
            text = "Xong"
            setOnClickListener { finish() }
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF10151B.toInt())
            setPadding(20, 20, 20, 20)
            addView(previewView, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 640))
            addView(captureButton)
            addView(statusLabel)
            addView(sectionTitle("Khả năng thiết bị"))
            addView(capabilityLabel)
            addView(sectionTitle("Tái tạo 3D (máy chủ)"))
            addView(serverUrlField)
            addView(reconstructButton)
            addView(progressBar)
            addView(stageLabel)
            addView(doneButton)
        }
        val scroll = ScrollView(this).apply { addView(root) }
        scroll.applySafeAreaInsets { insetTop, insetBottom, insetLeft, insetRight ->
            scroll.setPadding(insetLeft, insetTop, insetRight, insetBottom)
        }
        setContentView(scroll)

        photoCount = photoFiles().size
        updateStatus()
        capabilityLabel.text = probeCapabilities()

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            startCamera()
        } else {
            @Suppress("DEPRECATION")
            requestPermissions(arrayOf(Manifest.permission.CAMERA), REQUEST_CAMERA)
        }
    }

    private fun textView(size: Float) = TextView(this).apply {
        setTextColor(0xFFD8E6F2.toInt())
        setPadding(6, 8, 6, 8)
        textSize = size
    }

    private fun sectionTitle(text: String) = TextView(this).apply {
        this.text = text
        setTextColor(0xFFFFFFFF.toInt())
        textSize = 15f
        setPadding(6, 18, 6, 4)
    }

    private fun photoFiles(): List<File> =
        sourceDir.listFiles { f -> f.extension.lowercase() in listOf("jpg", "jpeg", "png") }
            ?.sortedBy { it.name } ?: emptyList()

    private fun updateStatus() {
        statusLabel.text = "Đã chụp: $photoCount ảnh\n" +
            "Chụp quanh vật thể, mỗi ảnh chồng lấp ~70%, đủ sáng và nét (tối thiểu 8 ảnh)."
    }

    // ---------- capabilities ----------
    private fun probeCapabilities(): String {
        val lines = mutableListOf<String>()

        val arAvailability = try {
            ArCoreApk.getInstance().checkAvailability(this)
        } catch (e: Exception) {
            null
        }
        lines += when (arAvailability) {
            ArCoreApk.Availability.SUPPORTED_INSTALLED -> "ARCore: hỗ trợ"
            ArCoreApk.Availability.SUPPORTED_NOT_INSTALLED,
            ArCoreApk.Availability.SUPPORTED_APK_TOO_OLD -> "ARCore: hỗ trợ, cần cài/cập nhật"
            ArCoreApk.Availability.UNSUPPORTED_DEVICE_NOT_CAPABLE -> "ARCore: không hỗ trợ"
            else -> "ARCore: chưa rõ (thiết bị ảo/không có Google Play Services for AR)"
        }

        // Depth API needs a real ARCore session; this throws on unsupported
        // devices, which is exactly the signal we report.
        var depthText = "Depth API: chưa kiểm tra được (cần ARCore)"
        if (arAvailability == ArCoreApk.Availability.SUPPORTED_INSTALLED) {
            depthText = try {
                val session = Session(this)
                val supported = session.isDepthModeSupported(Config.DepthMode.AUTOMATIC)
                session.close()
                if (supported) "Depth API: hỗ trợ (depth từ chuyển động/ToF)" else "Depth API: không hỗ trợ"
            } catch (e: Exception) {
                "Depth API: lỗi kiểm tra (${e.javaClass.simpleName})"
            }
        }
        lines += depthText

        // ToF / depth-output cameras (Android's LiDAR equivalent).
        var tofText = "Camera ToF/depth: không thấy"
        try {
            val manager = getSystemService(Context.CAMERA_SERVICE) as CameraManager
            for (id in manager.cameraIdList) {
                val caps = manager.getCameraCharacteristics(id)
                    .get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES) ?: continue
                if (caps.contains(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT)) {
                    tofText = "Camera ToF/depth: có (camera $id)"
                    break
                }
            }
        } catch (e: Exception) {
            tofText = "Camera ToF/depth: lỗi kiểm tra (${e.javaClass.simpleName})"
        }
        lines += tofText
        lines += "LiDAR (kiểu iPhone): Android không trang bị; dùng Depth API/ToF thay thế."
        return lines.joinToString("\n")
    }

    // ---------- camera ----------
    private fun startCamera() {
        val providerFuture = ProcessCameraProvider.getInstance(this)
        providerFuture.addListener({
            try {
                val provider = providerFuture.get()
                val preview = Preview.Builder().build().also {
                    it.surfaceProvider = previewView.surfaceProvider
                }
                imageCapture = ImageCapture.Builder()
                    .setCaptureMode(ImageCapture.CAPTURE_MODE_MINIMIZE_LATENCY)
                    .build()
                provider.unbindAll()
                provider.bindToLifecycle(this, CameraSelector.DEFAULT_BACK_CAMERA, preview, imageCapture)
            } catch (e: Exception) {
                Log.e(TAG, "camera start failed", e)
                statusLabel.text = "Camera lỗi: ${e.message}"
            }
        }, ContextCompat.getMainExecutor(this))
    }

    private fun takePhoto() {
        val capture = imageCapture ?: return
        val stamp = SimpleDateFormat("yyyyMMdd_HHmmss_SSS", Locale.US).format(Date())
        val file = File(sourceDir, "capture_$stamp.jpg")
        val options = ImageCapture.OutputFileOptions.Builder(file).build()
        capture.takePicture(options, ContextCompat.getMainExecutor(this),
            object : ImageCapture.OnImageSavedCallback {
                override fun onImageSaved(output: ImageCapture.OutputFileResults) {
                    photoCount++
                    Log.i(TAG, "photo saved: ${file.name} (${file.length()} bytes)")
                    updateStatus()
                }

                override fun onError(exception: ImageCaptureException) {
                    Log.e(TAG, "photo failed", exception)
                    statusLabel.text = "Chụp lỗi: ${exception.message}"
                }
            })
    }

    // ---------- reconstruction client ----------
    private fun serverBase(): String {
        val url = serverUrlField.text.toString().trim().trimEnd('/')
        getSharedPreferences("scan", MODE_PRIVATE).edit().putString("server_url", url).apply()
        return url
    }

    private fun startReconstruction() {
        val photos = photoFiles()
        if (photos.size < 8) {
            stageLabel.text = "Cần tối thiểu 8 ảnh (hiện có ${photos.size})."
            return
        }
        if (working) return
        working = true
        reconstructButton.isEnabled = false
        progressBar.visibility = View.VISIBLE
        progressBar.isIndeterminate = false
        Thread { runReconstruction(photos) }.start()
    }

    private fun setStage(text: String, progress: Int? = null) {
        runOnUiThread {
            stageLabel.text = text
            if (progress != null) progressBar.progress = progress
        }
    }

    private fun runReconstruction(photos: List<File>) {
        try {
            val base = serverBase()
            val sessionFile = File(projectDir, "server_session.txt")
            var sessionId = if (sessionFile.exists()) sessionFile.readText().trim() else ""
            if (sessionId.isEmpty() || request("GET", "$base/sessions/$sessionId") == null) {
                val created = request("POST", "$base/sessions", body = "")
                    ?: throw IllegalStateException("không tạo được phiên trên máy chủ")
                sessionId = JSONObject(created).getString("session_id")
                sessionFile.writeText(sessionId)
            }
            Log.i(TAG, "server session: $sessionId")

            // Resume: skip files the server already has with the same size.
            val existing = mutableMapOf<String, Long>()
            try {
                val filesJson = JSONObject(
                    request("GET", "$base/sessions/$sessionId/files") ?: "{}"
                ).optJSONArray("files")
                if (filesJson != null) {
                    for (i in 0 until filesJson.length()) {
                        val item = filesJson.getJSONObject(i)
                        existing[item.getString("name")] = item.getLong("bytes")
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "files list failed, uploading all: ${e.message}")
            }
            photos.forEachIndexed { index, file ->
                if (existing[file.name] == file.length()) {
                    setStage("Bỏ qua ảnh đã có ${index + 1}/${photos.size}…", (index + 1) * 50 / photos.size)
                } else {
                    uploadWithRetry("$base/sessions/$sessionId/images", file)
                    setStage("Đang tải ảnh ${index + 1}/${photos.size}…", (index + 1) * 50 / photos.size)
                }
            }

            request("POST", "$base/sessions/$sessionId/reconstruct", body = "")
                ?: throw IllegalStateException("không khởi động được tái tạo")

            var status = JSONObject()
            var pollFailures = 0
            while (true) {
                Thread.sleep(3000)
                try {
                    status = JSONObject(
                        request("GET", "$base/sessions/$sessionId")
                            ?: throw IllegalStateException("mất kết nối máy chủ")
                    )
                    pollFailures = 0
                } catch (e: Exception) {
                    if (++pollFailures >= 5) throw e
                    setStage("Mất kết nối tạm thời, đang thử lại ($pollFailures/5)…")
                    continue
                }
                val state = status.optString("state")
                val stage = status.optString("stage")
                val progressValue = (status.optDouble("progress", 0.0) * 50).toInt() + 50
                setStage("Máy chủ: $stage (${status.optDouble("progress", 0.0).times(100).toInt()}%)", progressValue)
                if (state == "done") break
                if (state == "error") throw IllegalStateException("Máy chủ lỗi: ${status.optString("error")}")
            }

            setStage("Đang tải mô hình về…", 98)
            val resultFile = File(File(projectDir, "meshes").apply { mkdirs() }, "scan_result.glb")
            download("$base/sessions/$sessionId/result", resultFile)
            Log.i(TAG, "result downloaded: ${resultFile.length()} bytes")
            // Marker so the editor imports the result even if it was
            // destroyed while this screen was in the foreground.
            File(projectDir, "pending_scan_result.txt").writeText("meshes/scan_result.glb")

            val info = status.getJSONObject("result")
            setStage(
                "Xong: ${info.optInt("faces")} mặt, ${info.optInt("registered_images")} ảnh đăng ký, " +
                    "${info.optDouble("seconds")}s. Quay lại editor để xem.",
                100,
            )
            runOnUiThread {
                setResult(RESULT_OK, android.content.Intent().apply {
                    putExtra("resultMesh", "meshes/scan_result.glb")
                })
                reconstructButton.isEnabled = true
                working = false
            }
        } catch (e: Exception) {
            Log.e(TAG, "reconstruction failed", e)
            setStage("Lỗi: ${e.message}")
            runOnUiThread {
                reconstructButton.isEnabled = true
                working = false
                progressBar.visibility = View.GONE
            }
        }
    }

    private fun request(method: String, url: String, body: String? = null): String? {
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = method
            connectTimeout = 10_000
            readTimeout = 30_000
            if (body != null) {
                doOutput = true
                outputStream.use { it.write(body.toByteArray()) }
            }
        }
        val code = connection.responseCode
        val text = try {
            (if (code in 200..299) connection.inputStream else connection.errorStream)
                ?.bufferedReader()?.readText()
        } catch (e: Exception) {
            null
        }
        connection.disconnect()
        if (code == 404) return null
        if (code !in 200..299) throw IllegalStateException("HTTP $code: ${text ?: ""}")
        return text
    }

    private fun uploadWithRetry(url: String, file: File) {
        var lastError: Exception? = null
        repeat(3) { attempt ->
            try {
                uploadMultipart(url, file)
                return
            } catch (e: Exception) {
                lastError = e
                Log.w(TAG, "upload ${file.name} attempt ${attempt + 1} failed: ${e.message}")
                Thread.sleep(1500)
            }
        }
        throw lastError ?: IllegalStateException("upload failed: ${file.name}")
    }

    private fun uploadMultipart(url: String, file: File) {
        val boundary = "----mobile3d${System.currentTimeMillis()}"
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            doOutput = true
            connectTimeout = 10_000
            readTimeout = 120_000
            setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
        }
        connection.outputStream.use { out ->
            out.write("--$boundary\r\n".toByteArray())
            out.write("Content-Disposition: form-data; name=\"file\"; filename=\"${file.name}\"\r\n".toByteArray())
            out.write("Content-Type: image/jpeg\r\n\r\n".toByteArray())
            file.inputStream().use { it.copyTo(out) }
            out.write("\r\n--$boundary--\r\n".toByteArray())
        }
        val code = connection.responseCode
        if (code !in 200..299) {
            val error = connection.errorStream?.bufferedReader()?.readText()
            connection.disconnect()
            throw IllegalStateException("tải ${file.name} lỗi HTTP $code: $error")
        }
        connection.disconnect()
    }

    private fun download(url: String, dest: File) {
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.connectTimeout = 10_000
        connection.readTimeout = 120_000
        if (connection.responseCode !in 200..299) {
            throw IllegalStateException("tải kết quả lỗi HTTP ${connection.responseCode}")
        }
        connection.inputStream.use { input ->
            dest.outputStream().use { input.copyTo(it) }
        }
        connection.disconnect()
    }

    @Deprecated("Permission callback")
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_CAMERA && grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
            startCamera()
        } else if (requestCode == REQUEST_CAMERA) {
            statusLabel.text = "Chưa có quyền camera. Cấp quyền trong Settings để chụp."
        }
    }

    companion object {
        private const val TAG = "Mobile3D"
        private const val REQUEST_CAMERA = 4102
    }
}
