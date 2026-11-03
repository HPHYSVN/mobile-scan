package com.mobile3d.validation

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.LinearLayout
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
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Capture session (M3 slice): guided photo capture into the current
 * project's source/ folder. Reconstruction is NOT done here - photos are
 * the input for a future reconstruction step (see Help text in MainActivity).
 * ARCore is only probed for availability; no AR session is started, and the
 * UI says so instead of pretending otherwise.
 */
class CaptureActivity : ComponentActivity() {

    private lateinit var statusLabel: TextView
    private lateinit var previewView: PreviewView
    private var imageCapture: ImageCapture? = null
    private var photoCount = 0

    private val sourceDir: File by lazy {
        File(filesDir, "current_project/source").apply { mkdirs() }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        statusLabel = TextView(this).apply {
            setTextColor(0xFFFFFFFF.toInt())
            setPadding(16, 10, 16, 10)
            textSize = 13f
        }
        previewView = PreviewView(this)
        val captureButton = Button(this).apply { text = "Capture photo" }
        val doneButton = Button(this).apply { text = "Done" }
        captureButton.setOnClickListener { takePhoto() }
        doneButton.setOnClickListener { finish() }

        val buttons = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(captureButton, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(doneButton, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        }
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF14181D.toInt())
            addView(statusLabel)
            addView(previewView, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
            addView(buttons)
        }
        setContentView(root)

        photoCount = sourceDir.listFiles { f -> f.extension == "jpg" }?.size ?: 0
        updateStatus(arNote = probeArCore())

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            startCamera()
        } else {
            @Suppress("DEPRECATION")
            requestPermissions(arrayOf(Manifest.permission.CAMERA), REQUEST_CAMERA)
        }
    }

    private fun probeArCore(): String {
        return try {
            when (ArCoreApk.getInstance().checkAvailability(this)) {
                ArCoreApk.Availability.SUPPORTED_INSTALLED -> "ARCore: supported (session not started in this build)"
                ArCoreApk.Availability.SUPPORTED_NOT_INSTALLED,
                ArCoreApk.Availability.SUPPORTED_APK_TOO_OLD -> "ARCore: supported, needs install/update"
                ArCoreApk.Availability.UNSUPPORTED_DEVICE_NOT_CAPABLE -> "ARCore: not supported on this device"
                else -> "ARCore: unknown/transient"
            }
        } catch (e: Exception) {
            "ARCore: check failed (${e.javaClass.simpleName})"
        }
    }

    private fun updateStatus(arNote: String) {
        statusLabel.text = "Da chup: $photoCount anh -> current_project/source/\n" +
            "Chup quanh vat the, moi anh chong lap ~70% voi anh truoc, anh sang/net.\n$arNote"
    }

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
                provider.bindToLifecycle(
                    this, CameraSelector.DEFAULT_BACK_CAMERA, preview, imageCapture,
                )
                Log.i(TAG, "camera started")
            } catch (e: Exception) {
                Log.e(TAG, "camera start failed", e)
                statusLabel.text = "Camera loi: ${e.message}"
            }
        }, ContextCompat.getMainExecutor(this))
    }

    private fun takePhoto() {
        val capture = imageCapture ?: return
        val stamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
        val file = File(sourceDir, "capture_$stamp.jpg")
        val options = ImageCapture.OutputFileOptions.Builder(file).build()
        capture.takePicture(options, ContextCompat.getMainExecutor(this),
            object : ImageCapture.OnImageSavedCallback {
                override fun onImageSaved(output: ImageCapture.OutputFileResults) {
                    photoCount++
                    Log.i(TAG, "photo saved: ${file.name} (${file.length()} bytes)")
                    updateStatus(arNote = probeArCore())
                }

                override fun onError(exception: ImageCaptureException) {
                    Log.e(TAG, "photo failed", exception)
                    statusLabel.text = "Chup loi: ${exception.message}"
                }
            })
    }

    @Deprecated("Permission callback")
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_CAMERA && grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
            startCamera()
        } else if (requestCode == REQUEST_CAMERA) {
            statusLabel.text = "Chua co quyen camera. Cap quyen trong Settings de chup."
        }
    }

    companion object {
        private const val TAG = "Mobile3D"
        private const val REQUEST_CAMERA = 4102
    }
}
