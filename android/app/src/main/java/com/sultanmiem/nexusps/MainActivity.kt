package com.sultanmiem.nexusps

import android.app.Activity
import android.graphics.Color
import android.graphics.Canvas
import android.graphics.Paint
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
    private external fun nativeLoadDiscImage(image: ByteArray): Boolean
    private external fun nativeLoadDiscCue(cueText: String, binFilename: String, image: ByteArray): Boolean
    private external fun nativeBiosLoaded(): Boolean
    private external fun nativeRunForCycles(cycleBudget: Long): Long
    private external fun nativeSetControllerButtons(pressedMask: Int)
    private external fun nativeSetControllerAnalogAxes(x: Int, y: Int)
    private val pressedControllerButtons = mutableSetOf<String>()
    private var pendingCueText: String? = null
    private var pendingCueBinName: String? = null

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
        val runCoreButton = Button(this).apply {
            text = "Run PS1 core (100,000 cycles)"
            setOnClickListener {
                if (!nativeBiosLoaded()) {
                    status.text = "Select and load your PS1 BIOS first"
                } else {
                    isEnabled = false
                    val executed = nativeRunForCycles(100_000L)
                    status.text = "PS1 core executed $executed CPU cycles"
                    isEnabled = true
                }
            }
        }
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
            text = "Controller: connected to PS1 digital pad"
            textSize = 13f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 12)
        }
        val controller = createTouchController(controllerStatus)
        val notice = TextView(this).apply {
            text = "Touch buttons update the native PS1 controller through SIO0; the virtual stick sends continuous DualShock analog axes. Game execution, GPU display and audio output are still separate development tasks. Import copies files; originals are kept."
            textSize = 14f
            setPadding(0, 24, 0, 20)
        }
        listOf(title, subtitle, status, biosButton, biosInfo, runCoreButton, discButton, discInfo, folderButton, folderInfo, importButton, controllerTitle, controllerStatus, controller, notice).forEach { root.addView(it) }
        val scroll = ScrollView(this).apply { isFillViewport = true; addView(root) }
        setContentView(scroll)
        refreshFolderInfo()
        restoreSavedBios()
    }

    private fun createTouchController(statusLabel: TextView): View {
        fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

        fun button(label: String, accent: Int = Color.rgb(55, 65, 81), height: Int = 52): Button {
            val inputMask = controllerButtonMask(label)
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
                            if (inputMask != 0) updateControllerButton(label, true, statusLabel)
                            else statusLabel.text = "PSP-style controller"
                            true
                        }
                        MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                            view.alpha = 1f
                            view.scaleX = 1f
                            view.scaleY = 1f
                            if (inputMask != 0) updateControllerButton(label, false, statusLabel)
                            else statusLabel.text = "Controller: connected to PS1 digital pad"
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

        val joystickRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER; setPadding(0, dp(4), 0, dp(8)) }
        joystickRow.addView(createVirtualJoystick(statusLabel), LinearLayout.LayoutParams(dp(132), dp(132)))
        panel.addView(joystickRow)

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

    private fun createVirtualJoystick(statusLabel: TextView): View {
        val base = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(48, 57, 73) }
        val ring = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(103, 116, 139); style = Paint.Style.STROKE; strokeWidth = 5f }
        val knob = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(75, 139, 180) }
        val edge = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(175, 210, 230); style = Paint.Style.STROKE; strokeWidth = 3f }
        var axisX = 0f
        var axisY = 0f
        val view = object : View(this) {
            override fun onDraw(canvas: Canvas) {
                val cx = width / 2f; val cy = height / 2f; val r = minOf(width, height) * 0.43f
                canvas.drawCircle(cx, cy, r, base); canvas.drawCircle(cx, cy, r, ring)
                canvas.drawLine(cx-r*0.72f, cy, cx+r*0.72f, cy, ring); canvas.drawLine(cx, cy-r*0.72f, cx, cy+r*0.72f, ring)
                canvas.drawCircle(cx+axisX*r*0.68f, cy+axisY*r*0.68f, r*0.34f, knob)
                canvas.drawCircle(cx+axisX*r*0.68f, cy+axisY*r*0.68f, r*0.34f, edge)
            }
        }
        fun update(x: Float, y: Float) {
            val cx=view.width/2f; val cy=view.height/2f; val r=(minOf(view.width,view.height)*0.43f).coerceAtLeast(1f)
            var dx=(x-cx)/r; var dy=(y-cy)/r; val length=kotlin.math.sqrt(dx*dx+dy*dy)
            if (length > 1f) { dx/=length; dy/=length }
            val normalizedLength = kotlin.math.sqrt(dx*dx+dy*dy)
            if (normalizedLength <= 0.12f) { dx=0f; dy=0f }
            else { val scale = (normalizedLength-0.12f)/0.88f/normalizedLength; dx*=scale; dy*=scale }
            axisX=dx; axisY=dy
            val analogX = (128f + dx * 127f).toInt().coerceIn(0, 255)
            val analogY = (128f + dy * 127f).toInt().coerceIn(0, 255)
            nativeSetControllerAnalogAxes(analogX, analogY)
            statusLabel.text = "Analog X: $analogX • Y: $analogY"
            view.invalidate()
        }
        view.setOnTouchListener { _, event -> when(event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_POINTER_DOWN -> { update(event.x,event.y); true }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL, MotionEvent.ACTION_POINTER_UP -> { axisX=0f; axisY=0f; nativeSetControllerAnalogAxes(128, 128); statusLabel.text = "Analog stick centered • buttons ready"; view.invalidate(); true }
            else -> true
        } }
        return view
    }

    private fun controllerButtonMask(label: String): Int = when (label) {
        "SELECT" -> 1 shl 0
        "START" -> 1 shl 3
        "↑" -> 1 shl 4
        "→" -> 1 shl 5
        "↓" -> 1 shl 6
        "←" -> 1 shl 7
        "L2" -> 1 shl 8
        "R2" -> 1 shl 9
        "L1" -> 1 shl 10
        "R1" -> 1 shl 11
        "△" -> 1 shl 12
        "○" -> 1 shl 13
        "×" -> 1 shl 14
        "□" -> 1 shl 15
        else -> 0
    }

    private fun updateControllerButton(label: String, pressed: Boolean, statusLabel: TextView) {
        if (pressed) pressedControllerButtons.add(label) else pressedControllerButtons.remove(label)
        sendControllerState(statusLabel)
    }

    private fun sendControllerState(statusLabel: TextView) {
        val mask = pressedControllerButtons.fold(0) { acc, name -> acc or controllerButtonMask(name) }
        nativeSetControllerButtons(mask)
        statusLabel.text = if (pressedControllerButtons.isEmpty()) "DualShock analog controller connected" else "Pressed: " + pressedControllerButtons.joinToString(" + ")
    }

    private fun readBiosBytes(uri: Uri): ByteArray? =
        contentResolver.openInputStream(uri)?.use { it.readBytes() }

    private fun loadBiosIntoCore(bytes: ByteArray): Boolean =
        nativeLoadBios(bytes) && nativeBiosLoaded()

    private fun validateBios(bytes: ByteArray): String? {
        val expectedSize = 512 * 1024
        if (bytes.size != expectedSize) {
            return "Invalid BIOS size: ${bytes.size} bytes. Expected exactly 524,288 bytes (512 KiB)."
        }
        if (bytes.all { it == 0.toByte() } || bytes.all { (it.toInt() and 0xFF) == 0xFF }) {
            return "The file contains only repeated empty data and does not look like a BIOS image."
        }
        return null
    }

    private fun detectBiosRegion(bytes: ByteArray): String {
        // PS1 BIOS ROMs commonly include a human-readable region string.
        // If a revision omits it, report Unknown rather than guessing from the filename.
        val romText = String(bytes, Charsets.ISO_8859_1).uppercase()
        val markers = listOf(
            "FOR JAPAN AREA" to "Japan (NTSC-J)",
            "FOR U/C AREA" to "North America (NTSC-U/C)",
            "FOR EUROPE AREA" to "Europe (PAL)",
            "FOR ASIA AREA" to "Asia",
            "FOR CHINA AREA" to "China"
        )
        return markers.firstOrNull { romText.contains(it.first) }?.second
            ?: "Unknown region"
    }

    private fun restoreSavedBios() {
        val saved = prefs.getString("bios_uri", null) ?: return
        val uri = Uri.parse(saved)
        try {
            val bytes = readBiosBytes(uri)
            val error = if (bytes == null) "Android could not read the saved BIOS file." else validateBios(bytes)
            if (error != null || bytes == null || !loadBiosIntoCore(bytes)) {
                biosInfo.text = "BIOS: saved file unavailable or failed to load"
                status.text = error ?: "Could not load saved BIOS into native core"
            } else {
                val region = detectBiosRegion(bytes)
                biosInfo.text = "BIOS: $region"
                status.text = "BIOS loaded • Region: $region • PS1 boot screen is not started"
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
                val bytes = try { readBiosBytes(uri) } catch (e: Exception) {
                    status.text = e.message ?: "Could not read this file."
                    null
                }
                val error = if (bytes == null) "Android could not read this file." else validateBios(bytes)
                if (error != null) {
                    prefs.edit().remove("bios_uri").apply()
                    biosInfo.text = "BIOS: invalid file"
                    status.text = "BIOS validation failed"
                    AlertDialog.Builder(this).setTitle("Invalid PS1 BIOS")
                        .setMessage(error)
                        .setPositiveButton("OK", null).show()
                } else {
                    val loaded = try { bytes != null && loadBiosIntoCore(bytes) } catch (_: Exception) { false }
                    if (loaded && bytes != null) {
                        val region = detectBiosRegion(bytes)
                        prefs.edit().putString("bios_uri", uri.toString()).apply()
                        biosInfo.text = "BIOS: $region • $name"
                        status.text = "BIOS loaded successfully • Region: $region"
                    } else {
                        prefs.edit().remove("bios_uri").apply()
                        biosInfo.text = "BIOS: validated but native loading failed"
                        status.text = "The BIOS passed file checks but could not be loaded by the core"
                    }
                }
            }
            pickDisc -> {
                val pendingCue = pendingCueText
                if (pendingCue == null && name.substringAfterLast('.', "").equals("cue", true)) {
                    val cueText = try { readBiosBytes(uri)?.toString(Charsets.UTF_8) } catch (_: Exception) { null }
                    val referencedBin = cueText?.lineSequence()?.mapNotNull { line ->
                        val trimmed = line.trim()
                        if (!trimmed.startsWith("FILE ", true)) null
                        else if (trimmed.length > 5 && trimmed[5] == '"') trimmed.substringAfter('"').substringBefore('"')
                        else trimmed.substringAfter("FILE", "").trim().substringBefore(' ')
                    }?.firstOrNull { it.isNotBlank() }
                    if (cueText == null || referencedBin == null) {
                        discInfo.text = "Disc: invalid CUE sheet"
                        status.text = "Could not find a FILE entry in this CUE sheet."
                    } else {
                        pendingCueText = cueText
                        pendingCueBinName = referencedBin.substringAfterLast('/')
                        status.text = "CUE loaded. Now choose its BIN file: $pendingCueBinName"
                        explainFileAccess(pickDisc, "*/*")
                    }
                    return
                }
                status.text = "Reading disc image..."
                val image = try { readBiosBytes(uri) } catch (e: Exception) {
                    status.text = e.message ?: "Could not read disc image"
                    null
                }
                val expectedBin = pendingCueBinName
                if (pendingCue != null && expectedBin != null && !name.equals(expectedBin, true)) {
                    pendingCueText = null
                    pendingCueBinName = null
                    discInfo.text = "Disc: wrong BIN selected"
                    status.text = "Expected $expectedBin but selected $name. Choose the CUE again."
                } else if (image == null) {
                    pendingCueText = null
                    pendingCueBinName = null
                    discInfo.text = "Disc: could not read file"
                    status.text = "Disc image read failed"
                } else if (image.size < 2048 || (image.size % 2352 != 0 && image.size % 2048 != 0)) {
                    pendingCueText = null
                    pendingCueBinName = null
                    discInfo.text = "Disc: invalid image"
                    status.text = "Unsupported disc image size. Expected raw 2352-byte or cooked 2048-byte sectors."
                } else if (!nativeBiosLoaded()) {
                    pendingCueText = null
                    pendingCueBinName = null
                    discInfo.text = "Disc: selected, BIOS required"
                    status.text = "Load a valid PS1 BIOS before loading a game."
                } else {
                    val loaded = if (pendingCue != null) nativeLoadDiscCue(pendingCue, name, image)
                                 else nativeLoadDiscImage(image)
                    pendingCueText = null
                    pendingCueBinName = null
                    if (loaded) {
                        discInfo.text = "Disc loaded: $name • ${image.size / 1024 / 1024} MiB"
                        status.text = if (pendingCue != null) "CUE/BIN disc mounted in PS1 CD-ROM core"
                                      else "BIN disc image loaded into PS1 CD-ROM core"
                    } else {
                        discInfo.text = "Disc: core rejected image"
                        status.text = "The CD-ROM core could not mount this image."
                    }
                }
            }
        }
    }
}
