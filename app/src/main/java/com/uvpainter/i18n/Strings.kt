package com.uvpainter.i18n

import androidx.compose.runtime.staticCompositionLocalOf
import java.util.Locale

/**
 * Idiomas de la interfaz.
 *
 * La app nacio en castellano y se abre al estandar EFIGS: ingles, frances,
 * italiano, aleman y el castellano de siempre. Cada uno lleva su nombre escrito
 * como lo escribe quien lo habla, que es lo unico que entiende alguien que abre
 * la app en un idioma que no es el suyo y busca el propio en la lista.
 */
enum class AppLanguage(val code: String, val nativeName: String) {
    ES("es", "Español"),
    EN("en", "English"),
    FR("fr", "Français"),
    IT("it", "Italiano"),
    DE("de", "Deutsch"),
    ;

    /** Para fechas y numeros: la hora del ultimo punto de control se lee local. */
    val locale: Locale get() = Locale.forLanguageTag(code)

    val strings: Strings
        get() = when (this) {
            ES -> stringsEs
            EN -> stringsEn
            FR -> stringsFr
            IT -> stringsIt
            DE -> stringsDe
        }

    companion object {
        fun fromCode(code: String?): AppLanguage? =
            entries.firstOrNull { it.code == code }

        /**
         * Idioma del sistema si esta entre los que hablamos; si no, castellano.
         * Nadie deberia tener que buscar el ajuste la primera vez que abre.
         */
        fun fromSystem(): AppLanguage =
            fromCode(Locale.getDefault().language) ?: ES
    }
}

/**
 * Todos los rotulos de la app, agrupados por donde se leen.
 *
 * Van en grupos y no en una sola lista plana por dos razones: un constructor de
 * doscientos argumentos roza el limite de la maquina virtual, y asi el compilador
 * canta si una traduccion se deja un texto por el camino, que es justo lo que no
 * se ve al probar la app en el idioma propio.
 *
 * Los que llevan %s o %d se rellenan con [String.format] en el sitio donde se
 * usan.
 */
data class Strings(
    val tool: ToolStrings,
    val brush: BrushStrings,
    val fill: FillStrings,
    val color: ColorStrings,
    val shape: ShapeStrings,
    val input: InputStrings,
    val layers: LayerStrings,
    val view: ViewStrings,
    val doc: DocStrings,
    val projects: ProjectStrings,
    val reference: ReferenceStrings,
    val labels: LabelStrings,
    val status: StatusStrings,
    val errors: ErrorStrings,
)

/** Barra de herramientas y carriles laterales. */
data class ToolStrings(
    val undo: String,
    val redo: String,
    val brush: String,
    val eraser: String,
    val fill: String,
    val picker: String,
    val stabilizer: String,
    val shapesAndBounds: String,
    val layers: String,
    val view: String,
    val penAndPalm: String,
    /** Botón de la barra que abre imágenes de referencia, varias de una vez. */
    val reference: String,
    val frame: String,
    val projects: String,
    val document: String,
    val fullscreen: String,
    val showUi: String,
    /** Rotulos cortos de los deslizadores verticales: no caben mas de tres letras. */
    val railSize: String,
    val railOpacity: String,
    val railStabilizer: String,
)

data class BrushStrings(
    val title: String,
    val size: String,
    val lockSize: String,
    val lockSizeHelp: String,
    val hardness: String,
    val opacity: String,
    val flow: String,
    val smoothing: String,
    val spacing: String,
    val tipTitle: String,
    val tipFlatten: String,
    val tipAngle: String,
    val tipFollowsStroke: String,
    val grain: String,
    val grainScale: String,
    val penTitle: String,
    val pressureToSize: String,
    val pressureToOpacity: String,
    val sensitivity: String,
    val sensitivityHelp: String,
    val pressureCurve: String,
    val minSize: String,
    val tiltToSize: String,
    val projectionTitle: String,
    val restrictToIsland: String,
    val restrictToIslandHelp: String,
    val noPaintHidden: String,
    val noPaintBackfaces: String,
    val alphaLock: String,
    val edgeFade: String,
)

data class FillStrings(
    val title: String,
    val closedArea: String,
    val closedAreaHelp: String,
    val islandHelp: String,
    val tolerance: String,
    val toleranceHelp: String,
    val footer: String,
)

data class ColorStrings(
    val title: String,
    val help: String,
    val favoriteOn: String,
    val favoriteOff: String,
    val favorites: String,
    val recents: String,
)

data class ShapeStrings(
    val title: String,
    val help: String,
    val polygonSides: String,
    val fromCenter: String,
    val boundsTitle: String,
    val boundsHelp: String,
    val drawBoundary: String,
    val clearBoundary: String,
    val respectBounds: String,
    val boundsActive: String,
    val boundsNone: String,
    val boundsThickness: String,
)

