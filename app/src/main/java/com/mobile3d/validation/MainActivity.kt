package com.mobile3d.validation

import android.app.Activity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.FrameLayout
import android.widget.TextView

/** M0 validation only: Kotlin -> JNI -> C++ -> GLES 3.0 viewport. No product UI. */
class MainActivity : Activity() {

    private lateinit var viewport: ViewportView
    private lateinit var statsView: TextView
    private val handler = Handler(Looper.getMainLooper())
    private val statsTick = object : Runnable {
        override fun run() {
            val s = viewport.frameStats()
            val text = "frames=${s[0].toLong()} avgMs=%.2f verts=${s[3].toLong()} tris=${s[4].toLong()}\n".format(s[1]) +
                "deform=%.2fms normals=%.2fms upload=%.2fms region=${s[8].toLong()}v".format(s[5], s[6], s[7])
            statsView.text = text
            Log.i(TAG, text)
            handler.postDelayed(this, 1000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        viewport = ViewportView(this)
        statsView = TextView(this).apply {
            setTextColor(0xFFFFFFFF.toInt())
            setBackgroundColor(0x66000000)
            setPadding(24, 12, 24, 12)
            textSize = 13f
        }
        val root = FrameLayout(this)
        root.addView(viewport)
        root.addView(statsView)
        setContentView(root)
        loadFixture()
    }

    private fun loadFixture() {
        // D3/D4 spike: bundled fixture, copied to filesDir because native reads a path.
        Thread {
            try {
                val name = "sphere_98k.glb"
                val out = java.io.File(filesDir, name)
                if (!out.exists() || out.length() == 0L) {
                    assets.open(name).use { input -> out.outputStream().use { input.copyTo(it) } }
                }
                viewport.loadMesh(out.absolutePath) { ms ->
                    Log.i(TAG, "fixture load: $ms ms")
                    if (ms >= 0) viewport.setDeformEnabled(true)
                }
                benchmarkJni()
            } catch (e: Exception) {
                Log.e(TAG, "fixture copy failed", e)
            }
        }.start()
    }

    // D9: raw JNI round-trip overhead. Runs off the UI thread; result is only
    // meaningful comparatively (emulator x86 here, not an ARM device).
    private fun benchmarkJni() {
        Thread {
            val handleProbe = NativeEngine.nativeCreate()
            val calls = 10_000
            repeat(1_000) { NativeEngine.nativePing(handleProbe) }  // warm up JIT/lookup
            val start = System.nanoTime()
            var sink = 0L
            repeat(calls) { sink += NativeEngine.nativePing(handleProbe) }
            val elapsedNs = System.nanoTime() - start
            NativeEngine.nativeDestroy(handleProbe)
            Log.i(TAG, "jni ping: $calls calls in ${elapsedNs / 1_000_000} ms " +
                "(%.2f us/call, sink=$sink)".format(elapsedNs / 1_000.0 / calls))
        }.start()
    }

    override fun onResume() {
        super.onResume()
        viewport.onResume()
        handler.postDelayed(statsTick, 1000)
    }

    override fun onPause() {
        handler.removeCallbacks(statsTick)
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
