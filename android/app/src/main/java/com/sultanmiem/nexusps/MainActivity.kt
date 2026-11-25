package com.sultanmiem.nexusps

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {
    private lateinit var status: TextView
    private lateinit var biosInfo: TextView
    private lateinit var discInfo: TextView

    private val pickBios = 10
    private val pickDisc = 11

    private external fun nativeCoreVersion(): String

    companion object {
        init {
            System.loadLibrary("nexus_ps1_android")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(28, 48, 28, 28)
        }

        val title = TextView(this).apply {
            text = "NEXUS-PS"
            textSize = 30f
            gravity = Gravity.CENTER
        }
        val subtitle = TextView(this).apply {
            text = "PlayStation 1 • Android"
            textSize = 16f
            gravity = Gravity.CENTER
        }
        status = TextView(this).apply {
            text = "Native core: " + nativeCoreVersion()
            textSize = 14f
            setPadding(0, 28, 0, 20)
        }
        biosInfo = TextView(this).apply { text = "BIOS: not selected" }
        discInfo = TextView(this).apply { text = "Disc: not selected" }

        val biosButton = Button(this).apply {
            text = "Choose BIOS"
            setOnClickListener { openPicker(pickBios, "application/octet-stream") }
        }
        val discButton = Button(this).apply {
            text = "Choose game (CUE or BIN)"
            setOnClickListener { openPicker(pickDisc, "*/*") }
        }

        val notice = TextView(this).apply {
            text = "Development build. File selection is ready, but game loading, GPU display, audio and touch controls are not connected yet."
            textSize = 14f
            setPadding(0, 28, 0, 0)
        }

        listOf(title, subtitle, status, biosButton, biosInfo, discButton, discInfo, notice)
            .forEach { root.addView(it) }

        setContentView(root)
    }

    private fun openPicker(requestCode: Int, mime: String) {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = mime
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, requestCode)
    }

    @Deprecated("Use Activity Result APIs in a later UI pass")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK) return
        val uri: Uri = data?.data ?: return
        contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        val name = uri.lastPathSegment ?: uri.toString()
        when (requestCode) {
            pickBios -> biosInfo.text = "BIOS selected: $name"
            pickDisc -> discInfo.text = "Disc file selected: $name"
        }
    }
}
