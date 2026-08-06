package com.uvpainter.state

import android.content.Context
import android.net.Uri
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.asImageBitmap
import com.uvpainter.engine.NativeBridge
import com.uvpainter.engine.RenderThread
import com.uvpainter.io.TextureIo
import java.io.File
import kotlinx.serialization.json.Json

/**
 * Une la interfaz con el motor nativo.
 *
 * Toda llamada que toque GL se encola en [RenderThread]; la UI solo lee estado
 * de Compose, que se refresca desde los callbacks.
 */
class PainterController(private val appContext: Context) {

    val handle: Long = NativeBridge.nativeCreate()
    val renderThread = RenderThread(handle).also { it.start() }

    /** Fotogramas por segundo del hilo de render. Solo se mide con [showFps]. */
    var fps by mutableStateOf(0f)
        private set
    var showFps by mutableStateOf(false)
        private set

    fun setFpsVisible(value: Boolean) {
        showFps = value
        renderThread.onFps = if (value) { measured -> postToUi { fps = measured } } else null
        // Un contador que solo se mueve mientras pintas no dice mucho: con el
        // panel abierto se pasa a redibujar en bucle, para que el numero sea
        // real tambien en reposo. Se apaga entero al quitar el interruptor.
        renderThread.setContinuousRendering(value)
        scheduleSettingsSave()
    }

    var brush by mutableStateOf(BrushState())
        private set
    var viewport by mutableStateOf(ViewportState())
        private set
    var paintMode by mutableStateOf(PaintMode.PAINT)
        private set
    var meshInfo by mutableStateOf(MeshInfo())
        private set

    val layers = mutableStateListOf<LayerUi>()
    var activeLayer by mutableStateOf(0)
        private set

    var canUndo by mutableStateOf(false)
        private set
    var canRedo by mutableStateOf(false)
        private set
    var historyMemoryMb by mutableStateOf(0)
        private set

    var documentResolution by mutableStateOf(2048)
        private set
    var uiVisible by mutableStateOf(true)
    var statusMessage by mutableStateOf<String?>(null)
    var busy by mutableStateOf(false)
        private set

    // -----------------------------------------------------------------------
    // Barra principal: el usuario la coloca donde le estorbe menos
    // -----------------------------------------------------------------------
    var toolbarOffsetX by mutableStateOf(0f)
        private set
    var toolbarOffsetY by mutableStateOf(0f)
        private set
    var toolbarScale by mutableStateOf(1f)
        private set

    fun moveToolbar(deltaX: Float, deltaY: Float) {
        toolbarOffsetX += deltaX
        toolbarOffsetY += deltaY
        scheduleSettingsSave()
    }

    fun scaleToolbar(factor: Float) {
        toolbarScale = (toolbarScale * factor).coerceIn(0.6f, 2.0f)
        scheduleSettingsSave()
    }

    fun resetToolbarPlacement() {
        toolbarOffsetX = 0f
        toolbarOffsetY = 0f
        toolbarScale = 1f
        scheduleSettingsSave()
    }

    /** Longitud de cuerda que se recupera al volver a activar el regulador. */
    var stabilizerMemory by mutableStateOf(24f)
        private set

    fun setStabilizerEnabled(enabled: Boolean) {
        if (enabled) {
            updateBrush { it.copy(stabilizerRadiusPx = stabilizerMemory.coerceAtLeast(4f)) }
        } else {
            if (brush.stabilizerRadiusPx > 0.5f) stabilizerMemory = brush.stabilizerRadiusPx
            updateBrush { it.copy(stabilizerRadiusPx = 0f) }
        }
    }

    fun setStabilizerRadius(value: Float) {
        stabilizerMemory = value
        updateBrush { it.copy(stabilizerRadiusPx = value) }
    }

    /**
     * Avisa de que se descarto un contacto por rechazo de palma. Dar señal
     * visible importa: sin ella el usuario no sabe si la app le ignoro por
     * diseño o si simplemente no responde.
     */
    fun notePalmRejected() {
        val now = System.currentTimeMillis()
        if (now - lastPalmNoticeMs < 2500) return
        lastPalmNoticeMs = now
        statusMessage = "Apoyo de la mano descartado"
    }

    private var lastPalmNoticeMs = 0L

    /** 0 = Y arriba (glTF/Unity), 1 = Z arriba (Blender/3ds Max). */
    var upAxis by mutableStateOf(0)
        private set
    var flipUp by mutableStateOf(false)
        private set
    var quarterTurns by mutableStateOf(0)
        private set

    fun setOrientation(
        newUpAxis: Int = upAxis,
        newFlipUp: Boolean = flipUp,
        newQuarterTurns: Int = quarterTurns,
    ) {
        renderThread.post {
            NativeBridge.nativeSetOrientation(handle, newUpAxis, newFlipUp, newQuarterTurns)
            val orientation = NativeBridge.nativeGetOrientation(handle)
            renderThread.requestRender()
            postToUi { applyOrientationSnapshot(orientation) }
        }
    }