data class InputStrings(
    val title: String,
    val fingersToNavigate: String,
    val oneFinger: String,
    val twoFingers: String,
    val twoFingersHelp: String,
    val rejectWide: String,
    val palmThreshold: String,
    val palmThresholdHelp: String,
    val stylusOnly: String,
    val fingerPaints: String,
    val twistRotates: String,
    val orbitSensitivity: String,
    val cameraTitle: String,
    val zoomToPinch: String,
    val zoomToPinchHelp: String,
    val twoFingerPan: String,
    val twoFingerPanHelp: String,
    val penButtonTitle: String,
    val penButtonHelp: String,
    val gesturesTitle: String,
    val gesturesHelp: String,
)

data class LayerStrings(
    val title: String,
    /** Como se llama la capa de abajo del todo, la que nace con el documento. */
    val background: String,
    /** Las demas, por orden de creacion. %d el numero. */
    val numbered: String,
    /** Coletilla de la capa duplicada, que se pega al nombre de la original. */
    val copySuffix: String,
    val add: String,
    val duplicate: String,
    val delete: String,
    val help: String,
    val visibility: String,
    val moveUp: String,
    val moveDown: String,
    val opacity: String,
    val alphaShort: String,
    val clipShort: String,
    val lockShort: String,
    val clearShort: String,
)

data class ViewStrings(
    val title: String,
    val seams: String,
    val wireframe: String,
    val hideBackfaces: String,
    val markUnpainted: String,
    val unlitShading: String,
    val unlitShadingHelp: String,
    val roughness: String,
    val metallic: String,
    val checkerDensity: String,
    val frameModel: String,
    val showFps: String,
    val showFpsHelp: String,
)

data class DocStrings(
    val title: String,
    /** %1$s nombre, %2$d triangulos, %3$d vertices, %4$d islas UV. */
    val meshSummary: String,
    /** Se pega al resumen de arriba. %d materiales. */
    val meshMaterials: String,
    val unnamedModel: String,
    val noModel: String,
    val uvWarning: String,
    val orientationTitle: String,
    val yUp: String,
    val zUp: String,
    val flip: String,
    val rotate90: String,
    val atlasTitle: String,
    val atlasHelp: String,
    val importModel: String,
    val importImage: String,
    val exportPng: String,
    val toolbarTitle: String,
    val toolbarHelp: String,
    val toolbarReset: String,
    /** %d megabytes ocupados por el historial. */
    val history: String,
    val languageTitle: String,
    val languageHelp: String,
)

data class ProjectStrings(
    val title: String,
    val help: String,
    val nameLabel: String,
    val save: String,
    val newProject: String,
    val refresh: String,
    val autoSave: String,
    /** %s nombre del proyecto abierto. */
    val autoSaveOpen: String,
    val autoSaveNone: String,
    /** %d minutos. */
    val minutes: String,
    /** %s hora del ultimo punto de control. */
    val lastCheckpoint: String,
    val noCheckpoints: String,
    val empty: String,
    val open: String,
    val deleteProject: String,
    /** %s nombre del proyecto abierto ahora mismo. */
    val openSuffix: String,
    /** %s carpeta. */
    val savedIn: String,
)

data class ReferenceStrings(
    val title: String,
    val move: String,
    val picker: String,
    val viewOnly: String,
    val close: String,
    val imageDescription: String,
)

/** Rotulos de las enumeraciones: modos, puntas, figuras y preajustes. */
data class LabelStrings(
    val viewUnlit: String,
    val viewPbr: String,
    val viewMatcap: String,
    val viewUvChecker: String,

    val blendNormal: String,
    val blendMultiply: String,
    val blendScreen: String,
    val blendOverlay: String,
    val blendAdd: String,
    val blendDodge: String,
    val blendBurn: String,
    val blendSoftLight: String,

    val tipRound: String,
    val tipFlat: String,
    val tipSquare: String,
    val tipSpray: String,

    val shapeFree: String,
    val shapeLine: String,
    val shapeRectangle: String,
    val shapeEllipse: String,
    val shapePolygon: String,

    val penNone: String,
    val penEraseHeld: String,
    val penPickHeld: String,
    val penToggleEraser: String,
    val penUndo: String,
    val penRedo: String,
    val penToggleStabilizer: String,
    val penResetView: String,
    val penToggleUi: String,

    val presetInk: String,
    val presetFlat: String,
    val presetSoftShadow: String,
    val presetAirbrush: String,
    val presetFineDetail: String,
    val presetRuler: String,
    val presetCharcoal: String,
    val presetDryTexture: String,
)

/** Avisos de la barra inferior. */
data class StatusStrings(
    val palmRejected: String,
    /** %s error del motor. */
    val sessionRestoreFailed: String,
    val sessionRestored: String,
    val documentFailed: String,
    val nameYourProject: String,
    /** %s nombre. */
    val projectSaved: String,
    /** %s nombre. */
    val projectOpened: String,
    val projectDeleted: String,
    /** %s error del motor. */
    val autoSaveFailed: String,
    val newProject: String,
    val boundsCleared: String,
    /** %d triangulos. */
    val modelLoaded: String,
    /** %d pixeles de lado. */
    val documentResized: String,
    val nothingToExport: String,
    /** %s nombre del archivo. */
    val savedToDownloads: String,
    val pngFailed: String,
    val imageImported: String,
    val imageImportFailed: String,
    /** Nombre por defecto del PNG exportado si el modelo no tiene nombre. */
    val textureFileName: String,
    val referenceOpenFailed: String,
)

