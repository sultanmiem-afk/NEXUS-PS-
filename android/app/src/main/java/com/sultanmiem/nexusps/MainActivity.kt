package com.sultanmiem.nexusps

import android.app.Activity
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.app.AlertDialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.ScrollView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.documentfile.provider.DocumentFile

class MainActivity : Activity() {
    private lateinit var status: TextView
    private lateinit var biosInfo: TextView
    private lateinit var discInfo: TextView
    private lateinit var folderInfo: TextView
    private val pickBios = 10
    private val pickDisc = 11
    private val pickFolder = 12
    private val importGames = 13
    private val prefs by lazy { getSharedPreferences("nexus_ps", MODE_PRIVATE) }
    private external fun nativeCoreVersion(): String
    private external fun nativeLoadBios(image: ByteArray): Boolean
    private external fun nativeBiosLoaded(): Boolean

    companion object {
        init { System.loadLibrary("nexus_ps1_android") }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(28, 48, 28, 28)
        }
        val title = TextView(this).apply { text = "NEXUS-PS"; textSize = 30f; gravity = Gravity.CENTER }
        val subtitle = TextView(this).apply { text = "PlayStation 1 • Android"; textSize = 16f; gravity = Gravity.CENTER }
        status = TextView(this).apply { text = "Native core: " + nativeCoreVersion(); textSize = 14f; setPadding(0, 28, 0, 20) }
        biosInfo = TextView(this).apply { text = if (prefs.getString("bios_uri", null) != null) "BIOS: previously validated (select again to recheck)" else "BIOS: not selected" }
        discInfo = TextView(this).apply { text = "Disc: not selected" }
        folderInfo = TextView(this).apply { text = "Games folder: not selected" }
        val biosButton = Button(this).apply { text = "Choose BIOS (any file type)"; setOnClickListener { explainFileAccess(pickBios, "*/*") } }
        val discButton = Button(this).apply { text = "Choose game (CUE or BIN)"; setOnClickListener { explainFileAccess(pickDisc, "*/*") } }
        val folderButton = Button(this).apply { text = "Create / choose games folder"; setOnClickListener { chooseGamesFolder() } }
        val importButton = Button(this).apply { text = "Copy games into NEXUS-PS folder"; setOnClickListener { chooseGamesToImport() } }
        val controllerTitle = TextView(this).apply {
            text = "Touch Controller"
            textSize = 22f
            gravity = Gravity.CENTER
            setPadding(0, 28, 0, 12)
        }
        val controllerStatus = TextView(this).apply {
            text = "Controller: ready • input preview only"
            textSize = 13f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 12)
        }
        val controller = createTouchController(controllerStatus)
        val notice = TextView(this).apply {
            text = "Development build. Touch buttons currently show press/release feedback; PS1 controller protocol, game loading, GPU display and audio are not connected yet. Import copies files; originals are kept."
            textSize = 14f
            setPadding(0, 24, 0, 20)
        }
        listOf(title, subtitle, status, biosButton, biosInfo, discButton, discInfo, folderButton, folderInfo, importButton, controllerTitle, controllerStatus, controller, notice).forEach { root.addView(it) }
        val scroll = ScrollView(this).apply { isFillViewport = true; addView(root) }
        setContentView(scroll)
        refreshFolderInfo()
        restoreSavedBios()
    }

    private fun createTouchController(statusLabel: TextView): View {
        fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

        fun button(label: String, accent: Int = Color.rgb(55, 65, 81), height: Int = 52): Button {
            return Button(this).apply {
                text = label
                textSize = if (label.length > 2) 11f else 17f
                isAllCaps = false
                setTextColor(Color.WHITE)
                background = GradientDrawable().apply {
                    shape = GradientDrawable.RECTANGLE
                    cornerRadius = dp(18).toFloat()
                    setColor(accent)
                    setStroke(dp(1), Color.argb(90, 255, 255, 255))
                    setGradientType(GradientDrawable.LINEAR_GRADIENT)
                }
                elevation = dp(5).toFloat()
                setOnTouchListener { view, event ->
                    when (event.actionMasked) {
                        MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                            view.alpha = 0.58f
                            view.scaleX = 0.94f
                            view.scaleY = 0.94f
                            statusLabel.text = "Pressed: $label • PSP-style layout preview"
                            true
                        }
                        MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                            view.alpha = 1f
                            view.scaleX = 1f
                            view.scaleY = 1f
                            statusLabel.text = "Controller: ready • input bridge not connected"
                            true
                        }
                        else -> true
                    }
                }
            }
        }

        fun buttonRow(vararg items: Pair<String, Int>): LinearLayout =
            LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER
                items.forEach { (label, color) ->
                    val b = button(label, color, 54)
                    addView(b, LinearLayout.LayoutParams(0, dp(54), 1f).apply {
                        setMargins(dp(3), dp(3), dp(3), dp(3))
                    })
                }
            }

        val dark = Color.rgb(24, 29, 39)
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(10), dp(12), dp(10), dp(12))
            background = GradientDrawable().apply {
                cornerRadius = dp(24).toFloat()
                setColor(dark)
                setStroke(dp(1), Color.rgb(75, 85, 99))
            }
        }

        panel.addView(buttonRow("L2" to Color.rgb(67, 76, 92), "L1" to Color.rgb(67, 76, 92),
            "R1" to Color.rgb(67, 76, 92), "R2" to Color.rgb(67, 76, 92)))

        val main = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, dp(10), 0, dp(10))
        }

        val dpad = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
        }
        dpad.addView(buttonRow(" " to dark, "↑" to Color.rgb(55, 65, 81), " " to dark))
        dpad.addView(buttonRow("←" to Color.rgb(55, 65, 81), "●" to Color.rgb(39, 47, 62), "→" to Color.rgb(55, 65, 81)))
        dpad.addView(buttonRow(" " to dark, "↓" to Color.rgb(55, 65, 81), " " to dark))
        main.addView(dpad, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))

        val face = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
        }
        face.addView(buttonRow("△" to Color.rgb(48, 105, 133), "○" to Color.rgb(143, 55, 67)))
        face.addView(buttonRow("×" to Color.rgb(48, 105, 133), "□" to Color.rgb(143, 55, 67)))
        main.addView(face, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        panel.addView(main)

        val centerControls = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        centerControls.addView(button("SELECT", Color.rgb(72, 78, 91), 42),
            LinearLayout.LayoutParams(0, dp(42), 1f).apply { setMargins(dp(28), 0, dp(8), 0) })
        centerControls.addView(button("START", Color.rgb(72, 78, 91), 42),
            LinearLayout.LayoutParams(0, dp(42), 1f).apply { setMargins(dp(8), 0, dp(28), 0) })
        panel.addView(centerControls)
        return panel
    }

    private fun loadBiosIntoCore(uri: Uri): Boolean {
        val bytes = contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: return false
        return nativeLoadBios(bytes) && nativeBiosLoaded()
    }

    private fun restoreSavedBios() {
        val saved = prefs.getString("bios_uri", null) ?: return
        val uri = Uri.parse(saved)
        try {
            val error = validateBios(uri)
            if (error != null || !loadBiosIntoCore(uri)) {
                biosInfo.text = "BIOS: saved file unavailable or failed to load"
                status.text = error ?: "Could not load saved BIOS into native core"
            } else {
                biosInfo.text = "BIOS: loaded into native PS1 core"
                status.text = "BIOS loaded • PS1 boot screen is not started"
            }
        } catch (e: Exception) {
            biosInfo.text = "BIOS: saved file unavailable"
            status.text = e.message ?: "Could not restore saved BIOS"
        }
    }

    private fun explainFileAccess(requestCode: Int, mime: String) {
        AlertDialog.Builder(this).setTitle("File access")
            .setMessage("NEXUS-PS needs permission to read the file you select. Android grants access only to the selected file.")
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Continue") { _, _ -> openPicker(requestCode, mime) }.show()
    }

    private fun openPicker(requestCode: Int, mime: String) {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE); type = mime
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, requestCode)
    }

    private fun validateBios(uri: Uri): String? {
        val expectedSize = 512L * 1024L
        val descriptor = contentResolver.openAssetFileDescriptor(uri, "r")
        descriptor?.use {
            if (it.length >= 0L && it.length != expectedSize) {
                return "Invalid BIOS size: ${it.length} bytes. A PS1 BIOS must be exactly 524,288 bytes (512 KiB)."
            }
        }

        val input = contentResolver.openInputStream(uri)
            ?: return "Android could not read this file."
        input.use { stream ->
            val buffer = ByteArray(8192)
            var total = 0L
            var allZero = true
            var allFF = true
            while (true) {
                val count = stream.read(buffer)
                if (count < 0) break
                if (count == 0) continue
                total += count
                if (total > expectedSize) {
                    return "This file is larger than 512 KiB, so it is not a supported PS1 BIOS image."
                }
                for (i in 0 until count) {
                    val value = buffer[i].toInt() and 0xFF
                    if (value != 0) allZero = false
                    if (value != 0xFF) allFF = false
                }
            }
            if (total != expectedSize) {
                return "Invalid BIOS size: $total bytes. Expected exactly 524,288 bytes (512 KiB)."
            }
            if (allZero || allFF) {
                return "The file contains only repeated empty data and does not look like a BIOS image."
            }
        }
        return null
    }

    private fun chooseGamesFolder() {
        AlertDialog.Builder(this).setTitle("Choose storage location")
            .setMessage("Choose a parent folder. NEXUS-PS will create NEXUS-PS Games inside it and request read/write access.")
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Choose") { _, _ ->
                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
                }
                startActivityForResult(intent, pickFolder)
            }.show()
    }

    private fun gamesDirectory(): DocumentFile? {
        val saved = prefs.getString("games_tree_uri", null) ?: return null
        val tree = DocumentFile.fromTreeUri(this, Uri.parse(saved)) ?: return null
        return tree.findFile("NEXUS-PS Games") ?: tree.createDirectory("NEXUS-PS Games")
    }

    private fun refreshFolderInfo() {
        val folder = gamesDirectory()
        folderInfo.text = if (folder != null && folder.exists()) "Games folder: NEXUS-PS Games (ready)" else "Games folder: not selected"
    }

    private fun chooseGamesToImport() {
        if (gamesDirectory() == null) {
            AlertDialog.Builder(this).setMessage("Choose or create the NEXUS-PS Games folder first.")
                .setPositiveButton("OK", null).show()
            return
        }
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE); type = "*/*"
            putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, importGames)
    }

    private fun displayName(uri: Uri): String {
        contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) return cursor.getString(0) ?: "game-file"
        }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "game-file"
    }

    private fun copyIntoGamesFolder(uri: Uri): String {
        val folder = gamesDirectory() ?: throw IllegalStateException("Games folder is unavailable")
        val name = displayName(uri)
        val target = folder.createFile(contentResolver.getType(uri) ?: "application/octet-stream", name)
            ?: throw IllegalStateException("Cannot create " + name)
        val input = contentResolver.openInputStream(uri) ?: throw IllegalStateException("Cannot read " + name)
        input.use { source ->
            val output = contentResolver.openOutputStream(target.uri, "w") ?: throw IllegalStateException("Cannot write " + name)
            output.use { destination -> source.copyTo(destination) }
        }
        return name
    }

    @Deprecated("Use Activity Result APIs in a later UI pass")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK || data == null) return

        if (requestCode == pickFolder) {
            val treeUri = data.data ?: return
            val flags = data.flags and (Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            try { contentResolver.takePersistableUriPermission(treeUri, flags) }
            catch (_: SecurityException) { status.text = "Could not keep folder permission"; return }
            prefs.edit().putString("games_tree_uri", treeUri.toString()).apply()
            status.text = if (gamesDirectory() != null) "NEXUS-PS Games folder is ready" else "Could not create games folder"
            refreshFolderInfo()
            return
        }

        if (requestCode == importGames) {
            val uris = mutableListOf<Uri>()
            data.clipData?.let { clip -> for (i in 0 until clip.itemCount) uris.add(clip.getItemAt(i).uri) }
                ?: data.data?.let { uris.add(it) }
            var copied = 0
            val errors = mutableListOf<String>()
            for (uri in uris) {
                try {
                    val flags = data.flags and Intent.FLAG_GRANT_READ_URI_PERMISSION
                    if (flags != 0) contentResolver.takePersistableUriPermission(uri, flags)
                    copyIntoGamesFolder(uri)
                    copied++
                } catch (e: Exception) {
                    errors.add(displayName(uri) + ": " + (e.message ?: "copy failed"))
                }
            }
            status.text = "Copied " + copied + " file(s)" + if (errors.isNotEmpty()) " • " + errors.size + " failed" else ""
            refreshFolderInfo()
            if (errors.isNotEmpty()) AlertDialog.Builder(this).setTitle("Some files were not copied")
                .setMessage(errors.joinToString("\n")).setPositiveButton("OK", null).show()
            return
        }

        val uri = data.data ?: return
        val granted = data.flags and Intent.FLAG_GRANT_READ_URI_PERMISSION
        if (granted != 0) try { contentResolver.takePersistableUriPermission(uri, granted) } catch (_: SecurityException) { }
        val name = displayName(uri)
        when (requestCode) {
            pickBios -> {
                status.text = "Checking BIOS file..."
                val error = try { validateBios(uri) } catch (e: Exception) {
                    e.message ?: "Could not inspect this file."
                }
                if (error != null) {
                    prefs.edit().remove("bios_uri").apply()
                    biosInfo.text = "BIOS: invalid file"
                    status.text = "BIOS validation failed"
                    AlertDialog.Builder(this).setTitle("Invalid PS1 BIOS")
                        .setMessage(error)
                        .setPositiveButton("OK", null).show()
                } else {
                    val loaded = try { loadBiosIntoCore(uri) } catch (_: Exception) { false }
                    if (loaded) {
                        prefs.edit().putString("bios_uri", uri.toString()).apply()
                        biosInfo.text = "BIOS loaded: $name (512 KiB)"
                        status.text = "BIOS is loaded inside the native PS1 core"
                    } else {
                        prefs.edit().remove("bios_uri").apply()
                        biosInfo.text = "BIOS: validated but native loading failed"
                        status.text = "The BIOS passed file checks but could not be loaded by the core"
                    }
                }
            }
            pickDisc -> discInfo.text = "Disc file selected: " + name
        }
    }
}
