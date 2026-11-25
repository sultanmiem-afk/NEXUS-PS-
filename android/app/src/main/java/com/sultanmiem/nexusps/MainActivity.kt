package com.sultanmiem.nexusps

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.widget.Button
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
        biosInfo = TextView(this).apply { text = "BIOS: not selected" }
        discInfo = TextView(this).apply { text = "Disc: not selected" }
        folderInfo = TextView(this).apply { text = "Games folder: not selected" }
        val biosButton = Button(this).apply { text = "Choose BIOS (any file type)"; setOnClickListener { explainFileAccess(pickBios, "*/*") } }
        val discButton = Button(this).apply { text = "Choose game (CUE or BIN)"; setOnClickListener { explainFileAccess(pickDisc, "*/*") } }
        val folderButton = Button(this).apply { text = "Create / choose games folder"; setOnClickListener { chooseGamesFolder() } }
        val importButton = Button(this).apply { text = "Copy games into NEXUS-PS folder"; setOnClickListener { chooseGamesToImport() } }
        val notice = TextView(this).apply {
            text = "Development build. Game loading, GPU display, audio and touch controls are not connected yet. Import copies files; originals are kept."
            textSize = 14f
            setPadding(0, 28, 0, 0)
        }
        listOf(title, subtitle, status, biosButton, biosInfo, discButton, discInfo, folderButton, folderInfo, importButton, notice).forEach { root.addView(it) }
        setContentView(root)
        refreshFolderInfo()
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
            pickBios -> biosInfo.text = "BIOS selected: " + name
            pickDisc -> discInfo.text = "Disc file selected: " + name
        }
    }
}