/**
 * Lo que responde el motor nativo cuando algo no sale.
 *
 * El C++ ya no devuelve la frase hecha sino una clave (ver [translateEngineError]):
 * un motor que habla castellano deja media app sin traducir en cuanto el archivo
 * que abres esta roto.
 */
data class ErrorStrings(
    val engineNotReady: String,
    val fileUnreadable: String,
    val gltfBadJson: String,
    val noTriangleMesh: String,
    val glbTruncated: String,
    val notGlb: String,
    val gltf2Only: String,
    val glbNoJsonChunk: String,
    val objNoFaces: String,
    val fileTooShort: String,
    /** %s extension encontrada. */
    val unknownFormat: String,
    val nothingToSave: String,
    val layersUnreadable: String,
    /** %s ruta. */
    val writeFailed: String,
    val projectWriteFailed: String,
    val projectCloseFailed: String,
    val projectNotFound: String,
    val projectInvalid: String,
    /** Por si el motor devuelve algo que aqui no conocemos. %s clave cruda. */
    val unknown: String,
)

/**
 * Nombres de capa de fabrica que el motor ha ido dejando por ahi.
 *
 * El nombre vive dentro del documento, asi que una sesion abierta en castellano
 * guarda «Fondo» y «Capa 2» y los sigue guardando aunque despues se cambie de
 * idioma: son datos, no rotulos. Se traducen al pintarlos, reconociendo los de
 * fabrica en cualquiera de los cinco idiomas. Un nombre que no sea de fabrica
 * se muestra tal cual, que es lo que hay que hacer el dia que se puedan
 * renombrar las capas a mano.
 */
private val backgroundLayerNames =
    setOf("Fondo", "Background", "Arrière-plan", "Sfondo", "Hintergrund")

private val numberedLayerNames =
    setOf("Capa", "Layer", "Calque", "Livello", "Ebene")

/** Lo que el motor le pega al nombre al duplicar una capa. */
private val copyLayerWords =
    setOf("copia", "copy", "copie", "kopie")

fun translateLayerName(raw: String, strings: Strings): String {
    // Una capa duplicada dos veces arrastra la coletilla dos veces, y la de
    // dentro puede ser de otro idioma: se quitan todas antes de mirar el nombre.
    var base = raw.trim()
    var copies = 0
    while (base.contains(' ') && base.substringAfterLast(' ').lowercase() in copyLayerWords) {
        base = base.substringBeforeLast(' ').trim()
        copies++
    }

    val number = base.substringAfterLast(' ', "").toIntOrNull()
    val translated = when {
        base in backgroundLayerNames -> strings.layers.background
        number != null && base.substringBeforeLast(' ') in numberedLayerNames ->
            strings.layers.numbered.format(number)
        else -> base
    }

    return if (copies == 0) {
        translated
    } else {
        translated + " ${strings.layers.copySuffix}".repeat(copies)
    }
}

/**
 * Traduce lo que sube del motor.
 *
 * Llega como `clave` o `clave|dato` (la ruta que no se pudo escribir, la
 * extension que no conocemos). Si la clave no esta en la tabla se muestra tal
 * cual: mas vale un mensaje raro que un mensaje vacio.
 */
fun translateEngineError(raw: String?, strings: Strings): String? {
    if (raw == null) return null
    val key = raw.substringBefore('|')
    val arg = raw.substringAfter('|', "")
    val e = strings.errors
    return when (key) {
        "err.engine_not_ready" -> e.engineNotReady
        "err.file_unreadable" -> e.fileUnreadable
        "err.gltf_bad_json" -> e.gltfBadJson
        "err.no_triangle_mesh" -> e.noTriangleMesh
        "err.glb_truncated" -> e.glbTruncated
        "err.not_glb" -> e.notGlb
        "err.gltf2_only" -> e.gltf2Only
        "err.glb_no_json_chunk" -> e.glbNoJsonChunk
        "err.obj_no_faces" -> e.objNoFaces
        "err.file_too_short" -> e.fileTooShort
        "err.unknown_format" -> e.unknownFormat.format(arg)
        "err.nothing_to_save" -> e.nothingToSave
        "err.layers_unreadable" -> e.layersUnreadable
        "err.write_failed" -> e.writeFailed.format(arg)
        "err.project_write_failed" -> e.projectWriteFailed
        "err.project_close_failed" -> e.projectCloseFailed
        "err.project_not_found" -> e.projectNotFound
        "err.project_invalid" -> e.projectInvalid
        else -> if (key.startsWith("err.")) e.unknown.format(key) else raw
    }
}

/**
 * Idioma vigente para quien dibuja pantalla. Se sirve desde la raiz de la
 * interfaz, asi que cambiarlo repinta la app entera sin reiniciar nada.
 */
val LocalStrings = staticCompositionLocalOf { stringsEs }

/**
 * El idioma en si, para lo que no es un rotulo: fechas y horas, que se leen
 * distintas segun el pais aunque la frase que las envuelve ya este traducida.
 */
val LocalAppLanguage = staticCompositionLocalOf { AppLanguage.ES }