    private fun applyOrientationSnapshot(orientation: IntArray) {
        upAxis = orientation[0]
        flipUp = orientation[1] == 1
        quarterTurns = orientation[2]
    }

    val brushPresets = mutableStateListOf<BrushPreset>().apply { addAll(defaultBrushPresets) }
    val recentColors = mutableStateListOf<androidx.compose.ui.graphics.Color>()
    val favoriteColors = mutableStateListOf<androidx.compose.ui.graphics.Color>()

    var input by mutableStateOf(InputSettings())
        private set
    var tool by mutableStateOf(Tool.BRUSH)
        private set

    private val prefs = appContext.getSharedPreferences("uvpainter", Context.MODE_PRIVATE)

    // -----------------------------------------------------------------------
    // Ajustes: todo lo que no es pintura se recuerda entre sesiones
    // -----------------------------------------------------------------------
    private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }

    private val settingsWriter = Runnable {
        val settings = UiSettings(
            brush = brush,
            input = input,
            viewport = viewport,
            tool = tool,
            documentResolution = documentResolution,
            toolbarOffsetX = toolbarOffsetX,
            toolbarOffsetY = toolbarOffsetY,
            toolbarScale = toolbarScale,
            stabilizerMemory = stabilizerMemory,
            autoSaveEnabled = autoSaveEnabled,
            autoSaveMinutes = autoSaveMinutes,
            openProjectPath = currentProjectPath,
            showFps = showFps,
        )
        runCatching {
            prefs.edit().putString(KEY_SETTINGS, json.encodeToString(settings)).apply()
        }
    }

    /**
     * Los ajustes se escriben medio segundo después del último cambio: arrastrar
     * un deslizador dispara decenas de cambios por segundo y no tiene sentido
     * escribir en disco en cada uno.
     */
    private fun scheduleSettingsSave() {
        uiHandler.removeCallbacks(settingsWriter)
        uiHandler.postDelayed(settingsWriter, 500)
    }

    /** Fuerza la escritura pendiente. Se llama al pausar la actividad. */
    fun flushSettings() {
        uiHandler.removeCallbacks(settingsWriter)
        settingsWriter.run()
    }

    private fun restoreSettings() {
        val raw = prefs.getString(KEY_SETTINGS, null) ?: return
        val saved = runCatching { json.decodeFromString<UiSettings>(raw) }.getOrNull() ?: return
        brush = saved.brush
        input = saved.input
        viewport = saved.viewport
        tool = saved.tool
        paintMode = paintModeFor(saved.tool)
        documentResolution = saved.documentResolution
        toolbarOffsetX = saved.toolbarOffsetX
        toolbarOffsetY = saved.toolbarOffsetY
        toolbarScale = saved.toolbarScale
        stabilizerMemory = saved.stabilizerMemory
        autoSaveEnabled = saved.autoSaveEnabled
        autoSaveMinutes = saved.autoSaveMinutes
        // Se comprueba que siga existiendo: si lo borraron desde el explorador,
        // el autoguardado lo resucitaría sin que nadie lo haya pedido.
        currentProjectPath = saved.openProjectPath?.takeIf { File(it).isFile }
        showFps = saved.showFps
        if (showFps) renderThread.onFps = { measured -> postToUi { fps = measured } }
    }

    private fun paintModeFor(value: Tool): PaintMode = when (value) {
        Tool.ERASER -> PaintMode.ERASE
        Tool.BOUNDARY -> PaintMode.BOUNDARY
        else -> PaintMode.PAINT
    }

    /** Se guarda el pincel activo para poder volver tras usar el borrador. */
    private var brushBeforeErase: BrushState? = null
    private var surfaceReady = false

    // -----------------------------------------------------------------------
    // Arranque
    // -----------------------------------------------------------------------
    fun onSurfaceReady(defaultModelAsset: String?) {
        if (surfaceReady) {
            // Volvemos de segundo plano con un contexto GL nuevo. El motor ya ha
            // vuelto a subir la malla y las capas por su cuenta, pero el pincel
            // y la vista viven en uniformes que hay que reenviar, y el historial
            // se fue con el contexto anterior.
            renderThread.post {
                pushBrush()
                pushViewport()
                NativeBridge.nativeSetPaintMode(handle, paintMode.nativeValue)
                refreshLayersOnRenderThread()
                renderThread.requestRender()
            }
            // El contexto anterior murio con la superficie, y con el la bandera
            // interna del bucle continuo. Si el contador seguia activo, se
            // retoma; si no, no hace nada.
            if (showFps) renderThread.setContinuousRendering(true)
            return
        }
        surfaceReady = true

        renderThread.post {
            pushBrush()
            pushViewport()
            NativeBridge.nativeSetPaintMode(handle, paintMode.nativeValue)

            // Primero se intenta retomar la sesion anterior tal cual se dejo;
            // solo si no hay ninguna se carga el modelo de arranque.
            var info: MeshInfo? = null
            var restored = false
            if (sessionFile.exists()) {
                val error = NativeBridge.nativeLoadProject(handle, sessionFile.absolutePath)
                if (error == null) {
                    restored = true
                    info = readMeshInfo(NativeBridge.nativeGetModelName(handle))
                } else {
                    postToUi { statusMessage = "No se pudo recuperar la sesión: $error" }
                }
            }

            // El modelo primero y el documento despues: crear el documento
            // recompila sus shaders, y hacerlo dos veces es tiempo tirado.
            if (!restored && defaultModelAsset != null) {
                val bytes = runCatching {
                    appContext.assets.open(defaultModelAsset).use { it.readBytes() }
                }.getOrNull()
                if (bytes != null) {
                    val name = defaultModelAsset.substringAfterLast('/')
                    val error = NativeBridge.nativeLoadModel(handle, bytes, "glb", name)
                    if (error == null) {
                        info = readMeshInfo(name)
                    } else {
                        postToUi { statusMessage = error }
                    }
                }
            }

            val created = restored || NativeBridge.nativeCreateDocument(handle, documentResolution)
            refreshLayersOnRenderThread()
            val orientation = NativeBridge.nativeGetOrientation(handle)
            val boundary = NativeBridge.nativeHasBoundary(handle)
            NativeBridge.nativeDrawFrame(handle)
            postToUi {
                info?.let { loaded -> meshInfo = loaded }
                applyOrientationSnapshot(orientation)
                hasBoundary = boundary
                if (restored) statusMessage = "Sesión recuperada"
                // Si el documento no se pudo crear, decirlo: en silencio parece
                // que la app funciona pero el pincel no hace nada.
                if (!created) statusMessage = "No se pudo crear el documento de pintura"
                if (showFps) renderThread.setContinuousRendering(true)
            }
        }
    }

    fun destroy() {
        uiHandler.removeCallbacks(autoSaveRunnable)
        renderThread.shutdown {
            NativeBridge.nativeSurfaceDestroyed(handle)
            NativeBridge.nativeDestroy(handle)
        }
    }

    // -----------------------------------------------------------------------
    // Proyectos
    //
    // Un .uvp guarda el modelo, las capas con sus pixeles y los limites, asi que
    // cerrar la app deja de significar empezar de cero. La sesion se guarda sola
    // al salir; los proyectos con nombre los guarda el usuario.
    // -----------------------------------------------------------------------
    private val sessionFile = File(appContext.filesDir, "sesion.uvp")

    /** Carpeta visible desde el explorador: Android/data/com.uvpainter/files. */
    val projectsDir: File =
        File(appContext.getExternalFilesDir(null) ?: appContext.filesDir, "Proyectos")

    var projects by mutableStateOf<List<ProjectEntry>>(emptyList())
        private set

    /**
     * Proyecto con nombre que está abierto ahora mismo, si lo hay. El
     * autoguardado escribe aquí; sin él, `null` y solo se toca la sesión.
     */
    var currentProjectPath by mutableStateOf<String?>(null)
        private set

    val currentProjectName: String?
        get() = currentProjectPath?.let { File(it).nameWithoutExtension }

    fun refreshProjects() {
        val found = runCatching {
            projectsDir.listFiles { file -> file.isFile && file.name.endsWith(EXT) }
                ?.sortedByDescending { it.lastModified() }
                ?.map { ProjectEntry(it.nameWithoutExtension, it.absolutePath, it.length(), it.lastModified()) }
                .orEmpty()
        }.getOrDefault(emptyList())
        projects = found
    }

    fun saveProjectAs(rawName: String) {
        val name = sanitizeName(rawName)
        if (name.isEmpty()) {
            statusMessage = "Ponle un nombre al proyecto"
            return
        }
        busy = true
        renderThread.post {
            projectsDir.mkdirs()
            val target = File(projectsDir, "$name$EXT")
            val error = NativeBridge.nativeSaveProject(handle, target.absolutePath)
            postToUi {
                busy = false
                statusMessage = error ?: "Proyecto guardado: $name"
                // Guardar con nombre pasa a ser el proyecto abierto: a partir de
                // aquí el autoguardado escribe ahí y no solo en la sesión.
                if (error == null) setCurrentProject(target.absolutePath)
                refreshProjects()
            }
        }
    }

    fun openProject(path: String) {
        busy = true
        documentDirty = true
        renderThread.post {
            val error = NativeBridge.nativeLoadProject(handle, path)
            if (error != null) {
                postToUi {
                    busy = false
                    statusMessage = error
                }
                return@post
            }
            val info = readMeshInfo(NativeBridge.nativeGetModelName(handle))
            refreshLayersOnRenderThread()
            val orientation = NativeBridge.nativeGetOrientation(handle)
            val boundary = NativeBridge.nativeHasBoundary(handle)
            NativeBridge.nativeDrawFrame(handle)
            postToUi {
                busy = false
                meshInfo = info
                applyOrientationSnapshot(orientation)
                hasBoundary = boundary
                setCurrentProject(path)
                statusMessage = "Proyecto abierto: ${File(path).nameWithoutExtension}"
            }
        }
    }

    fun deleteProject(path: String) {
        runCatching { File(path).delete() }
        // Si era el abierto, dejar de apuntarlo: el autoguardado lo recrearía.
        if (currentProjectPath == path) setCurrentProject(null)
        refreshProjects()
        statusMessage = "Proyecto borrado"
    }

    private fun setCurrentProject(path: String?) {
        currentProjectPath = path
        scheduleSettingsSave()
    }

    /**
     * Guarda la sesion. Se llama al pausar la actividad y espera a que termine:
     * el proceso puede morir en cuanto la app deja de estar delante, y un
     * guardado a medias no sirve de nada.
     */
    fun saveSessionBlocking() {
        // Abrir un selector de archivos también pausa la actividad, así que sin
        // esta comprobación se pagaría el guardado entero cada vez que se busca
        // una imagen sin haber tocado el dibujo.
        if (!surfaceReady || !documentDirty) return
        // El proyecto abierto solo se actualiza si el autoguardado está puesto:
        // sin él, «Guardar» es tuyo y no se toca a tus espaldas.
        val project = currentProjectPath.takeIf { autoSaveEnabled }
        renderThread.postAndWait(timeoutMs = 20_000) { writeCheckpoint(project) }
    }

    /** Marca que hay pintura sin guardar. Lo pone todo lo que toca las capas. */
    @Volatile
    private var documentDirty = false

    // -----------------------------------------------------------------------
    // Autoguardado
    //
    // Guardar al pausar cubre el caso normal, pero no el que de verdad duele:
    // que la app se vaya abajo con dos horas de trabajo encima. Esto deja un
    // punto de control en la sesión, que es justo lo que se recupera al abrir.
    // -----------------------------------------------------------------------
    var autoSaveEnabled by mutableStateOf(false)
        private set
    var autoSaveMinutes by mutableStateOf(5)
        private set
    var lastAutoSaveMs by mutableStateOf(0L)
        private set

    fun setAutoSave(enabled: Boolean = autoSaveEnabled, minutes: Int = autoSaveMinutes) {
        autoSaveEnabled = enabled
        autoSaveMinutes = minutes.coerceIn(1, 60)
        scheduleSettingsSave()
        scheduleAutoSave()
    }

    /**
     * Escribe el punto de control. Debe correr en el hilo de render.
     *
     * Con un proyecto abierto se escribe ahí y el archivo se copia a la sesión.
     * Volcar el documento dos veces costaría el doble —el volcado es leer el
     * atlas entero y comprimirlo—; copiar un `.uvp` ya escrito son un par de
     * cientos de KB. Devuelve null si fue bien, o el error.
     */
    private fun writeCheckpoint(project: String?): String? {
        val target = project ?: sessionFile.absolutePath
        val error = NativeBridge.nativeSaveProject(handle, target)
        if (error == null) {
            documentDirty = false
            if (project != null) {
                runCatching { File(project).copyTo(sessionFile, overwrite = true) }
            }
        }
        return error
    }

    private val autoSaveRunnable = Runnable { runAutoSave() }

    private fun scheduleAutoSave() {
        uiHandler.removeCallbacks(autoSaveRunnable)
        if (!autoSaveEnabled) return
        uiHandler.postDelayed(autoSaveRunnable, autoSaveMinutes * 60_000L)
    }

    private fun runAutoSave() {
        // Sin cambios desde el último guardado no hay nada que escribir, y leer
        // el atlas entero para volver a guardar lo mismo cuesta casi un segundo.
        if (surfaceReady && documentDirty) {
            val project = currentProjectPath
            renderThread.post {
                val error = writeCheckpoint(project)
                postToUi {
                    if (error == null) {
                        lastAutoSaveMs = System.currentTimeMillis()
                        if (project != null) refreshProjects()
                    } else {
                        statusMessage = "El autoguardado falló: $error"
                    }
                }
            }
        }
        scheduleAutoSave()
    }

    /** Empieza de cero con el modelo actual: capas nuevas y sin sesion guardada. */
    fun newProject() {
        busy = true
        renderThread.post {
            NativeBridge.nativeCreateDocument(handle, documentResolution)
            refreshLayersOnRenderThread()
            NativeBridge.nativeDrawFrame(handle)
            runCatching { sessionFile.delete() }
            documentDirty = false
            postToUi {
                busy = false
                hasBoundary = false
                // Empezar de cero desengancha del proyecto anterior: si no, el
                // autoguardado lo machacaría con el lienzo vacío.
                setCurrentProject(null)
                statusMessage = "Proyecto nuevo"
            }
        }
    }

    private fun sanitizeName(raw: String): String =
        raw.trim().replace(Regex("[^\\p{L}\\p{N} _-]"), "").take(48)

    // -----------------------------------------------------------------------
    // Pincel
    // -----------------------------------------------------------------------
    fun updateBrush(transform: (BrushState) -> BrushState) {
        brush = transform(brush)
        renderThread.post { pushBrush() }
        scheduleSettingsSave()
    }

    fun applyPreset(preset: BrushPreset) {
        // El color no se toca: es del artista, no del preajuste.
        brush = preset.brush.copy(color = brush.color)
        renderThread.post { pushBrush() }
        scheduleSettingsSave()
    }

    /**
     * Cambia el color activo. NO toca el historial: mientras arrastras por el
     * selector esto se llama decenas de veces por segundo, y registrarlas todas
     * llenaba los "recientes" de variaciones del mismo tono.
     */
    fun setColor(color: androidx.compose.ui.graphics.Color) {
        brush = brush.copy(color = color)
        renderThread.post { pushBrush() }
        scheduleSettingsSave()
    }

    /** Fija el color actual en el historial. Se llama al soltar el selector. */
    fun commitColor(color: androidx.compose.ui.graphics.Color = brush.color) {
        // Se descartan los colores casi idénticos al último: si no, un ajuste
        // fino deja tres entradas indistinguibles.
        val duplicate = recentColors.firstOrNull { isNearlySame(it, color) }
        if (duplicate != null) recentColors.remove(duplicate)
        recentColors.add(0, color)
        while (recentColors.size > 30) recentColors.removeAt(recentColors.lastIndex)
        persistColors(KEY_RECENTS, recentColors)
    }

    fun toggleFavorite(color: androidx.compose.ui.graphics.Color = brush.color) {
        val existing = favoriteColors.firstOrNull { isNearlySame(it, color) }
        if (existing != null) favoriteColors.remove(existing) else favoriteColors.add(0, color)
        while (favoriteColors.size > 40) favoriteColors.removeAt(favoriteColors.lastIndex)
        persistColors(KEY_FAVORITES, favoriteColors)
    }

    fun isFavorite(color: androidx.compose.ui.graphics.Color): Boolean =
        favoriteColors.any { isNearlySame(it, color) }

    private fun isNearlySame(
        a: androidx.compose.ui.graphics.Color,
        b: androidx.compose.ui.graphics.Color,
    ): Boolean {
        val dr = kotlin.math.abs(a.red - b.red)
        val dg = kotlin.math.abs(a.green - b.green)
        val db = kotlin.math.abs(a.blue - b.blue)
        return dr + dg + db < 0.035f
    }

    private fun persistColors(
        key: String,
        colors: List<androidx.compose.ui.graphics.Color>,
    ) {
        prefs.edit()
            .putString(key, colors.joinToString(",") { it.value.toULong().toString(16) })
            .apply()
    }

    private fun restoreColors(
        key: String,
        into: MutableList<androidx.compose.ui.graphics.Color>,
    ) {
        val raw = prefs.getString(key, null) ?: return
        raw.split(',').forEach { token ->
            val value = token.trim().toULongOrNull(16) ?: return@forEach
            into.add(androidx.compose.ui.graphics.Color(value))
        }
    }

    /**
     * `persist` en false para los cambios temporales (los del botón del S-Pen):
     * si se guardaran, soltar el lápiz con la app cerrándose dejaría el
     * cuentagotas como herramienta al volver.
     */
    fun changeTool(newTool: Tool, persist: Boolean = true) {
        tool = newTool
        changePaintMode(paintModeFor(newTool))
        // La figura activa depende de la herramienta, así que hay que reenviar
        // el pincel al cambiarla.
        renderThread.post { pushBrush() }
        if (persist) scheduleSettingsSave()
    }

    fun clearBoundary() {
        documentDirty = true
        renderThread.post {
            NativeBridge.nativeClearBoundary(handle)
            renderThread.requestRender()
            postToUi {
                hasBoundary = false
                statusMessage = "Límites borrados"
            }
        }
    }

    fun refreshBoundaryState() {
        renderThread.post {
            val has = NativeBridge.nativeHasBoundary(handle)
            postToUi { hasBoundary = has }
        }
    }

    var hasBoundary by mutableStateOf(false)
        private set

    fun updateInput(transform: (InputSettings) -> InputSettings) {
        input = transform(input)
        scheduleSettingsSave()
    }

    /** Bote de pintura en el punto tocado. */
    fun fillAt(x: Float, y: Float) {
        documentDirty = true
        NativeBridge.nativeFillAt(handle, x, y)
        renderThread.requestRender()
        renderThread.post {
            refreshLayersOnRenderThread()
            renderThread.requestRender()
        }
    }

    fun changePaintMode(mode: PaintMode) {
        paintMode = mode
        renderThread.post {
            NativeBridge.nativeSetPaintMode(handle, mode.nativeValue)
            renderThread.requestRender()
        }
    }

    /** Borrador temporal mientras se mantiene el boton del S-Pen. */
    fun setTemporaryErase(active: Boolean) {
        if (active) {
            if (brushBeforeErase == null) {
                brushBeforeErase = brush
                changePaintMode(PaintMode.ERASE)
            }
        } else if (brushBeforeErase != null) {
            brushBeforeErase = null
            changePaintMode(PaintMode.PAINT)
        }
    }

    /**
     * Botón lateral del S-Pen. Se llama al pulsar y al soltar.
     *
     * Las acciones momentáneas se deshacen al soltar; las demás se disparan una
     * sola vez, al pulsar, porque repetirlas al soltar duplicaría un deshacer.
     */
    fun onPenButton(pressed: Boolean) {
        when (input.penButtonAction) {
            PenButtonAction.NONE -> Unit
            PenButtonAction.ERASE_WHILE_HELD -> setTemporaryErase(pressed)
            PenButtonAction.PICK_WHILE_HELD -> setTemporaryTool(pressed, Tool.PICKER)
            PenButtonAction.TOGGLE_ERASER -> if (pressed) {
                changeTool(if (tool == Tool.ERASER) Tool.BRUSH else Tool.ERASER)
            }
            PenButtonAction.UNDO -> if (pressed) undo()
            PenButtonAction.REDO -> if (pressed) redo()
            PenButtonAction.TOGGLE_STABILIZER -> if (pressed) {
                setStabilizerEnabled(brush.stabilizerRadiusPx <= 0.5f)
            }
            PenButtonAction.RESET_VIEW -> if (pressed) resetView()
            PenButtonAction.TOGGLE_UI -> if (pressed) uiVisible = !uiVisible
        }
    }

    /** Cambia de herramienta mientras se mantiene el botón, y la devuelve al soltar. */
    private fun setTemporaryTool(active: Boolean, temporary: Tool) {
        if (active) {
            if (toolBeforePenButton == null && tool != temporary) {
                toolBeforePenButton = tool
                changeTool(temporary, persist = false)
            }
        } else {
            toolBeforePenButton?.let { previous ->
                toolBeforePenButton = null
                changeTool(previous, persist = false)
            }
        }
    }

    private var toolBeforePenButton: Tool? = null

    fun pickColorAt(x: Int, y: Int) {
        renderThread.post {
            val argb = NativeBridge.nativePickColor(handle, x, y)
            val color = androidx.compose.ui.graphics.Color(argb)
            postToUi { setColor(color) }
        }
    }

    private fun pushBrush() {
        var flags = 0
        if (brush.pressureAffectsSize) flags = flags or NativeBridge.FLAG_PRESSURE_SIZE
        if (brush.pressureAffectsOpacity) flags = flags or NativeBridge.FLAG_PRESSURE_OPACITY
        if (brush.depthTest) flags = flags or NativeBridge.FLAG_DEPTH_TEST
        if (brush.backfaceCull) flags = flags or NativeBridge.FLAG_BACKFACE_CULL
        if (brush.alphaLock) flags = flags or NativeBridge.FLAG_ALPHA_LOCK
        if (brush.restrictToIsland) flags = flags or NativeBridge.FLAG_RESTRICT_ISLAND
        if (brush.restrictToRegion) flags = flags or NativeBridge.FLAG_RESTRICT_REGION
        if (brush.tipFollowsStroke) flags = flags or NativeBridge.FLAG_TIP_FOLLOWS_STROKE
        if (brush.shapeFromCenter) flags = flags or NativeBridge.FLAG_SHAPE_FROM_CENTER
        if (brush.lockSizeToSurface) flags = flags or NativeBridge.FLAG_LOCK_SIZE_TO_SURFACE

        val values = FloatArray(NativeBridge.B_FLOAT_COUNT)
        values[NativeBridge.B_RADIUS] = brush.radiusPx
        values[NativeBridge.B_HARDNESS] = brush.hardness
        values[NativeBridge.B_OPACITY] = brush.opacity
        values[NativeBridge.B_FLOW] = brush.flow
        values[NativeBridge.B_SPACING] = brush.spacingPx
        values[NativeBridge.B_RED] = brush.color.red
        values[NativeBridge.B_GREEN] = brush.color.green
        values[NativeBridge.B_BLUE] = brush.color.blue
        values[NativeBridge.B_SMOOTHING] = brush.smoothing
        values[NativeBridge.B_STABILIZER] = brush.stabilizerRadiusPx
        values[NativeBridge.B_PRESSURE_GAIN] = brush.pressureGain
        values[NativeBridge.B_PRESSURE_CURVE] = brush.pressureCurve
        values[NativeBridge.B_SIZE_FLOOR] = brush.pressureSizeFloor
        values[NativeBridge.B_OPACITY_FLOOR] = brush.pressureOpacityFloor
        values[NativeBridge.B_TILT_SIZE] = brush.tiltSize
        values[NativeBridge.B_FACING_CUTOFF] = brush.facingCutoff
        values[NativeBridge.B_FACING_FULL] = brush.facingFull
        values[NativeBridge.B_TIP_ASPECT] = brush.tipAspect
        values[NativeBridge.B_TIP_ANGLE] = brush.tipAngle
        values[NativeBridge.B_GRAIN_AMOUNT] = brush.grainAmount
        values[NativeBridge.B_GRAIN_SCALE] = brush.grainScale

        val options = IntArray(NativeBridge.B_INT_COUNT)
        options[NativeBridge.B_FLAGS] = flags
        options[NativeBridge.B_TIP_SHAPE] = brush.tipShape.nativeValue
        // La figura solo se aplica con la herramienta de formas activa.
        options[NativeBridge.B_SHAPE] =
            if (tool == Tool.SHAPE) brush.shape.nativeValue else ShapeKind.NONE.nativeValue
        options[NativeBridge.B_POLYGON_SIDES] = brush.polygonSides

        NativeBridge.nativeSetBrush(handle, values, options)
        renderThread.requestRender()
    }

    // -----------------------------------------------------------------------
    // Visor
    // -----------------------------------------------------------------------
    fun updateViewport(transform: (ViewportState) -> ViewportState) {
        viewport = transform(viewport)
        renderThread.post { pushViewport() }
        scheduleSettingsSave()
    }

    fun resetView() {
        renderThread.post {
            NativeBridge.nativeResetView(handle)
            renderThread.requestRender()
        }
    }

    private fun pushViewport() {
        NativeBridge.nativeSetViewport(
            handle,
            viewport.mode.nativeValue,
            viewport.wireframe,
            viewport.seams,
            viewport.backfaceCull,
            viewport.unpaintedTint,
            viewport.roughness,
            viewport.metallic,
            viewport.uvCheckerDensity,
            viewport.unlitShading,
        )
        renderThread.requestRender()
    }

    // -----------------------------------------------------------------------
    // Capas
    // -----------------------------------------------------------------------
    fun addLayer() = onRenderThreadWithRefresh {
        NativeBridge.nativeAddLayer(handle, "Capa ${NativeBridge.nativeLayerCount(handle) + 1}")
    }

    fun removeLayer(index: Int) = onRenderThreadWithRefresh {
        NativeBridge.nativeRemoveLayer(handle, index)
    }

    fun duplicateLayer(index: Int) = onRenderThreadWithRefresh {
        NativeBridge.nativeDuplicateLayer(handle, index)
    }

    fun moveLayer(from: Int, to: Int) = onRenderThreadWithRefresh {
        NativeBridge.nativeMoveLayer(handle, from, to)
    }

    fun selectLayer(index: Int) = onRenderThreadWithRefresh {
        NativeBridge.nativeSetActiveLayer(handle, index)
    }

    fun clearLayer(index: Int) = onRenderThreadWithRefresh {
        NativeBridge.nativeClearLayer(handle, index)
    }

    fun fillLayer(index: Int, color: androidx.compose.ui.graphics.Color) =
        onRenderThreadWithRefresh {
            NativeBridge.nativeFillLayer(handle, index, color.red, color.green, color.blue, color.alpha)
        }

    fun updateLayer(layer: LayerUi) = onRenderThreadWithRefresh {
        NativeBridge.nativeSetLayerProps(
            handle, layer.index, layer.name, layer.opacity, layer.blend.nativeValue,
            layer.visible, layer.locked, layer.alphaLock, layer.clipToBelow,
        )
    }

    // -----------------------------------------------------------------------
    // Historial
    // -----------------------------------------------------------------------
    fun undo() = onRenderThreadWithRefresh { NativeBridge.nativeUndo(handle) }

    fun redo() = onRenderThreadWithRefresh { NativeBridge.nativeRedo(handle) }

    /**
     * Se llama al terminar cada trazo. Aprovecha para refrescar también las
     * miniaturas: es el momento en que el contenido de la capa cambió.
     */
    fun refreshHistory() {
        documentDirty = true
        renderThread.post {
            refreshLayersOnRenderThread()
            val has = NativeBridge.nativeHasBoundary(handle)
            postToUi { hasBoundary = has }
        }
    }

    // -----------------------------------------------------------------------
    // Modelo y texturas
    // -----------------------------------------------------------------------
    fun importModel(uri: Uri) {
        busy = true
        documentDirty = true
        renderThread.post {
            val (bytes, extension, name) = TextureIo.readUri(appContext, uri)
            if (bytes == null) {
                postToUi {
                    busy = false
                    statusMessage = "No se pudo leer el archivo"
                }
                return@post
            }
            val error = NativeBridge.nativeLoadModel(handle, bytes, extension, name)
            if (error != null) {
                postToUi {
                    busy = false
                    statusMessage = error
                }
                return@post
            }
            NativeBridge.nativeCreateDocument(handle, documentResolution)
            val info = readMeshInfo(name)
            refreshLayersOnRenderThread()
            val orientation = NativeBridge.nativeGetOrientation(handle)
            NativeBridge.nativeDrawFrame(handle)
            postToUi {
                busy = false
                meshInfo = info
                applyOrientationSnapshot(orientation)
                statusMessage = "Modelo cargado: ${info.triangles} triángulos"
            }
        }
    }

    fun changeDocumentResolution(resolution: Int) {
        busy = true
        scheduleSettingsSave()
        renderThread.post {
            NativeBridge.nativeCreateDocument(handle, resolution)
            refreshLayersOnRenderThread()
            NativeBridge.nativeDrawFrame(handle)
            postToUi {
                documentResolution = resolution
                busy = false
                statusMessage = "Documento a ${resolution}px (capas reiniciadas)"
            }
        }
    }

    fun exportTexture(dilate: Boolean = true) {
        busy = true
        renderThread.post {
            val pixels = NativeBridge.nativeExportComposite(handle, dilate)
            val size = NativeBridge.nativeDocumentResolution(handle)
            postToUi {
                busy = false
                statusMessage = if (pixels == null) {
                    "No hay nada que exportar"
                } else {
                    val fileName = buildString {
                        append(meshInfo.name.substringBeforeLast('.').ifBlank { "textura" })
                        append("_baseColor_")
                        append(System.currentTimeMillis() / 1000)
                        append(".png")
                    }
                    TextureIo.savePng(appContext, pixels, size, fileName)
                        ?.let { "Guardado en Descargas: $it" }
                        ?: "No se pudo guardar el PNG"
                }
            }
        }
    }

    fun importTextureToActiveLayer(uri: Uri) {
        busy = true
        documentDirty = true
        renderThread.post {
            val size = NativeBridge.nativeDocumentResolution(handle)
            val pixels = TextureIo.readImageAsRgba(appContext, uri, size)
            val ok = pixels != null &&
                NativeBridge.nativeImportLayerPixels(handle, NativeBridge.nativeGetActiveLayer(handle), pixels, size)
            NativeBridge.nativeDrawFrame(handle)
            postToUi {
                busy = false
                statusMessage = if (ok) "Imagen importada en la capa activa" else "No se pudo importar la imagen"
            }
        }
    }

    // -----------------------------------------------------------------------
    // Interno
    // -----------------------------------------------------------------------
    private fun readMeshInfo(name: String): MeshInfo {
        val stats = NativeBridge.nativeGetMeshStats(handle)
        return MeshInfo(
            vertices = stats[0],
            triangles = stats[1],
            submeshes = stats[2],
            islands = stats.getOrElse(5) { 0 },
            hasUVs = stats[3] == 1,
            uvsOutside01 = stats[4] == 1,
            loaded = stats[1] > 0,
            name = name,
        )
    }

    private inline fun onRenderThreadWithRefresh(crossinline action: () -> Unit) {
        documentDirty = true
        renderThread.post {
            action()
            refreshLayersOnRenderThread()
            renderThread.requestRender()
        }
    }

    /** Debe ejecutarse en el hilo de render: consulta el estado nativo. */
    private fun refreshLayersOnRenderThread() {
        val names = NativeBridge.nativeGetLayerNames(handle)
        val props = NativeBridge.nativeGetLayerProps(handle)
        val active = NativeBridge.nativeGetActiveLayer(handle)
        val history = NativeBridge.nativeHistoryState(handle)
        val resolution = NativeBridge.nativeDocumentResolution(handle)

        val snapshot = names.mapIndexed { index, name ->
            val base = index * 6
            LayerUi(
                index = index,
                name = name,
                opacity = props.getOrElse(base) { 1f },
                blend = BlendMode.fromNative(props.getOrElse(base + 1) { 0f }.toInt()),
                visible = props.getOrElse(base + 2) { 1f } > 0.5f,
                locked = props.getOrElse(base + 3) { 0f } > 0.5f,
                alphaLock = props.getOrElse(base + 4) { 0f } > 0.5f,
                clipToBelow = props.getOrElse(base + 5) { 0f } > 0.5f,
                thumbnail = readThumbnail(index),
            )
        }

        postToUi {
            layers.clear()
            layers.addAll(snapshot)
            activeLayer = active
            canUndo = history[0] == 1
            canRedo = history[1] == 1
            historyMemoryMb = history[2]
            documentResolution = resolution
        }
    }

    /**
     * Miniatura de una capa. Debe ejecutarse en el hilo de render: lee de la
     * GPU. Se pide reducida (96 px) para que la lectura de vuelta sea de 36 KB
     * y no de los 16 MB que ocupa la capa entera.
     */
    private fun readThumbnail(index: Int): androidx.compose.ui.graphics.ImageBitmap? {
        val pixels = NativeBridge.nativeLayerThumbnail(handle, index, THUMBNAIL_SIZE)
            ?: return null
        return runCatching {
            val bitmap = android.graphics.Bitmap.createBitmap(
                THUMBNAIL_SIZE, THUMBNAIL_SIZE, android.graphics.Bitmap.Config.ARGB_8888,
            )
            bitmap.copyPixelsFromBuffer(java.nio.ByteBuffer.wrap(pixels))
            bitmap.asImageBitmap()
        }.getOrNull()
    }

    private fun postToUi(block: () -> Unit) {
        uiHandler.post(block)
    }

    init {
        restoreSettings()
        restoreColors(KEY_RECENTS, recentColors)
        restoreColors(KEY_FAVORITES, favoriteColors)
        refreshProjects()
        scheduleAutoSave()
    }

    private companion object {
        val uiHandler = android.os.Handler(android.os.Looper.getMainLooper())
        const val KEY_RECENTS = "recent_colors"
        const val KEY_FAVORITES = "favorite_colors"
        const val KEY_SETTINGS = "ui_settings"
        const val THUMBNAIL_SIZE = 96
        const val EXT = ".uvp"
    }
}
